// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Core/IARFramePool.h"
#include "../IAR.h"

UIARFramePool::UIARFramePool()
    : DefaultSampleRate(0)
    , DefaultNumChannels(0)
    , DefaultFrameBufferSizeInSamples(0)
{
    UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Construtor chamado."));
}

UIARFramePool::~UIARFramePool()
{
    ClearPool(); // Garante que todos os frames sejam liberados
    UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Destrutor chamado."));
}

void UIARFramePool::InitializePool(int32 PoolSize, int32 SampleRate, int32 NumChannels, int32 FrameBufferSizeInSamples)
{
    ClearPool(); // Limpa qualquer estado anterior
    DefaultSampleRate = SampleRate;
    DefaultNumChannels = NumChannels;
    DefaultFrameBufferSizeInSamples = FrameBufferSizeInSamples;

    for (int32 i = 0; i < PoolSize; ++i)
    {
        TSharedPtr<FIAR_AudioFrameData> NewFrame = MakeShared<FIAR_AudioFrameData>(DefaultSampleRate, DefaultNumChannels, 0.0f);
        NewFrame->RawSamplesPtr->SetNumZeroed(DefaultFrameBufferSizeInSamples * DefaultNumChannels); // Pre-allocate buffer
        AvailableFrames.Enqueue(NewFrame);
    }
    UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Inicializado com %d frames. SampleRate=%d, Channels=%d, BufferSize=%d."),
        PoolSize, SampleRate, NumChannels, FrameBufferSizeInSamples);
}

TSharedPtr<FIAR_AudioFrameData> UIARFramePool::AcquireFrame()
{
    TSharedPtr<FIAR_AudioFrameData> Frame;
    if (AvailableFrames.Dequeue(Frame))
    {
        // Reset timestamp and ensure buffer size is correct (though it should be from init)
        Frame->Timestamp = 0.0f;
        Frame->SampleRate = DefaultSampleRate;
        Frame->NumChannels = DefaultNumChannels;
        // The RawSamplesPtr->TArray<float> is reused, so its memory is kept. Just ensure its size is correct.
        if (Frame->RawSamplesPtr->Num() != (DefaultFrameBufferSizeInSamples * DefaultNumChannels))
        {
            Frame->RawSamplesPtr->SetNumZeroed(DefaultFrameBufferSizeInSamples * DefaultNumChannels);
        }
        UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Frame adquirido do pool.")); // Alterado de Verbose para Log
        return Frame;
    }
    else
    {
        // Pool está vazio, cria um novo frame
        TSharedPtr<FIAR_AudioFrameData> NewFrame = MakeShared<FIAR_AudioFrameData>(DefaultSampleRate, DefaultNumChannels, 0.0f);
        NewFrame->RawSamplesPtr->SetNumZeroed(DefaultFrameBufferSizeInSamples * DefaultNumChannels);
        UE_LOG(LogIAR, Warning, TEXT("UIARFramePool: Pool vazio, novo frame criado. Considere aumentar o PoolSize inicial."));
        return NewFrame;
    }
}

void UIARFramePool::ReleaseFrame(TSharedPtr<FIAR_AudioFrameData> Frame)
{
    if (Frame.IsValid())
    {
        // Clear data for security/privacy if necessary, or just rely on new data overwriting it.
        // Frame->RawSamplesPtr->SetNumZeroed(0); // Optional: clear data
        AvailableFrames.Enqueue(Frame);
        UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Frame liberado de volta ao pool.")); // Alterado de Verbose para Log
    }
}

void UIARFramePool::ClearPool()
{
    TSharedPtr<FIAR_AudioFrameData> Frame;
    int32 Count = 0;
    while (AvailableFrames.Dequeue(Frame))
    {
        Count++;
    }
    UE_LOG(LogIAR, Log, TEXT("UIARFramePool: Pool limpo. %d frames liberados."), Count);
}
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Private\Core\IARFramePool.cpp ---
