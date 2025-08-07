// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
// ATUALIZADO: Herda de UIARAudioSource, que já herda de UIARMediaSource
#include "IARAudioSource.h" 
#include "Core/IARFramePool.h" 
#include "Misc/ScopeLock.h" 
#include "TimerManager.h"

#include "IARAudioSimulatedSource.generated.h"

/**
 * @brief Fonte de áudio simulada que gera uma onda senoidal programaticamente.
 * Ideal para testes do pipeline de áudio sem depender de hardware real.
 */
UCLASS(BlueprintType)
class IAR_API UIARAudioSimulatedSource : public UIARAudioSource // ATUALIZADO: Herda de UIARAudioSource
{
    GENERATED_BODY()

public:
    UIARAudioSimulatedSource();
    virtual ~UIARAudioSimulatedSource();

    /**
     * @brief Inicializa a fonte de áudio simulada.
     * @param StreamSettings As configurações do stream de áudio (SampleRate, NumChannels).
     */
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia a geração da onda senoidal e o envio de frames.
     */
    virtual void StartCapture() override;

    /**
     * @brief Para a geração da onda senoidal e o envio de frames.
     */
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de áudio.
     */
    virtual void Shutdown() override;

protected:
    /**
     * @brief Preenche um FIAR_AudioFrameData com dados de onda senoidal simulados.
     * Esta função é chamada periodicamente por um timer.
     */
    void FillSimulatedFrame();

private:
    
    FTimerHandle AudioGenerationTimerHandle; // Timer para gerar frames periodicamente
    
    float CurrentTimeAccumulator; // Acumulador de tempo para a geração da onda senoidal
    float SineWaveFrequencyHz; // Frequência da onda senoidal em Hz

    int32 SamplesPerFrame; // Quantidade de amostras por frame
    float FrameDurationSeconds; // Duração de cada frame em segundos

    float AmplitudeModulationTime; // Para variar a amplitude ao longo do tempo
};
