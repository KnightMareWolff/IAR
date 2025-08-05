// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "FFmpegLogReader.h"
#include "../IAR.h"

FFMpegLogReader::FFMpegLogReader(void* InReadPipe, const FString& InDisplayName, TArray<uint8>* InOutCapturedData)
    : ReadPipe(InReadPipe)
    , DisplayName(InDisplayName)
    , WorkerThread(nullptr)
    , bShouldStop(false)
    , OutCapturedData(InOutCapturedData)
{
}

FFMpegLogReader::~FFMpegLogReader()
{
    EnsureCompletion(); // Garante que a thread terminou antes de destruir o objeto
}

bool FFMpegLogReader::Init()
{
    return true;
}

uint32 FFMpegLogReader::Run()
{
    TArray<uint8> Buffer;
    Buffer.Reserve(4096); 

    while (!bShouldStop)
    {
        Buffer.Reset(); 
        
        // FPlatformProcess::ReadPipeToArray retorna true se a leitura foi bem-sucedida (mesmo que 0 bytes),
        // e false se o pipe foi fechado ou ocorreu um erro.
        bool bReadSuccess = FPlatformProcess::ReadPipeToArray(ReadPipe, Buffer);
        uint32 BytesRead = Buffer.Num();

        if (BytesRead > 0)
        {
            FString LogOutput = FString(BytesRead, (ANSICHAR*)Buffer.GetData());
            UE_LOG(LogIAR, Log, TEXT(" %s  %s"), *DisplayName, *LogOutput.TrimEnd());

            if (OutCapturedData)
            {
                OutCapturedData->Append(Buffer.GetData(), BytesRead);
            }
        }
        else // BytesRead é 0
        {
            if (!bReadSuccess) // O pipe foi fechado ou um erro ocorreu
            {
                UE_LOG(LogIAR, Log, TEXT("FFMpegLogReader: Pipe %s closed. Exiting thread."), *DisplayName);
                break; // Sai do loop, o pipe está fechado
            }
            // Se bReadSuccess é true mas BytesRead é 0, o pipe está aberto mas vazio.
            FPlatformProcess::Sleep(0.01f); // Pequena pausa para evitar busy-waiting
        }
    }
    return 0;
}

void FFMpegLogReader::Stop()
{
    bShouldStop.AtomicSet(true);
}

void FFMpegLogReader::Exit()
{
    // Limpeza adicional se necessário
}

void FFMpegLogReader::Start()
{
    if (!WorkerThread)
    {
        WorkerThread = FRunnableThread::Create(this, *FString::Printf(TEXT("FFmpegLogReaderThread_%s"), *DisplayName), 0, TPri_BelowNormal);
    }
}

void FFMpegLogReader::EnsureCompletion()
{
    Stop(); // Sinaliza para parar
    if (WorkerThread)
    {
        WorkerThread->WaitForCompletion(); // Espera a thread terminar
        delete WorkerThread;
        WorkerThread = nullptr;
    }
}
