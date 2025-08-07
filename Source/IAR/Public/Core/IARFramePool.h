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
#include "Core/IAR_Types.h" // Incluir FIAR_AudioFrameData
#include "Containers/Queue.h" // Para TQueue (thread-safe)

#include "IARFramePool.generated.h"

/**
 * @brief Pool de objetos para reuso eficiente de buffers de áudio (FIAR_AudioFrameData).
 * Projetado para ser thread-safe, permitindo que múltiplas threads adquiram e liberem frames.
 */
UCLASS()
class IAR_API UIARFramePool : public UObject
{
    GENERATED_BODY()

public:
    UIARFramePool();
    virtual ~UIARFramePool();

    /**
     * @brief Inicializa o pool com um tamanho inicial e as configurações de frame.
     * @param PoolSize O número inicial de frames a serem pré-alocados no pool.
     * @param SampleRate A taxa de amostragem para os frames.
     * @param NumChannels O número de canais para os frames.
     * @param FrameBufferSizeInSamples O tamanho (em amostras) do buffer de áudio de cada frame.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Frame Pool")
    void InitializePool(int32 PoolSize, int32 SampleRate, int32 NumChannels, int32 FrameBufferSizeInSamples);

    /**
     * @brief Adquire um FIAR_AudioFrameData do pool. Se o pool estiver vazio, um novo frame é criado.
     * @return Um TSharedPtr para um FIAR_AudioFrameData.
     */
    TSharedPtr<FIAR_AudioFrameData> AcquireFrame();

    /**
     * @brief Libera um FIAR_AudioFrameData de volta para o pool para reuso.
     * @param Frame O TSharedPtr para o frame a ser liberado.
     */
    void ReleaseFrame(TSharedPtr<FIAR_AudioFrameData> Frame);

    /**
     * @brief Limpa o pool, liberando toda a memória alocada.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Frame Pool")
    void ClearPool();

private:
    TQueue<TSharedPtr<FIAR_AudioFrameData>, EQueueMode::Mpsc> AvailableFrames; // Multi-produtor, single-consumidor
    
    // Configurações padrão para frames criados pelo pool
    int32 DefaultSampleRate;
    int32 DefaultNumChannels;
    int32 DefaultFrameBufferSizeInSamples;
};
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Public\Core\IARFramePool.h ---
