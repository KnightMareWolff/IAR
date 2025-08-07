// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h" 
#include "Core/IAR_Types.h" 
#include "Core/IARMIDITable.h" 
#include "Containers/Queue.h" 
#include "HAL/ThreadSafeBool.h" 
#include "Containers/Map.h" 

#include "IARAudioToMIDITranscriber.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogIARTranscriber, Log, All);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMIDITranscriptionEventGenerated, const FIAR_MIDIEvent& /*MIDIEvent*/);

/**
 * @brief Transcreve dados de áudio para eventos MIDI em tempo real (agora com suporte polifônico rudimentar).
 * Recebe FIAR_AudioFeatures e gera FIAR_MIDIEvent.
 */
UCLASS()
class IAR_API UIARAudioToMIDITranscriber : public UObject
{
    GENERATED_BODY()

public:
    UIARAudioToMIDITranscriber();
    virtual ~UIARAudioToMIDITranscriber();

    /**
     * @brief Inicializa o transcritor.
     * @param InSampleRate A taxa de amostragem do áudio de entrada esperado.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioAnalysis|MIDI Transcription")
    void Initialize(int32 InSampleRate);

    /**
     * @brief Processa um frame de features de áudio para gerar eventos MIDI.
     * @param AudioFeatures As features de áudio extraídas (RMS, Pitch, etc.).
     * @param Timestamp O tempo atual do frame em segundos.
     * @param FrameDuration A duração do frame em segundos.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioAnalysis|MIDI Transcription")
    void ProcessAudioFeatures(const FIAR_AudioFeatures& AudioFeatures, float Timestamp, float FrameDuration);

    /**
     * @brief Desliga o transcritor, finalizando quaisquer notas ativas.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioAnalysis|MIDI Transcription")
    void Shutdown();

    // Delegate para notificar eventos MIDI transcritos
    FOnMIDITranscriptionEventGenerated OnMIDITranscriptionEventGenerated;

protected:
    // Reordenado para resolver C5038
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|MIDI Transcription")
    int32 SampleRate; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|MIDI Transcription")
    float SilenceThresholdRMS = 0.005f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|MIDI Transcription")
    float PitchChangeThresholdHz = 5.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|MIDI Transcription")
    float MinNoteDuration = 0.05f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|MIDI Transcription",
              meta = (Tooltip = "Número de frames que uma nota deve estar ausente antes de ser desligada."))
    int32 NoteOffToleranceFrames = 5; 

private:
    TMap<int32, FIAR_AudioNoteFeature> ActiveNotesMap; 
    TMap<int32, int32> NoteOffToleranceCounters; 

    float PreviousPitchHz = 0.0f;
    float PitchSmoothingFactor = 0.7f; 

    int32 FreqToMidi(float FreqHz) const;
};
