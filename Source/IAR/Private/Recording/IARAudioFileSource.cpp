// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Recording/IARAudioFileSource.h"
#include "../IAR.h"
#include "Kismet/GameplayStatics.h" // Para GetTimeSeconds
#include "Misc/Paths.h" // Para FPaths
#include "Recording/IARAudioEncoder.h" // ADICIONADO: Para a nova função de decodificação
#include "HAL/PlatformFileManager.h" // ADICIONADO: Para verificar existência de arquivo
#include "../GlobalStatics.h" // ADICIONADO: Para obter o caminho raiz de gravações
#include "Async/Async.h" // Para AsyncTask (se fosse necessário para o LoadFileAsync)
#include "Engine/World.h"

UIARAudioFileSource::UIARAudioFileSource()
    : UIARAudioSource()
    // , LoadedSoundWave(nullptr) // Removido
    , CurrentSampleIndex(0)
    , NumSamplesPerFrame(0)
    , bIsFileLoaded(false)
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Construtor chamado."));
}

UIARAudioFileSource::~UIARAudioFileSource()
{
    Shutdown();
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Destrutor chamado."));
}

void UIARAudioFileSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    // Primeiro, chame a inicialização da classe base
    Super::Initialize(StreamSettings, InFramePool);

    if (!FramePool || !IsValid(FramePool))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: FramePool inválido. Não é possível inicializar a fonte."));
        return;
    }

    if (CurrentStreamSettings.FilePath.IsEmpty())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: FilePath vazio. Não é possível carregar arquivo de áudio."));
        return;
    }

    // Constrói o caminho completo do arquivo no disco, usando a raiz padrão
    FullDiskFilePathInternal = FPaths::Combine(UIARGlobalStatics::GetIARRecordingRootPath(), CurrentStreamSettings.FilePath);
    FPaths::NormalizeFilename(FullDiskFilePathInternal); // Normaliza para garantir o formato correto para o OS

    // O carregamento real do arquivo será feito de forma assíncrona ou no StartCapture, não no Initialize
    bIsFileLoaded = false;
    RawAudioSamples.Empty(); // Garante que esteja limpo
    CurrentSampleIndex = 0;

    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Inicializado para carregar arquivo '%s' (PATH: %s)."), *CurrentStreamSettings.FilePath, *FullDiskFilePathInternal);
}

void UIARAudioFileSource::StartCapture()
{
    // --- INÍCIO DA ALTERAÇÃO ESPECÍFICA DO PROBLEMA LATENTE ---
    // A lógica de carregamento bloqueante foi movida para Internal_LoadAudioFileBlocking
    // e será chamada da FLoadAudioFileLatentAction.
    // Aqui, apenas checamos se já está carregado, se não estiver, isso indica um erro de fluxo.
    if (!bIsFileLoaded)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: StartCapture chamado antes do arquivo de áudio ser carregado. Verifique o fluxo da StartRecording do IARAudioComponent."));
        return;
    }
    // --- FIM DA ALTERAÇÃO ESPECÍFICA DO PROBLEMA LATENTE ---

    if (bIsCapturing)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioFileSource: Captura já está ativa."));
        return;
    }

    if (RawAudioSamples.Num() == 0 || !GetWorld())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: Dados de audio vazios ou World inválido. Não é possível iniciar a captura."));
        return;
    }

    Super::StartCapture();

    // Inicia o timer para processar o arquivo em chunks
    GetWorld()->GetTimerManager().SetTimer(
        AudioFileProcessTimerHandle,
        this,
        &UIARAudioFileSource::ProcessFileFrame,
        0.02f, // Intervalo de 20ms para processar frames
        true // Repetir
    );
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Captura de áudio de arquivo iniciada."));
}

void UIARAudioFileSource::StopCapture()
{
    if (!bIsCapturing)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioFileSource: Captura já está inativa."));
        return;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AudioFileProcessTimerHandle);
    }

    Super::StopCapture();
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Captura de audio de arquivo parada."));
}

void UIARAudioFileSource::Shutdown()
{
    StopCapture();
    ResetFileSource(); // Limpa os dados carregados
    Super::Shutdown();
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Desligado e recursos liberados."));
}

void UIARAudioFileSource::ResetFileSource()
{
    RawAudioSamples.Empty();
    CurrentSampleIndex = 0;
    bIsFileLoaded = false;
    // LoadedSoundWave = nullptr; // Removido
    FullDiskFilePathInternal = TEXT("");
    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Fonte de arquivo resetada."));
}

// NOVO: Implementação da função bloqueante para carregamento do arquivo
bool UIARAudioFileSource::Internal_LoadAudioFileBlocking()
{
    if (bIsFileLoaded)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Arquivo já carregado. Pulando carregamento."));
        return true;
    }

    if (FullDiskFilePathInternal.IsEmpty())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: Caminho do arquivo interno vazio. Não é possível carregar."));
        return false;
    }

    // Verifica se o arquivo existe antes de tentar decodificar
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullDiskFilePathInternal))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: Arquivo de áudio não encontrado no caminho: %s. Verifique o FilePath relativo em suas StreamSettings."), *FullDiskFilePathInternal);
        return false;
    }

    int32 ActualSampleRate = 0;
    int32 ActualNumChannels = 0;
    // *** NOVA CHAMADA COM DR_WAV ***
    if (!UIARAudioEncoder::DecodeWaveFileToRawPCM_DrWav(FullDiskFilePathInternal, RawAudioSamples, ActualSampleRate, ActualNumChannels))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: Falha ao decodificar o arquivo de audio '%s' usando DrWav."), *FullDiskFilePathInternal);
        bIsFileLoaded = false;
        return false;
    }

    // Opcional: Validar se as configurações do stream correspondem ao arquivo
    // Se o arquivo WAV tiver uma taxa de amostragem ou número de canais diferente do esperado,
    // atualize as configurações do stream com as do arquivo.
    if (CurrentStreamSettings.SampleRate != ActualSampleRate || CurrentStreamSettings.NumChannels != ActualNumChannels)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioFileSource: Configurações do stream (%d SR, %d Ch) não correspondem ao arquivo WAV (%d SR, %d Ch). Usando as do arquivo."),
            CurrentStreamSettings.SampleRate, CurrentStreamSettings.NumChannels, ActualSampleRate, ActualNumChannels);
        CurrentStreamSettings.SampleRate = ActualSampleRate;
        CurrentStreamSettings.NumChannels = ActualNumChannels;
    }
    
    // Define o número de samples por frame a serem lidos por iteração.
    // Garanta que seja baseado nas configurações ATUALIZADAS.
    NumSamplesPerFrame = FMath::RoundToInt(CurrentStreamSettings.SampleRate * CurrentStreamSettings.NumChannels * 0.02f); 
    if (NumSamplesPerFrame <= 0) NumSamplesPerFrame = 4096; // Garante um valor mínimo

    CurrentSampleIndex = 0; // Começa do início do arquivo
    bIsFileLoaded = true;

    UE_LOG(LogIAR, Log, TEXT("UIARAudioFileSource: Arquivo de áudio '%s' carregado do disco com sucesso usando DrWav. SR: %d, Ch: %d, Samples Total: %d."),
        *FullDiskFilePathInternal, CurrentStreamSettings.SampleRate, CurrentStreamSettings.NumChannels, RawAudioSamples.Num());
    return true;
}


void UIARAudioFileSource::ProcessFileFrame()
{
    // NOVO: Adicionado 'this->' para garantir que os membros da classe sejam acessados corretamente
    if (!this->bIsCapturing || this->RawAudioSamples.Num() == 0 || !this->FramePool || !this->FramePool->IsValidLowLevelFast())
    {
        this->StopCapture(); // Para a captura se não houver mais dados ou FramePool for inválido
        return;
    }

    TSharedPtr<FIAR_AudioFrameData> AudioFrame = this->FramePool->AcquireFrame();
    if (!AudioFrame.IsValid())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioFileSource: Falha ao adquirir frame do pool."));
        return;
    }

    TArray<float>& FrameSamples = *(AudioFrame->RawSamplesPtr);
    FrameSamples.SetNum(this->NumSamplesPerFrame);

    int32 SamplesToRead = this->NumSamplesPerFrame;
    bool bLooping = this->CurrentStreamSettings.bLoopPlayback; // Assume a propriedade bLoopPlayback das settings
    // Copia os samples do arquivo para o frame
    for (int32 i = 0; i < SamplesToRead; ++i)
    {
        int32 SourceIndex = (this->CurrentSampleIndex + i);
        if (SourceIndex >= this->RawAudioSamples.Num())
        {
            if (bLooping)
            {
                // Loop: reseta o índice e continua lendo
                this->CurrentSampleIndex = 0;
                SourceIndex = 0;
            }
            else
            {
                // Não há mais dados e não está em loop, preenche com zero e para
                FrameSamples[i] = 0.0f;
                SamplesToRead = i; // Ajusta o tamanho do frame para o que foi realmente lido
                break;
            }
        }
        FrameSamples[i] = this->RawAudioSamples[SourceIndex];
    }
    
    // Ajusta o tamanho do frame para o que foi realmente preenchido
    FrameSamples.SetNum(SamplesToRead);

    this->CurrentSampleIndex += SamplesToRead; // Avança o índice para a próxima leitura

    // Se o índice atingiu o final do arquivo e não está em loop, para a captura
    if (this->CurrentSampleIndex >= this->RawAudioSamples.Num() && !bLooping)
    {
        this->StopCapture();
    }

    AudioFrame->SampleRate = this->CurrentStreamSettings.SampleRate;
    AudioFrame->NumChannels = this->CurrentStreamSettings.NumChannels;
    AudioFrame->Timestamp = UGameplayStatics::GetTimeSeconds(this);
    AudioFrame->CurrentStreamSettings = this->CurrentStreamSettings;

    this->OnAudioFrameAcquired.Broadcast(AudioFrame);
}
