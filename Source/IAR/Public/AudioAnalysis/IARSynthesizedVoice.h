// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Core/IAR_Types.h" // Para FIAR_MIDIEvent

// Definição de Pi para cálculos de onda senoidal
#ifndef PI
#define PI 3.1415926535897932f
#endif

/**
 * @brief Representa uma única voz sintetizada (uma nota).
 */
struct FSynthesizedVoice
{
    // === Parâmetros da Nota ===
    int32 MIDINoteNumber = 0;
    float FrequencyHz = 0.0f;
    float Velocity = 0.0f; // 0.0 a 1.0 (normalizado de 0-127 MIDI)
    int32 SampleRateHz = 0; // Taxa de amostragem para esta voz

    // === Estado do Oscilador ===
    float CurrentPhase = 0.0f; // Posição atual na onda (0.0 a 2.0 * PI)
    float PhaseIncrement = 0.0f; // Quanto a fase deve aumentar por amostra

    // === Envelope ADSR ===
    enum EEnvelopeState
    {
        Attack,
        Decay,
        Sustain,
        Release,
        Off
    };
    EEnvelopeState EnvelopeState = Off;
    float EnvelopeLevel = 0.0f; // Nível de amplitude atual do envelope
    float TargetLevel = 0.0f; // Nível alvo para a fase atual do envelope
    float Rate = 0.0f; // Taxa de mudança para a fase atual do envelope
    float LastTriggerTime = 0.0f; // Timestamp de quando a nota foi iniciada/reiniciada

    // Parâmetros configuráveis do envelope (você pode movê-los para o sintetizador)
    float AttackTime = 0.05f;  // segundos
    float DecayTime = 0.1f;    // segundos
    float SustainLevel = 0.7f; // 0.0 a 1.0
    float ReleaseTime = 0.2f;  // segundos

    // === Funções de Controle ===

    /**
     * @brief Inicia ou reinicia a voz (Note On).
     * @param InMIDINoteNumber O número da nota MIDI (0-127).
     * @param InVelocity A velocidade da nota (0-1.0).
     * @param InSampleRateHz A Sample Rate atual para calcular o incremento de fase.
     * @param CurrentTime O tempo atual em segundos.
     */
    void NoteOn(int32 InMIDINoteNumber, float InVelocity, int32 InSampleRateHz, float CurrentTime)
    {
        // NOVO: Clampeia a nota MIDI para uma faixa audível mais razoável
        // Ex: MIDI 21 (A0) a MIDI 108 (C8). Isso evita frequências extremamente agudas ou graves.
        MIDINoteNumber = FMath::Clamp(InMIDINoteNumber, 21, 108); 

        Velocity = InVelocity;
        SampleRateHz = InSampleRateHz; 

        // Usa a MIDINoteNumber clampeada para calcular a frequência
        FrequencyHz = 440.0f * FMath::Pow(2.0f, (MIDINoteNumber - 69) / 12.0f); 
        
        PhaseIncrement = (2.0f * PI * FrequencyHz) / SampleRateHz;
        CurrentPhase = 0.0f; // Começa a onda do zero

        EnvelopeState = Attack;
        EnvelopeLevel = 0.0f;
        TargetLevel = 1.0f * Velocity; // Vai para o nível máximo (multiplicado pela velocidade)
        Rate = TargetLevel / (AttackTime * SampleRateHz); // Taxa por amostra
        LastTriggerTime = CurrentTime;
    }

    /**
     * @brief Inicia a fase de release da voz (Note Off).
     * @param CurrentTime O tempo atual em segundos.
     */
    void NoteOff(float CurrentTime)
    {
        if (EnvelopeState != Release && EnvelopeState != Off)
        {
            EnvelopeState = Release;
            TargetLevel = 0.0f; // Vai para zero
            Rate = EnvelopeLevel / (ReleaseTime * SampleRateHz); // Taxa baseada no nível atual
            LastTriggerTime = CurrentTime;
        }
    }

    /**
     * @brief Processa o envelope ADSR para uma única amostra.
     * @return O fator de amplitude atual do envelope.
     */
    float ProcessEnvelope() 
    {
        switch (EnvelopeState)
        {
            case Attack:
                EnvelopeLevel += Rate;
                if (EnvelopeLevel >= TargetLevel)
                {
                    EnvelopeLevel = TargetLevel;
                    EnvelopeState = Decay;
                    TargetLevel = SustainLevel * Velocity;
                    Rate = (EnvelopeLevel - TargetLevel) / (DecayTime * SampleRateHz);
                }
                break;
            case Decay:
                EnvelopeLevel -= Rate;
                if (EnvelopeLevel <= TargetLevel)
                {
                    EnvelopeLevel = TargetLevel;
                    EnvelopeState = Sustain;
                    Rate = 0.0f; // Nenhuma mudança de nível durante Sustain
                }
                break;
            case Sustain:
                break;
            case Release:
                EnvelopeLevel -= Rate;
                if (EnvelopeLevel <= 0.0f)
                {
                    EnvelopeLevel = 0.0f;
                    EnvelopeState = Off;
                }
                break;
            case Off:
                EnvelopeLevel = 0.0f;
                break;
        }
        return FMath::Clamp(EnvelopeLevel, 0.0f, 1.0f);
    }

    /**
     * @brief Gera a próxima amostra de áudio para esta voz.
     * @return O valor da amostra (entre -1.0 e 1.0).
     */
    float GenerateSample() 
    {
        if (EnvelopeState == Off) return 0.0f;

        // Gera a onda senoidal
        float Sample = FMath::Sin(CurrentPhase);
        
        // Atualiza a fase para a próxima amostra
        CurrentPhase += PhaseIncrement;
        if (CurrentPhase >= (2.0f * PI))
        {
            CurrentPhase -= (2.0f * PI); 
        }

        // Aplica o envelope e retorna a amostra final
        return Sample * ProcessEnvelope(); 
    }
};

