// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadSafeBool.h"

/**
 * @brief Thread de trabalho para ler a saída (stdout/stderr) de um processo externo.
 * Usado para capturar logs do FFmpeg sem bloquear a Game Thread.
 */
class FFMpegLogReader : public FRunnable
{
public:
    /**
     * @brief Construtor.
     * @param InReadPipe Handle para o pipe de leitura (do lado do processo pai).
     * @param InDisplayName Nome para exibir no log (ex: "FFmpeg STDOUT", "FFmpeg STDERR").
     * @param InOutCapturedData Opcional. Ponteiro para um TArray<uint8> onde os dados binários lidos do pipe serão armazenados.
     */
    FFMpegLogReader(void* InReadPipe, const FString& InDisplayName, TArray<uint8>* InOutCapturedData = nullptr);
    
    virtual ~FFMpegLogReader();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    /**
     * @brief Inicia a thread de leitura.
     */
    void Start();

    /**
     * @brief Solicita que a thread pare e espera pela sua conclusão.
     */
    void EnsureCompletion();

private:
    void* ReadPipe;                 // Handle para o pipe de leitura
    FString DisplayName;            // Nome para exibição nos logs
    FRunnableThread* WorkerThread;  // A thread de execução
    FThreadSafeBool bShouldStop;    // Flag para sinalizar a parada da thread
    TArray<uint8>* OutCapturedData; // Onde os dados lidos serão armazenados

    // Desabilita cópia e atribuição
    FFMpegLogReader(const FFMpegLogReader&) = delete;
    FFMpegLogReader& operator=(const FFMpegLogReader&) = delete;
};
