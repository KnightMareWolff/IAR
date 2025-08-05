// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h"
#include "Core/IAR_Types.h"
#include "Recording/IARAudioSource.h"
#include "Recording/IARAudioEncoder.h"
#include "Core/IARFramePool.h"
#include "TimerManager.h"

#include "IARAudioCaptureSession.generated.h"

// --- DELEGATES DE GRAVAÇÃO EM ARQUIVO (PADRÃO IVR, USADO INTERNAMENTE PELA SESSÃO) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFileRecordingStarted, const FString&, SessionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFileRecordingStopped, const FString&, MasterFilePath);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIARFileRecordingTakeStarted, int32 , TakeNumber, const FString&, FilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIARFileRecordingTakeStopped, int32 , TakeNumber, const FString&, FilePath);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFileMasterRecordingStarted, const FString&, FilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFileMasterRecordingStopped, const FString&, FilePath);

/**
 * @brief Gerencia as sessões de gravação de áudio, incluindo gravações master e takes.
 * Orquestra múltiplos UIARAudioEncoder para diferentes saídas, e gerencia a rotação automática de takes.
 */
UCLASS()
class IAR_API UIARAudioCaptureSession : public UObject
{
    GENERATED_BODY()

public:
    UIARAudioCaptureSession();
    virtual ~UIARAudioCaptureSession();

    /**
     * @brief Inicializa a sessão de captura. Deve ser chamada uma vez antes de iniciar qualquer gravação.
     * @param InAudioSource A fonte de áudio de onde os frames serão recebidos.
     * @param InRecordingSettings As configurações globais de gravação (caminhos, prefixes).
     * @param InFramePool O FramePool compartilhado para esta sessão.
     */
    void InitializeSession(UIARAudioSource* InAudioSource, const FIAR_RecordingSettings& InRecordingSettings, UIARFramePool* InFramePool);

    /**
     * @brief Inicia a sessão de gravação global, começando a rotação automática de takes.
     * O áudio será gravado em takes segmentados e, ao parar, concatenado em um arquivo Master.
     * @param StreamSettings As configurações do stream para os takes.
     * @param CustomRecordingName Opcional. Um nome customizado para a sessão global.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Capture Session")
    void StartOverallRecording(const FIAR_AudioStreamSettings& StreamSettings, const FString& CustomRecordingName = TEXT(""));

    /**
     * @brief Para a sessão de gravação global, finaliza o take atual,
     * concatena todos os takes concluídos em um arquivo master, e deleta os takes intermediários.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Capture Session")
    void StopOverallRecording();

    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Capture Session")
    bool IsAnyRecordingActive() const;

    void OnAudioFrameReceived(TSharedPtr<FIAR_AudioFrameData> AudioFrame);

    void ShutdownSession();

    UIARFramePool* GetFramePool() const { return FramePool; }

    bool IsSessionInitialized() const { return bIsSessionInitialized; }

    // --- DELEGATES DA SESSÃO DE CAPTURA (INTERNOS, PUBLICOS PARA QUEM PRECISAR OUVIR EVENTOS DE GRAVAÇÃO DE ARQUIVO) ---
    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileRecordingStarted OnFileRecordingStarted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileRecordingStopped OnFileRecordingStopped;

    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileRecordingTakeStarted OnFileRecordingTakeStarted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileRecordingTakeStopped OnFileRecordingTakeStopped;

    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileMasterRecordingStarted OnFileMasterRecordingStarted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|File Recording Events")
    FOnIARFileMasterRecordingStopped OnFileMasterRecordingStopped;

private:
    FIAR_AudioStreamSettings CurrentSessionStreamSettings;
    FString CurrentOverallSessionName;

    UPROPERTY()
    TObjectPtr<UIARAudioEncoder> CurrentTakeEncoder;

    TArray<FString> CompletedTakeFilePaths;
    
    FTimerHandle TakeRotationTimerHandle;

    bool bIsOverallRecordingActive = false;

    UPROPERTY()
    UIARAudioSource* AudioSourceRef;

    FIAR_RecordingSettings RecordingSettings;

    int32 CurrentTakeNumber;

    FString SessionID;

    UPROPERTY()
    UIARFramePool* FramePool;

    bool bIsSessionInitialized = false;

    FString GenerateOutputFilePath(const FString& Prefix, const FString& Suffix = TEXT(""), int32 Index = -1) const;

    UFUNCTION()
    void OnEncoderErrorReceived(const FString& ErrorMessage);

    UFUNCTION()
    void OnPipeCongested(const FString& OutputFilePath);
    UFUNCTION()
    void OnPipeCleared(const FString& OutputFilePath);

    void StartNewTakeInternal();

    bool StopCurrentTakeInternal();

    UFUNCTION()
    void RotateCurrentTake();

    bool ConcatenateTakesToMaster(const FString& MasterFilePath, const TArray<FString>& TakeFilePaths);

    void DeleteTakeFiles(const TArray<FString>& FilePathsToDelete);
};
