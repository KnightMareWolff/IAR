// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Recording/IARAudioCaptureSession.h"
#include "../IAR.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Engine/World.h"

UIARAudioCaptureSession::UIARAudioCaptureSession()
    : CurrentTakeEncoder(nullptr)
    , bIsOverallRecordingActive(false)
    , AudioSourceRef(nullptr)
    , CurrentTakeNumber(0)
    , FramePool(nullptr)
    , bIsSessionInitialized(false)
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Construtor chamado."));
}

UIARAudioCaptureSession::~UIARAudioCaptureSession()
{
    ShutdownSession();
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Destrutor chamado."));
}

void UIARAudioCaptureSession::InitializeSession(UIARAudioSource* InAudioSource, const FIAR_RecordingSettings& InRecordingSettings, UIARFramePool* InFramePool)
{
    if (bIsSessionInitialized) { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Sessão já inicializada. Ignorando chamada.")); return; }
    if (!InAudioSource || !InFramePool) { UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Falha na inicialização. InAudioSource ou InFramePool é nulo.")); return; }
    
    AudioSourceRef = InAudioSource; 
    RecordingSettings = InRecordingSettings;
    SessionID = FGuid::NewGuid().ToString();
    FramePool = InFramePool;

    bIsSessionInitialized = true;
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Sessão '%s' inicializada com sucesso."), *SessionID);
}

void UIARAudioCaptureSession::StartOverallRecording(const FIAR_AudioStreamSettings& StreamSettings, const FString& CustomRecordingName)
{
    if (!bIsSessionInitialized) { UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Não é possível iniciar gravação global. Sessão não inicializada.")); return; }
    if (!FramePool || !IsValid(FramePool)) { UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: FramePool da sessão inválido. Não é possível iniciar gravação global.")); return; }
    if (bIsOverallRecordingActive) { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Gravação global já está ativa. Ignorando StartOverallRecording.")); return; }

    CurrentSessionStreamSettings = StreamSettings;
    CurrentOverallSessionName = CustomRecordingName.IsEmpty() ? FString::Printf(TEXT("Session_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))) : CustomRecordingName;
    CompletedTakeFilePaths.Empty();
    CurrentTakeNumber = RecordingSettings.InitialTakeNumber;

    bIsOverallRecordingActive = true;
    StartNewTakeInternal();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TakeRotationTimerHandle, this, &UIARAudioCaptureSession::RotateCurrentTake, RecordingSettings.TakeDurationSeconds, true);
        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Timer de rotação de takes iniciado (%.2f segundos)."), RecordingSettings.TakeDurationSeconds);
    }

    if (AudioSourceRef && !AudioSourceRef->IsCapturing())
    {
        AudioSourceRef->StartCapture(); // Inicia a captura da fonte (que alimentará o encoder)
        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Captura da fonte de áudio iniciada."));
    }

    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Sessão de gravação global '%s' iniciada."), *CurrentOverallSessionName);
    OnFileRecordingStarted.Broadcast(SessionID); // Broadcast com o delegate renomeado
}

void UIARAudioCaptureSession::StopOverallRecording()
{
    if (!bIsOverallRecordingActive) { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Nenhuma gravação global ativa para parar.")); return; }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TakeRotationTimerHandle);
        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Timer de rotação de takes parado."));
    }

    if (AudioSourceRef && AudioSourceRef->IsCapturing())
    {
        AudioSourceRef->StopCapture(); // Para a captura da fonte
        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Captura da fonte de áudio parada."));
    }

    StopCurrentTakeInternal();
    bIsOverallRecordingActive = false;

    FString MasterFilePath = GenerateOutputFilePath(RecordingSettings.MasterRecordingPrefix, CurrentOverallSessionName, -1);
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Iniciando concatenação para o arquivo master: %s"), *MasterFilePath);
    
    OnFileMasterRecordingStarted.Broadcast(MasterFilePath); // Broadcast com o delegate renomeado

    AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, MasterFilePath, CompletedTakeFilePathsCopy = this->CompletedTakeFilePaths]() mutable {
        bool bSuccess = ConcatenateTakesToMaster(MasterFilePath, CompletedTakeFilePathsCopy);
        AsyncTask(ENamedThreads::GameThread, [this, bSuccess, MasterFilePath, CompletedTakeFilePathsCopy]() {
            if (bSuccess)
            {
                UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Concatenou takes em arquivo master com sucesso: %s"), *MasterFilePath);
                DeleteTakeFiles(CompletedTakeFilePathsCopy);
                CompletedTakeFilePaths.Empty();
                OnFileMasterRecordingStopped.Broadcast(MasterFilePath); // Broadcast com o delegate renomeado
                OnFileRecordingStopped.Broadcast(MasterFilePath); // Notifica o fim da gravação geral em arquivo
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Falha ao concatenar takes para o arquivo master. Takes temporários não deletados."));
                OnFileRecordingStopped.Broadcast(TEXT("")); // Indica falha
            }
        });
    });
}

bool UIARAudioCaptureSession::IsAnyRecordingActive() const
{
    return bIsOverallRecordingActive;
}

void UIARAudioCaptureSession::OnAudioFrameReceived(TSharedPtr<FIAR_AudioFrameData> AudioFrame)
{
    if (CurrentTakeEncoder && CurrentTakeEncoder->IsEncodingActive())
    {
        CurrentTakeEncoder->EncodeFrame(AudioFrame);
    }
}

void UIARAudioCaptureSession::ShutdownSession()
{
    if (bIsOverallRecordingActive)
    {
        StopOverallRecording();
    }
    
    if (CurrentTakeEncoder)
    {
        CurrentTakeEncoder->ShutdownEncoder();
        CurrentTakeEncoder = nullptr;
    }

    bIsSessionInitialized = false;

    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Sessão '%s' desligada."), *SessionID);
}

void UIARAudioCaptureSession::StartNewTakeInternal()
{
    StopCurrentTakeInternal();

    FString TakeFileName = FString::Printf(TEXT("%s_%s"), *RecordingSettings.TakeRecordingPrefix, *CurrentOverallSessionName);
    FString OutputFilePath = GenerateOutputFilePath(TakeFileName, TEXT(""), CurrentTakeNumber);

    UIARAudioEncoder* NewTakeEncoder = NewObject<UIARAudioEncoder>(this);
    if (NewTakeEncoder)
    {
        NewTakeEncoder->OnAudioEncodingError.AddDynamic(this, &UIARAudioCaptureSession::OnEncoderErrorReceived);
        NewTakeEncoder->OnAudioPipeCongested.AddDynamic(this, &UIARAudioCaptureSession::OnPipeCongested);
        NewTakeEncoder->OnAudioPipeCleared.AddDynamic(this, &UIARAudioCaptureSession::OnPipeCleared);

        if (NewTakeEncoder->Initialize(CurrentSessionStreamSettings, TEXT(""), 0, 0, FramePool))
        {
            if (NewTakeEncoder->LaunchEncoder(OutputFilePath))
            {
                CurrentTakeEncoder = NewTakeEncoder;
                UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Novo take %d iniciado em '%s'."), CurrentTakeNumber, *CurrentTakeEncoder->GetCurrentOutputFilePath());
                OnFileRecordingTakeStarted.Broadcast(CurrentTakeNumber, CurrentTakeEncoder->GetCurrentOutputFilePath()); // Broadcast do delegate renomeado
                CurrentTakeNumber++;
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Falha ao lançar encoder para o take %d."), CurrentTakeNumber);
                NewTakeEncoder->ShutdownEncoder();
            }
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Falha ao inicializar encoder para o take %d."), CurrentTakeNumber);
        }
    }
}

bool UIARAudioCaptureSession::StopCurrentTakeInternal()
{
    if (CurrentTakeEncoder && CurrentTakeEncoder->IsEncodingActive())
    {
        CurrentTakeEncoder->FinishEncoding();
        FString CompletedTakePath = CurrentTakeEncoder->GetCurrentOutputFilePath();
        CompletedTakeFilePaths.Add(CompletedTakePath);

        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Take %d parado. Arquivo: '%s'"), CurrentTakeNumber -1, *CompletedTakePath);
        OnFileRecordingTakeStopped.Broadcast(CurrentTakeNumber - 1, CompletedTakePath); // Broadcast do delegate renomeado
        
        CurrentTakeEncoder->OnAudioEncodingError.RemoveDynamic(this, &UIARAudioCaptureSession::OnEncoderErrorReceived);
        CurrentTakeEncoder->OnAudioPipeCongested.RemoveDynamic(this, &UIARAudioCaptureSession::OnPipeCongested);
        CurrentTakeEncoder->OnAudioPipeCleared.RemoveDynamic(this, &UIARAudioCaptureSession::OnPipeCleared);
        CurrentTakeEncoder->ShutdownEncoder();
        CurrentTakeEncoder = nullptr;
        return true;
    }
    else if (CurrentTakeEncoder)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Tentativa de parar take inativo ou inválido, limpando referência."));
        CurrentTakeEncoder->ShutdownEncoder();
        CurrentTakeEncoder = nullptr;
    }
    return false;
}

void UIARAudioCaptureSession::RotateCurrentTake()
{
    if (bIsOverallRecordingActive)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Rotacionando take..."));
        StartNewTakeInternal();
    }
}

bool UIARAudioCaptureSession::ConcatenateTakesToMaster(const FString& MasterFilePath, const TArray<FString>& TakeFilePaths)
{
    if (TakeFilePaths.Num() == 0) { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Nenhuns takes para concatenar. Não foi criado arquivo master.")); return false; }

    FString TempDirPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("IAR_Temp"));
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*TempDirPath);

    FString TempListFilePath = FPaths::Combine(TempDirPath, TEXT("concat_list.txt"));
    FPaths::NormalizeFilename(TempListFilePath);

    FString ConcatListContent;
    for (const FString& TakePath : TakeFilePaths)
    {
        FString NormalizedTakePath = FPaths::ConvertRelativePathToFull(TakePath);
        NormalizedTakePath.ReplaceInline(TEXT(""), TEXT("/"), ESearchCase::CaseSensitive);
        ConcatListContent += FString::Printf(TEXT("file '%s'\n"), *NormalizedTakePath);
    }

    if (!FFileHelper::SaveStringToFile(ConcatListContent, *TempListFilePath))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Falha ao criar arquivo de lista de concatenação: %s"), *TempListFilePath);
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectory(*TempDirPath);
        return false;
    }
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Arquivo de lista de concatenação criado: %s"), *TempListFilePath);

    FString FFmpegExecPath = UIARAudioEncoder::GetFFmpegExecutablePathInternal();
    if (FFmpegExecPath.IsEmpty() || !FPlatformFileManager::Get().GetPlatformFile().FileExists(*FFmpegExecPath))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: FFmpeg executável não encontrado para concatenação: %s"), *FFmpegExecPath);
        FPlatformFileManager::Get().GetPlatformFile().DeleteDirectory(*TempDirPath);
        return false;
    }

    FString MasterOutputDir = FPaths::GetPath(MasterFilePath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*MasterOutputDir);

    FString NormalizedMasterFilePath = FPaths::ConvertRelativePathToFull(MasterFilePath);
    NormalizedMasterFilePath.ReplaceInline(TEXT(""), TEXT("/"), ESearchCase::CaseSensitive);
    FString ConcatArguments = FString::Printf(TEXT("-f concat -safe 0 -i %s -c copy %s"), *TempListFilePath, *NormalizedMasterFilePath);
    
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Executando FFmpeg para concatenação. Exec: %s Args: %s"), *FFmpegExecPath, *ConcatArguments);

    bool bSuccess = UIARAudioEncoder::LaunchBlockingFFmpegProcess(FFmpegExecPath, ConcatArguments);

    FPlatformFileManager::Get().GetPlatformFile().DeleteDirectory(*TempDirPath);

    if (!bSuccess)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: FFmpeg falhou ao concatenar arquivos de take para o master."));
    }
    return bSuccess;
}

void UIARAudioCaptureSession::DeleteTakeFiles(const TArray<FString>& FilePathsToDelete)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    for (const FString& FilePath : FilePathsToDelete)
    {
        if (PlatformFile.FileExists(*FilePath))
        {
            if (PlatformFile.DeleteFile(*FilePath)) { UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Arquivo de take deletado: %s"), *FilePath); }
            else { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Falha ao deletar arquivo de take: %s"), *FilePath); }
        }
        else { UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Tentativa de deletar arquivo de take que não existe: %s"), *FilePath); }
    }
}

FString UIARAudioCaptureSession::GenerateOutputFilePath(const FString& Prefix, const FString& Suffix, int32 Index) const
{
    FString FileName = Prefix;
    if (Index != -1) { FileName += FString::Printf(TEXT("_%03d"), Index); }
    if (!Suffix.IsEmpty()) { FileName += TEXT("_") + Suffix; }
    if (RecordingSettings.bAppendTimestamp) { FileName += TEXT("_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")); }
    FileName += TEXT(".wav"); 

    FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), RecordingSettings.BaseOutputFolder);
    FPaths::NormalizeDirectoryName(OutputDirectory);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*OutputDirectory);

    return FPaths::Combine(OutputDirectory, FileName);
}

UFUNCTION()
void UIARAudioCaptureSession::OnEncoderErrorReceived(const FString& ErrorMessage)
{
    UE_LOG(LogIAR, Error, TEXT("UIARAudioCaptureSession: Erro recebido do encoder: %s. Parando a captura da fonte de áudio."), *ErrorMessage);
    if (AudioSourceRef && AudioSourceRef->IsCapturing()) { AudioSourceRef->StopCapture(); }
}

UFUNCTION()
void UIARAudioCaptureSession::OnPipeCongested(const FString& OutputFilePath)
{
    UE_LOG(LogIAR, Warning, TEXT("UIARAudioCaptureSession: Pipe de encoder '%s' congestionado. Pausando fonte de áudio."), *OutputFilePath);
    if (AudioSourceRef && AudioSourceRef->IsCapturing()) { AudioSourceRef->StopCapture(); }
}

UFUNCTION()
void UIARAudioCaptureSession::OnPipeCleared(const FString& OutputFilePath)
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioCaptureSession: Pipe de encoder '%s' liberado. Retomando fonte de áudio."), *OutputFilePath);
    if (AudioSourceRef && !AudioSourceRef->IsCapturing()) { AudioSourceRef->StartCapture(); }
}
