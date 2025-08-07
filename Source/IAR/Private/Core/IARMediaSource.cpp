// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Core/IARMediaSource.h"
#include "../IAR.h"

UIARMediaSource::UIARMediaSource()
    : FramePool(nullptr)
{
    UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: Construtor chamado."));
}

UIARMediaSource::~UIARMediaSource()
{
    UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: Destrutor chamado."));
}

void UIARMediaSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    CurrentStreamSettings = StreamSettings;
    FramePool = InFramePool;

    // FramePool pode ser nulo se a fonte for apenas MIDI e n�o precisar de buffers de �udio.
    if (!FramePool && (StreamSettings.ContentType == EIARMediaContentType::Audio || StreamSettings.ContentType == EIARMediaContentType::AutoDetect))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMediaSource: FramePool � nulo. A fonte de �udio n�o pode ser inicializada."));
        // N�o define bIsInitialized aqui, pois a inicializa��o falhou.
    }

    UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: Inicializado com SampleRate=%d, Channels=%d, Codec=%s, ContentType=%s."),
        CurrentStreamSettings.SampleRate, CurrentStreamSettings.NumChannels, *CurrentStreamSettings.Codec, *UEnum::GetValueAsString(CurrentStreamSettings.ContentType));
}

void UIARMediaSource::StartCapture()
{
    if (!bIsCapturing)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: StartCapture chamado. (Implementa��o base, subclasses devem sobrescrever)"));
        bIsCapturing = true;
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMediaSource: StartCapture chamado, mas j� est� capturando."));
    }
}

void UIARMediaSource::StopCapture()
{
    if (bIsCapturing)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: StopCapture chamado. (Implementa��o base, subclasses devem sobrescrever)"));
        bIsCapturing = false;
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMediaSource: StopCapture chamado, mas n�o est� capturando."));
    }
}

void UIARMediaSource::Shutdown()
{
    StopCapture();
    FramePool = nullptr; // Libera a refer�ncia, o pool � gerenciado pela sess�o
    UE_LOG(LogIAR, Log, TEXT("UIARMediaSource: Shutdown chamado. (Implementa��o base, subclasses devem sobrescrever)"));
}
