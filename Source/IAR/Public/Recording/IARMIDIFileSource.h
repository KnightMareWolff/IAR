// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "IARAudioSource.h" // Herda de UIARMediaSource
#include "Core/IARFramePool.h" 
#include "Core/IAR_Types.h"
#include "MIDI/MidiFile.h" // NOVO: Inclui a biblioteca MidiFile
#include "Containers/Queue.h" // Para TQueue (se usarmos para eventos)
#include "TimerManager.h"
#include "IARMIDIFileSource.generated.h"

/**
 * @brief Fonte de mídia que lê eventos MIDI de um arquivo .mid usando a biblioteca smf::MidiFile.
 */
UCLASS(BlueprintType)
class IAR_API UIARMIDIFileSource : public UIARMediaSource // Herda de UIARMediaSource
{
    GENERATED_BODY()

public:
    UIARMIDIFileSource();
    virtual ~UIARMIDIFileSource();

    /**
     * @brief Inicializa a fonte de arquivo MIDI.
     * @param StreamSettings As configurações do stream MIDI (FilePath, PlaybackSpeed, LoopPlayback).
     * @param InFramePool O FramePool (não usado para MIDI, mas mantido para consistência).
     */
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia a reprodução e envio de frames MIDI.
     */
    virtual void StartCapture() override;

    /**
     * @brief Para a reprodução de frames MIDI.
     */
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de MIDI.
     */
    virtual void Shutdown() override;

    /**
     * @brief Verifica se o arquivo MIDI foi carregado com sucesso.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI File Source")
    bool IsFileLoaded() const { return bIsFileLoaded; }

    /**
     * @brief Reseta o estado da fonte de arquivo MIDI.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI File Source")
    void ResetFileSource();

    /**
     * @brief Carrega o arquivo MIDI do disco de forma bloqueante (para ser executado em uma thread de background).
     * @return true se o carregamento e a configuração foram bem-sucedidos.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI File Source|Internal")
    bool Internal_LoadMIDIFileBlocking();

     /**
     * @brief Retorna uma referência constante para o array de eventos MIDI carregados.
     */
    const TArray<FIAR_MIDIEvent>& GetLoadedMIDIEvents() const { return LoadedMIDIEvents; } // <<-- LINHA ADICIONADA AQUI

public:
    // Armazena o caminho absoluto completo do arquivo no disco (para simulação/logging)
    FString FullDiskFilePathInternal; 

private:
    TArray<FIAR_MIDIEvent> LoadedMIDIEvents; // Eventos MIDI carregados do arquivo
    int32 CurrentMIDIEventIndex;             // Índice do evento MIDI atual sendo despachado
    FTimerHandle MIDIPlaybackTimerHandle;    // Timer para controlar o despacho de eventos
    bool bIsFileLoaded;                      // Indica se os eventos foram carregados

    float CurrentPlaybackTime; // Tempo atual da reprodução (em segundos)

    /**
     * @brief Método chamado pelo timer para despachar frames MIDI.
     */
    void ProcessMIDIEventFrame();
};
