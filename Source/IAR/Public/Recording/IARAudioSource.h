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
#include "Core/IARFramePool.h" 
// ATUALIZADO: Inclui a nova classe base
#include "Core/IARMediaSource.h" 

#include "IARAudioSource.generated.h"
// DELEGATE MOVIDO PARA IAR_Types.h e UIARMediaSource
// DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioFrameAcquired, TSharedPtr<FIAR_AudioFrameData>);

/**
 * @brief Classe base para fontes de áudio que produzem FIAR_AudioFrameData.
 * Agora herda de UIARMediaSource.
 */
UCLASS(Abstract, BlueprintType) 
class IAR_API UIARAudioSource : public UIARMediaSource // ATUALIZADO: Herda de UIARMediaSource
{
    GENERATED_BODY()

public:
    UIARAudioSource();
    virtual ~UIARAudioSource();

    /**
     * @brief Inicializa a fonte de áudio.
     * @param StreamSettings As configurações do stream de áudio desejadas para esta fonte.
     * @param InFramePool O FramePool a ser utilizado para aquisição de frames.
     */
    // ATUALIZADO: Overrides Initialize da classe base
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia a captura de áudio da fonte.
     */
    // ATUALIZADO: Overrides StartCapture da classe base
    virtual void StartCapture() override;

    /**
     * @brief Para a captura de áudio da fonte.
     */
    // ATUALIZADO: Overrides StopCapture da classe base
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de áudio.
     */
    // ATUALIZADO: Overrides Shutdown da classe base
    virtual void Shutdown() override;

    // Delegates (agora herdados de UIARMediaSource)
    // FOnAudioFrameAcquired OnFrameAcquired; // Removido, está em UIARMediaSource
};
