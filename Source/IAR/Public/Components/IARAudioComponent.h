// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h"
#include "Core/IARMediaSource.h"
#include "Core/IAR_Types.h" 
#include "Recording/IARAudioCaptureSession.h"
#include "Core/IARFramePool.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/LatentActionManager.h" // Para FLatentActionManager
#include "LatentActions.h" // Para FPendingLatentAction
#include "AudioCapture.h" 
#include "AudioCaptureDeviceInterface.h" 
#include "../GlobalStatics.h" 

#include "AudioAnalysis/IARFeatureProcessor.h" 
#include "AudioAnalysis/IARAdvancedAudioFeatureProcessor.h" 
#include "AudioAnalysis/IARBasicAudioFeatureProcessor.h" 
#include "AudioAnalysis/IARAudioToMIDITranscriber.h" 
#include "AudioAnalysis/IARMIDIToAudioSynthesizer.h" 
#include "Recording/IARAudioFileSource.h" // Incluído para poder referenciar IARAudioFileSource
#include "Recording/IARMIDIFileSource.h" // NOVO: Adicionado para MIDIFileSource
#include "Recording/IARAudioFolderSource.h" // <<-- ADICIONADO
#include "Core/IARSampleRateConverter.h" 

#include "../Core/IARLambdaLatentAction.h" // ADICIONADO: Para usar nossa LambdaLatentAction

#include "IARAudioComponent.generated.h"


// --- DELEGATES DE GRAVAÇÃO (PADRÃO IVR) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIARRecordingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIARRecordingPaused); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIARRecordingResumed); 
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIARRecordingStopped);

// --- DELEGATE PARA FRAMES EM TEMPO REAL (PADRÃO IVR) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealTimeAudioFrameReady, const FIAR_JustRTFrame&, RealTimeFrame);

// --- DELEGATES PARA FEEDBACK DO PROCESSAMENTO DE DIRETÓRIO (NOVOS) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFolderProcessingCompleted, const FString&, OutputFolderPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIARFolderProcessingError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIARFolderProcessingProgress, const FString&, CurrentFileName, float, ProgressRatio);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class IAR_API UIARAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UIARAudioComponent();
    virtual ~UIARAudioComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; 

    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Component")
    void SetupAudioComponent(const FIAR_AudioStreamSettings& StreamSettings);

    /**
     * @brief Inicia a gravação de áudio, extração de features em tempo real,
     * ou o processamento em lote de um diretório (se SourceType for Folder).
     * Para fontes de arquivo ou diretório, esta função é latente e gerencia o carregamento/processamento assíncrono.
     * @param WorldContextObject Contexto do mundo para a ação latente.
     * @param LatentInfo Informações para o sistema de ações latentes do Blueprint.
     * @param CustomRecordingName Opcional. Um nome customizado para a sessão ou para logs.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Recording", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
    void StartRecording(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& CustomRecordingName = TEXT(""));

    /**
     * @brief Para a gravação ou o processamento em lote (se SourceType for Folder).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Recording")
    void StopRecording();

    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Conversion", meta = (Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject"))
    void ConvertAudioFile(UObject* WorldContextObject, FString InSourceAudioPath, FString OutConvertedAudioPath, const FIAR_AudioConversionSettings& ConversionSettings, bool bOverwrite);

    // --- DELEGATES PUBLICOS DO UIARAudioComponent ---
    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Recording Events")
    FOnIARRecordingStarted OnRecordingStarted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Recording Events")
    FOnIARRecordingPaused OnRecordingPaused; 

    UPROPERTY(BlueprintAssignable, Category = "IAR|Recording Events")
    FOnIARRecordingResumed OnRecordingResumed; 

    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Recording Events")
    FOnIARRecordingStopped OnRecordingStopped;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Real-Time Features Events")
    FOnRealTimeAudioFrameReady OnRealTimeAudioFrameReady; 

    // (Novos) Delegates para Feedback do Processamento de Diretório
    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Processing Events")
    FOnIARFolderProcessingCompleted OnFolderProcessingCompleted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Processing Events")
    FOnIARFolderProcessingError OnFolderProcessingError;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Processing Events")
    FOnIARFolderProcessingProgress OnFolderProcessingProgress;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Component") 
    FIAR_AudioStreamSettings AudioStreamSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Component") 
    FIAR_RecordingSettings RecordingSettings;

    // Adicionado para armazenar o nome da sessão global que pode ser definida pelo Blueprint
    // e usada pela FLoadAudioFileLatentAction ao iniciar a gravação.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|Audio Component|Internal")
    FString CurrentOverallSessionName; 

    bool bIsActive; 
    bool bIsOverallPipelineInitialized; 
    bool bIsRecordingCapable;           

    UPROPERTY()
    TObjectPtr<UIARMediaSource> CurrentMediaSource; 

    UPROPERTY()
    TObjectPtr<UIARAudioCaptureSession> AudioCaptureSession;

    UPROPERTY()
    TObjectPtr<UIARFramePool> FramePool;

    FIARSampleRateConverter SampleRateConverter; 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|Audio Devices")
    TArray<FIAR_AudioDeviceInfo> AvailableAudioInputDevices_BPFriendly;

    // NOVO: Propriedade para selecionar a classe do Feature Processor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Analysis Features")
    TSubclassOf<UIARFeatureProcessor> FeatureProcessorClass;

    // O Feature Processor REAL instanciado (agora pode ser de qualquer classe derivada de UIARFeatureProcessor)
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "IAR|Audio Analysis Features")
    TObjectPtr<UIARFeatureProcessor> FeatureProcessorInstance; 

    UPROPERTY() // O componente agora é dono da textura do espectrograma
    UTexture2D* SpectrogramTexture;

    UPROPERTY() // O componente é dono da textura da waveform
    UTexture2D* WaveformTexture; 

    UPROPERTY() // NOVO: O componente é dono da textura do espectrograma filtrado
    UTexture2D* FilteredSpectrogramTexture; 

    UPROPERTY()
    TObjectPtr<UIARAudioToMIDITranscriber> MIDITranscriber; 

    // MODIFICADO: Adicionado EditAnywhere e Instanced para expor as propriedades do sintetizador
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "IAR|MIDI Synthesis") 
    TObjectPtr<UIARMIDIToAudioSynthesizer> SynthesizerInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|MIDI Synthesis", 
              meta = (Tooltip = "Ativa/desativa a saída de áudio do sintetizador MIDI (se a fonte for MIDI)."))
    bool bEnableMIDISynthesizerOutput = true; // Mantida aqui para controle geral

    // NOVAS PROPRIEDADES PARA FILTROS DE PRÉ-PROCESSAMENTO (APLICADOS ANTES DO FEATURE PROCESSOR)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing")
    bool bEnableNoiseGate = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing", meta = (EditCondition = "bEnableNoiseGate", ClampMin = "0.0", Tooltip = "Limiar de RMS abaixo do qual o audio é silenciado."))
    float NoiseGateThresholdRMS = 0.005f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing")
    bool bEnableLowPassFilter = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing", meta = (EditCondition = "bEnableLowPassFilter", ClampMin = "20.0", ClampMax = "20000.0", Tooltip = "Frequência de corte para o filtro passa-baixa (Hz)."))
    float LowPassCutoffFrequencyHz = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing")
    bool bEnableHighPassFilter = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Pre-Processing", meta = (EditCondition = "bEnableHighPassFilter", ClampMin = "20.0", ClampMax = "20000.0", Tooltip = "Frequência de corte para o filtro passa-alta (Hz)."))
    float HighPassCutoffFrequencyHz = 20.0f;

    // Função auxiliar que agora será chamada de dentro de StartRecording
    void InitializeAudioPipelineInternal();

    // Métodos de callback para OnAudioFrameAcquired e OnMIDIFrameAcquired (existente)
    void OnAudioFrameAcquired(TSharedPtr<FIAR_AudioFrameData> AudioFrame);
    void OnMIDIFrameAcquired(TSharedPtr<FIAR_MIDIFrame> MIDIFrame);

    // Handlers para os Delegates do UIARFolderSource (para retransmitir ou processar eventos)
    UFUNCTION()
    void HandleFolderProcessingCompleted(const FString& OutputFolderPath);

    UFUNCTION()
    void HandleFolderProcessingError(const FString& ErrorMessage);

    UFUNCTION()
    void HandleFolderProcessingProgress(const FString& CurrentFileName, float ProgressRatio);

public: 
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Devices")
    void EnumerateAudioInputDevices();

    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Devices")
    bool GetAudioInputDeviceById(const FString& InDeviceId, FIAR_AudioDeviceInfo& OutDeviceInfo);

    UFUNCTION(BlueprintPure, Category = "IAR|Audio Devices")
    TArray<FIAR_AudioDeviceInfo> GetAvailableAudioInputDevicesList();
}; 
