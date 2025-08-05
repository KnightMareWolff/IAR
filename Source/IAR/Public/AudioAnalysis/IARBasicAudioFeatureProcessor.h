// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "AudioAnalysis/IARFeatureProcessor.h" // Herda da classe base abstrata
#include "Engine/Texture2D.h" // Adicionado para garantir a visibilidade do tipo UTexture2D
#include "IARBasicAudioFeatureProcessor.generated.h"

/**
 * @brief Implementa��o b�sica do processador de features de �udio.
 * Extrai m�tricas de dom�nio do tempo, estimativa de pitch e caracter�sticas do Attitude-Gram.
 */
UCLASS()
class IAR_API UIARBasicAudioFeatureProcessor : public UIARFeatureProcessor
{
    GENERATED_BODY()

public:
    UIARBasicAudioFeatureProcessor();
    virtual ~UIARBasicAudioFeatureProcessor();

    virtual void Initialize() override;
    virtual void Shutdown() override;

    /**
     * @brief Implementa��o concreta de ProcessFrame para extrair features e Attitude-Gram.
     * @param AudioFrame O frame de �udio a ser processado.
     * @param OutSpectrogramTexture Ponteiro para a textura do espectrograma para ser atualizada (esta classe n�o a usa, mas a assinatura deve corresponder).
     * @return As caracter�sticas de �udio extra�das.
     */
    virtual FIAR_AudioFeatures ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture) override;

private:
    /**
     * @brief Rudimentar detec��o de nota a partir do frame de �udio.
     * Considera uma "nota" se o pitch � est�vel e a amplitude acima de um limiar.
     * @param CurrentPitchHz Pitch estimado do frame.
     * @param CurrentRMS RMS do frame.
     * @param CurrentTimestamp Timestamp do frame atual.
     * @param FrameDuration Dura��o do frame em segundos.
     * @return true se uma nova nota foi detectada/finalizada neste frame, false caso contr�rio.
     */
    bool RudimentaryNoteDetection(float CurrentPitchHz, float CurrentRMS, float CurrentTimestamp, float FrameDuration);

    // Estado para Attitude-Gram (acumuladores para c�lculo ao longo do tempo)
    TArray<FIAR_AudioNoteFeature> DetectedNotesHistory; // Hist�rico de notas para Attitude-Gram
    FIAR_AudioNoteFeature CurrentBuildingNote; // Nota sendo "constru�da" ao longo dos frames
    float NoteOnsetTimestamp = 0.0f; // Timestamp de in�cio da nota atual
    bool bIsNoteActive = false; // Flag para indicar se uma nota est� "ativa"

    // Par�metros para detec��o rudimentar de notas
    const float SilenceThresholdRMS = 0.005f; // RMS abaixo deste valor � considerado sil�ncio
    const float PitchChangeThresholdHz = 5.0f; // Mudan�a m�nima de pitch para considerar nova nota
    const float MinNoteDuration = 0.05f; // Dura��o m�nima para considerar uma detec��o como nota
};
