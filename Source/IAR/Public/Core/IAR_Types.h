// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h" // Para UTextureRenderTarget2D
#include "Engine/Texture2D.h"             // Para UTexture2D
#include "IAR_Types.generated.h"

// Enum para definir o tipo de fonte de áudio
UENUM(BlueprintType)
enum class EIARAudioSourceType : uint8
{
    Simulated       UMETA(DisplayName = "Simulated Audio"),
    AudioMixer      UMETA(DisplayName = "Audio Mixer Device"),
    AudioFile       UMETA(DisplayName = "Single Audio File from Disk"),
    Folder          UMETA(DisplayName = "Folder with Media Files"), // <<-- ADICIONADO
    MIDIInput       UMETA(DisplayName = "MIDI Input Device (Live)"), 
    MIDIFile        UMETA(DisplayName = "MIDI File from Disk"), 
};

// NOVO: Enum para definir o tipo de conteúdo de mídia
UENUM(BlueprintType)
enum class EIARMediaContentType : uint8
{
    Audio           UMETA(DisplayName = "Audio (WAV, MP3, etc.)"),
    MIDI            UMETA(DisplayName = "MIDI (Symbolic Data)"),
    AutoDetect      UMETA(DisplayName = "Auto-Detect from File Extension") // Para FolderSource
};

/**
 * @brief Estrutura para configurar as propriedades do stream de áudio (taxa de amostragem, canais, codec, etc.).
 * Esta estrutura define como o áudio será capturado ou codificado.
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_AudioStreamSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    int32 SampleRate = 48000; // Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    int32 NumChannels = 2; // Estéreo

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    int32 BitDepth = 16; // 16-bit inteiro (para gravação) ou float (para processamento interno)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    FString Codec = TEXT("PCM"); // Ex: PCM, AAC, MP3, FLAC, OPUS

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    int32 Bitrate = 192000; // Em bps (para codecs comprimidos)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    EIARAudioSourceType SourceType = EIARAudioSourceType::Simulated; 

    // NOVO: Tipo de conteúdo para fontes baseadas em arquivo/pasta
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Content Type") // Categoria mais genérica
    EIARMediaContentType ContentType = EIARMediaContentType::Audio; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings")
    bool bEnableResampling = true; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Real-Time Output", 
              meta = (Tooltip = "Habilita a extração e broadcast de features de áudio em tempo real para Blueprints."))
    bool bEnableRTFeatures = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Real-Time Output",
              meta = (Tooltip = "Habilita a visualização de debug de features (e.g., espectrogramas em UTexture2D)."))
    bool bDebugDrawFeatures = false; 

    // Parâmetros específicos de cada fonte
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Source Specific",
              meta = (EditCondition = "SourceType == EIARAudioSourceType::AudioFile || SourceType == EIARAudioSourceType::MIDIFile", EditConditionHides))
    FString FilePath; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Source Specific",
                meta = (EditCondition = "SourceType == EIARAudioSourceType::Folder", EditConditionHides)) // <<-- ADICIONADO
    FString FolderPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Source Specific",
meta = (EditCondition = "SourceType == EIARAudioSourceType::AudioMixer || SourceType == EIARAudioSourceType::MIDIInput", EditConditionHides))
    int32 InputDeviceIndex = 0; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Playback Control")
    float PlaybackSpeed = 1.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Audio Stream Settings|Playback Control")
    bool bLoopPlayback = false; 
};

/**
 * @brief Representa um buffer de áudio bruto (um 'frame' de áudio) com metadados.
 * Esta estrutura é usada para passar dados de áudio entre componentes do plugin em tempo real.
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_AudioFrameData
{
    GENERATED_BODY()

    TSharedPtr<TArray<float>> RawSamplesPtr;

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio Frame Data")
    int32 SampleRate = 0;

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio Frame Data")
    int32 NumChannels = 0;

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio Frame Data")
    float Timestamp = 0.0f; 

    // ATUALIZADO: Adicionada FIAR_AudioStreamSettings ao frame para que processadores possam acessá-lo.
    // Isso é útil para flags como bDebugDrawFeatures
    FIAR_AudioStreamSettings CurrentStreamSettings;

    FIAR_AudioFrameData() : RawSamplesPtr(MakeShared<TArray<float>>()) {} 
    FIAR_AudioFrameData(int32 InSampleRate, int32 InNumChannels, float InTimestamp)
        : RawSamplesPtr(MakeShared<TArray<float>>()), SampleRate(InSampleRate), NumChannels(InNumChannels), Timestamp(InTimestamp) {}
};

// Representa um evento MIDI
USTRUCT(BlueprintType)
struct IAR_API FIAR_MIDIEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Event")
    uint8 Status; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Event")
    uint8 Data1; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Event")
    uint8 Data2; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Event")
    float Timestamp = 0.0f; 
};

// Representa um buffer de eventos MIDI
// ATUALIZADO: A data do MIDIFrame deve ser um Array de MIDI Events, e não apenas um Timestamp
USTRUCT(BlueprintType)
struct IAR_API FIAR_MIDIFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Frame")
    TArray<FIAR_MIDIEvent> Events; // O conteúdo do frame MIDI

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Frame")
    float Timestamp; // Tempo de início deste frame MIDI (do primeiro evento, ou início do chunk)
    // Duração do frame MIDI (útil para sincronização com áudio, se aplicável)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Frame")
    float Duration; 

    FIAR_MIDIFrame() : Timestamp(0.0f), Duration(0.0f) {}
    FIAR_MIDIFrame(float InTimestamp, float InDuration) : Timestamp(InTimestamp), Duration(InDuration) {}
};

/**
 * @brief Estrutura para uma única feature de nota de áudio detectada.
 * Estendida para incluir características relevantes para Mu6/Attitude-Gram,
 * com detalhes do "Piano Roll" (MIDINotesTable).
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_AudioNoteFeature
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    FString NoteName = TEXT(""); 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    bool bIsBemol = false; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    bool bIsSharp = false; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    float StartTime = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    float Duration = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    float PitchHz = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    int32 MIDINoteNumber = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    int32 Octave = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note",
              meta = (Tooltip = "Número de semitons da nota atual em relação à nota anterior. Usado no Contorno Melódico Mu6."))
    float SemitonesFromPrevious = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Note")
    float Velocity = 0.0f; 
};

/**
 * @brief Encapsula todas as features de áudio extraídas.
 * Estendida para incluir características do Attitude-Gram.
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_AudioFeatures
{
    GENERATED_BODY()

    // Métricas de Domínio do Tempo
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Time Domain")
    float RMSAmplitude = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Time Domain")
    float PeakAmplitude = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Time Domain")
    float ZeroCrossingRate = 0.0f; 

    // Métricas de Domínio da Frequência (calculadas via FFT ou estimativas)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    float SpectralCentroid = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    float SpectralBandwidth = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    float SpectralFlatness = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    float SpectralRollOff = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    float PitchEstimate = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Frequency Domain")
    TArray<float> MFCCs; 

    // Notas/eventos detectados (uma sequência de notas detectadas no frame/segmento de áudio)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Notes")
    TArray<FIAR_AudioNoteFeature> DetectedNotes; 

    // Resultados diretos da conversão MIDI (se Áudio para MIDI estiver ativo - futuro)
    // ATUALIZADO: Agora é um array de eventos MIDI, não um frame completo, pois MIDIFrame é para source.
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|MIDI Conversion")
    TArray<FIAR_MIDIEvent> ProcessedMIDIEvents; // Eventos MIDI processados/detectados neste frame de áudio
// Características do Attitude-Gram (calculadas a partir de DetectedNotesHistory)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    int32 OctavesUsed = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    int32 AccidentalsUsed = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    float AverageNoteDuration = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    int32 MostUsedMidiNote = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    int32 UniqueMidiNotesCount = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    int32 MaxConsecutiveRepeats = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram")
    float AverageBPM = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Features|Attitude-Gram",
              meta = (Tooltip = "Score de atitude do músico, baseado em duração média e repetições."))
    float AttitudeScore = 0.0f; 
};

/**
 * @brief Estrutura para a saída de frames de áudio em tempo real com features.
 * Consolidará os dados para o Blueprint.
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_JustRTFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    TArray<float> RawAudioBuffer; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    int32 SampleRate = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    int32 NumChannels = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    float Timestamp = 0.0f; 

    // Texturas para visualização (se bDebugDrawFeatures for true)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    UTexture2D* WaveformTexture = nullptr; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    UTexture2D* SpectrogramTexture = nullptr; 

    // NOVO: Textura para o espectrograma filtrado (visualização do efeito do filtro contextual)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    UTexture2D* FilteredSpectrogramTexture = nullptr; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Real-Time Output")
    FIAR_AudioFeatures Features; 
};

// Estrutura para configurações de Named Pipes
USTRUCT(BlueprintType)
struct IAR_API FIAR_PipeSettings
{
    GENERATED_BODY()

    FString BasePipeName = TEXT("UnrealAudioPipe");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    bool bIsServerPipe = true; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    bool bDuplexAccess = false; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    bool bBlockingMode = true; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    bool bMessageMode = false; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    int32 MaxInstances = 1; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    int32 OutBufferSize = 524288; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Pipe Settings")
    int32 InBufferSize = 524288; 
};

// Estrutura para configurações de gravação
USTRUCT(BlueprintType)
struct IAR_API FIAR_RecordingSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings")
    FString BaseOutputFolder = TEXT("Recording"); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings")
    FString MasterRecordingPrefix = TEXT("Master"); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings")
    FString TakeRecordingPrefix = TEXT("Take"); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings")
    bool bAppendTimestamp = true; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings")
    int32 InitialTakeNumber = 1; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Recording Settings", meta = (ClampMin = "0.1"))
    float TakeDurationSeconds = 5.0f; 
};

// Estrutura para informações de Take (similar ao IVR)
USTRUCT(BlueprintType)
struct IAR_API FIAR_TakeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    int32 TakeNumber = 0; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    float Duration = 0.0f; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    FDateTime StartTime; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    FDateTime EndTime; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    FString FilePath; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Take Info")
    FString SessionID; 
};

// Estrutura para configurar a conversão de áudio
USTRUCT(BlueprintType)
struct IAR_API FIAR_AudioConversionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Conversion Settings")
    FString Codec = TEXT("MP3"); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Conversion Settings")
    int32 Bitrate = 192000; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Conversion Settings", 
                meta = (Tooltip = "Taxa de amostragem de saída. 0 para usar a mesma do input."))
    int32 SampleRate = 0; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Conversion Settings", 
                meta = (Tooltip = "Número de canais de saída. 0 para usar o mesmo do input."))
    int32 NumChannels = 0; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Conversion Settings",
                meta = (Tooltip = "Se verdadeiro, o FFmpeg tentará otimizar para menor tamanho de arquivo. Pode demorar mais."))
    bool bOptimizeForSize = false;

    FString GetDefaultExtension() const
    {
        if (Codec.Equals(TEXT("MP3"), ESearchCase::IgnoreCase)) return TEXT(".mp3");
        if (Codec.Equals(TEXT("AAC"), ESearchCase::IgnoreCase)) return TEXT(".m4a"); 
        if (Codec.Equals(TEXT("FLAC"), ESearchCase::IgnoreCase)) return TEXT(".flac");
        if (Codec.Equals(TEXT("PCM"), ESearchCase::IgnoreCase)) return TEXT(".wav");
        return TEXT(".bin"); 
    }
};

// Struct para representar um dispositivo de áudio para fins de Blueprint/exibição
USTRUCT(BlueprintType)
struct FIAR_AudioDeviceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio")
    FString DeviceName; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio")
    FString DeviceId; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio")
    int32 NumInputChannels; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio")
    int32 SampleRate; 

    UPROPERTY(BlueprintReadOnly, Category = "IAR|Audio")
    bool bIsDefaultDevice; 
};
