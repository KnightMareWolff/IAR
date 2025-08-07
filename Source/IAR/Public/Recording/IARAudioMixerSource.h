// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
// ATUALIZADO: Herda de UIARAudioSource
#include "IARAudioSource.h" 
#include "AudioCapture.h" 
#include "AudioCaptureCore.h" 
#include "AudioCaptureDeviceInterface.h" 
#include "Core/IAR_Types.h" 

using namespace Audio;

#include "IARAudioMixerSource.generated.h"

/**
 * @brief Fonte de áudio que gerencia entradas de áudio físico, como microfones ou mixers.
 * Utiliza o módulo AudioCapture da Unreal Engine para acesso ao microfone.
 */
UCLASS(BlueprintType)
class IAR_API UIARAudioMixerSource : public UIARAudioSource // ATUALIZADO: Herda de UIARAudioSource
{
    GENERATED_BODY()

public:
    UIARAudioMixerSource();
    virtual ~UIARAudioMixerSource();

    /**
     * @brief Inicializa a fonte de áudio do mixer.
     * @param StreamSettings As configurações do stream de áudio (SampleRate, NumChannels, InputDeviceIndex).
     * @param InFramePool O FramePool a ser utilizado para aquisição de frames.
     */
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia a captura de áudio do microfone.
     */
    virtual void StartCapture() override;

    /**
     * @brief Para a captura de áudio do microfone.
     */
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de áudio.
     */
    virtual void Shutdown() override;

    FAudioCapture& GetIARAudioCapture() const { return *AudioCapture; }

private:
    TUniquePtr<FAudioCapture> AudioCapture; 
    
    // Callback para receber os dados de áudio do sistema
    void OnAudioCapture(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow);
};
