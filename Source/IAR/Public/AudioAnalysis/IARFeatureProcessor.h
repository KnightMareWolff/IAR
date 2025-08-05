// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Core/IAR_Types.h" // Inclui FIAR_AudioFrameData e FIAR_AudioFeatures
#include "Engine/Texture2D.h" // Adicionado para garantir a visibilidade do tipo UTexture2D para o compilador na classe base abstrata
#include "IARFeatureProcessor.generated.h"

/**
 * @brief Base abstrata para processadores de features de áudio em tempo real.
 * As implementações concretas extraem diversas características de frames de áudio.
 * Agora inclui funções para pré-processamento de áudio (Noise Gate, Low/High Pass Filters).
 */
UCLASS(Abstract)
class IAR_API UIARFeatureProcessor : public UObject
{
    GENERATED_BODY()

public:
    UIARFeatureProcessor();
    virtual ~UIARFeatureProcessor();

    /**
     * @brief Inicializa o processador de features.
     */
    virtual void Initialize();

    /**
     * @brief Desliga o processador de features.
     */
    virtual void Shutdown();

    /**
     * @brief Processa um frame de áudio para extrair diversas características.
     * Este é um método virtual puro que deve ser implementado por subclasses.
     * @param AudioFrame O frame de áudio a ser processado (já com SampleRate e NumChannels corretos).
     * @param OutSpectrogramTexture Ponteiro para a textura do espectrograma para ser atualizada.
     * @return As características de áudio extraídas encapsuladas em FIAR_AudioFeatures.
     */
    virtual FIAR_AudioFeatures ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture);

    /**
     * @brief Aplica um Noise Gate aos samples de áudio.
     * @param Samples Array de samples de áudio a serem processados.
     * @param ThresholdRMS Limiar de RMS abaixo do qual o áudio será atenuado.
     * @param AttackTimeMs Tempo em milissegundos para o gate abrir completamente.
     * @param ReleaseTimeMs Tempo em milissegundos para o gate fechar completamente.
     * @param SampleRate Taxa de amostragem do áudio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Processing")
    virtual void ApplyNoiseGate(TArray<float>& Samples, float ThresholdRMS, float AttackTimeMs, float ReleaseTimeMs, int32 SampleRate);

    /**
     * @brief Aplica um filtro passa-baixa aos samples de áudio.
     * Implementação simples de um filtro IIR de primeira ordem.
     * @param Samples Array de samples de áudio a serem processados.
     * @param CutoffFrequencyHz Frequência de corte do filtro em Hz.
     * @param SampleRate Taxa de amostragem do áudio.
     * @param NumChannels Número de canais do áudio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Processing")
    virtual void ApplyLowPassFilter(TArray<float>& Samples, float CutoffFrequencyHz, int32 SampleRate, int32 NumChannels);

    /**
     * @brief Aplica um filtro passa-alta aos samples de áudio.
     * Implementação simples de um filtro IIR de primeira ordem.
     * @param Samples Array de samples de áudio a serem processados.
     * @param CutoffFrequencyHz Frequência de corte do filtro em Hz.
     * @param SampleRate Taxa de amostragem do áudio.
     * @param NumChannels Número de canais do áudio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Processing")
    virtual void ApplyHighPassFilter(TArray<float>& Samples, float CutoffFrequencyHz, int32 SampleRate, int32 NumChannels);

protected:
    // Estado para detecção de notas e contorno melódico
    FIAR_AudioNoteFeature LastDetectedNote;
    bool bHasPreviousNote = false;

    /**
     * @brief Calcula uma estimativa de pitch básica baseada em Zero Crossing Rate.
     * @param Samples As amostras de áudio monofônicas.
     * @param SampleRate A taxa de amostragem.
     * @return A frequência estimada em Hz.
     */
    float CalculateZeroCrossingRatePitchEstimate(const TArray<float>& Samples, int32 SampleRate) const;

private:
    // Para filtros IIR de primeira ordem
    TArray<float> Z_LowPass;  // Z-1 state for low-pass filter (one per channel)
    TArray<float> Z_HighPass; // Z-1 state for high-pass filter (one per channel)
};
