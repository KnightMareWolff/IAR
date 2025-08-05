// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h" // Para LogIAR
#include "Core/IAR_Types.h"
#include "Core/IARMIDITable.h" 
#include "IARSynthesizedVoice.h" // Inclua a nova estrutura FSynthesizedVoice (assumindo em Public/AudioAnalysis)
#include "AudioDevice.h" // Para acessar o dispositivo de audio (USoundWaveProcedural)
#include "Sound/SoundWaveProcedural.h" // Para gerar audio dinamicamente
#include "Components/AudioComponent.h" // <--- ADICIONE ESTA LINHA!
#include "TimerManager.h"
#include "IARMIDIToAudioSynthesizer.generated.h" 

// Delegate para emitir frames de audio gerados, se necessário (opcional para o primeiro passo)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSynthesizedAudioFrameReady, const TArray<float>& /*AudioBuffer*/);

UCLASS()
class IAR_API UIARMIDIToAudioSynthesizer : public UObject
{
    GENERATED_BODY()

public:
    UIARMIDIToAudioSynthesizer(); 
    virtual ~UIARMIDIToAudioSynthesizer(); 

    /**
     * @brief Inicializa o sintetizador de audio.
     * @param InSampleRate A Sample Rate na qual o sintetizador operará.
     * @param InNumChannels O número de canais de saída (1 para mono, 2 para estéreo).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioSynthesis")
    void Initialize(int32 InSampleRate, int32 InNumChannels);

    /**
     * @brief Desliga o sintetizador, interrompendo a geração de audio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioSynthesis")
    void Shutdown();

    /**
     * @brief Processa um evento MIDI (Note On/Off) e atualiza o estado do sintetizador.
     * @param MIDIEvent O evento MIDI a ser processado.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioSynthesis")
    void ProcessMIDIEvent(const FIAR_MIDIEvent& MIDIEvent);

    /**
     * @brief Inicia a reprodução do audio gerado.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioSynthesis")
    void StartPlayback();

    /**
     * @brief Para a reprodução do audio gerado.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|AudioSynthesis")
    void StopPlayback();

    /**
     * @brief Retorna o intervalo de tempo para geração de buffers de áudio.
     * @return O intervalo de tempo em segundos.
     */
    UFUNCTION(BlueprintPure, Category = "IAR|AudioSynthesis|Settings") // <--- LINHA ADICIONADA AQUI
    float GetAudioBufferInterval() const { return AudioBufferInterval; } // <--- LINHA ADICIONADA AQUI

    // Delegate para emitir os frames de audio gerados (opcional para o primeiro passo)
    FOnSynthesizedAudioFrameReady OnSynthesizedAudioFrameReady;

    // Método que será chamado pelo timer para gerar audio
    void GenerateAudioBuffer();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|AudioSynthesis")
    int32 SampleRate = 48000;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|AudioSynthesis")
    int32 NumChannels = 2; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioSynthesis|Settings",
              meta = (ClampMin = "1", Tooltip = "Número máximo de notas que podem tocar simultaneamente."))
    int32 MaxPolyphony = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioSynthesis|Settings",
              meta = (ClampMin = "0.01", Tooltip = "Intervalo de tempo em segundos para gerar buffers de audio."))
    float AudioBufferInterval = 0.02f; // Gerar a cada 20ms

private:
    // Mapeamento de MIDI Note Number para a voz ativa
    TMap<int32, FSynthesizedVoice*> ActiveVoices; 

    // Pool de vozes para reutilização (evita alocações constantes)
    TArray<FSynthesizedVoice> VoicePool; // Os objetos FSynthesizedVoice reais

    UPROPERTY()
    USoundWaveProcedural* SynthesizedSoundWave; // Objeto que receberá as amostras de audio

    UPROPERTY()
    UAudioComponent* AudioPlayerComponent; // <--- ADICIONE ESTA LINHA! Referência ao componente de áudio que está tocando.

    FTimerHandle AudioGenerationTimerHandle; // Timer para gerar buffers de audio periodicamente

    // Estado do tempo para sincronização do sintetizador
    float CurrentSynthesizerTime = 0.0f;
};
