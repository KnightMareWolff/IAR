// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioMIDISource.h"
#include "../IAR.h"

// Adicione includes relevantes aqui (e.g., Core/IAR_Types.h, HAL/Runnable.h)

UIARAudioMIDISource::UIARAudioMIDISource()
{
    // Lógica do Construtor
    UE_LOG(LogIAR, Log, TEXT("IARAudioMIDISource: Inicializando..."));
}

UIARAudioMIDISource::~UIARAudioMIDISource()
{
    // Lógica do Destrutor (para limpeza de não-UObjects, se houver)
    UE_LOG(LogIAR, Log, TEXT("IARAudioMIDISource: Desinicializando..."));
}

void UIARAudioMIDISource::Initialize()
{
    // Lógica de Inicialização
    UE_LOG(LogIAR, Log, TEXT("IARAudioMIDISource: Inicializado com sucesso."));
    // Exemplo: Lançando uma worker thread
    // WorkerRunnable = new FMyWorkerRunnable();
    // WorkerThread = FRunnableThread::Create(WorkerRunnable, TEXT("MyWorkerThread"), 0, TPri_Normal);
}

void UIARAudioMIDISource::Shutdown()
{
    // Lógica de Desligamento
    UE_LOG(LogIAR, Log, TEXT("IARAudioMIDISource: Desligado."));
    // Exemplo: Parando e limpando uma worker thread
    // if (WorkerThread) { WorkerThread->WaitForCompletion(); delete WorkerThread; WorkerThread = nullptr; delete WorkerRunnable; WorkerRunnable = nullptr; }
}


