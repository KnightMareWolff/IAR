// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioSource.h"
#include "../IAR.h"

UIARAudioSource::UIARAudioSource()
    : UIARMediaSource() // ATUALIZADO: Chamar construtor da classe base
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Construtor chamado."));
}

UIARAudioSource::~UIARAudioSource()
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Destrutor chamado."));
}

// ATUALIZADO: Overrides Initialize da classe base
void UIARAudioSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    Super::Initialize(StreamSettings, InFramePool); // Chama a inicialização da classe base
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Inicializado."));
}

// ATUALIZADO: Overrides StartCapture da classe base
void UIARAudioSource::StartCapture()
{
    Super::StartCapture(); // Chama a implementação da classe base
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Captura iniciada."));
}

// ATUALIZADO: Overrides StopCapture da classe base
void UIARAudioSource::StopCapture()
{
    Super::StopCapture(); // Chama a implementação da classe base
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Captura parada."));
}

// ATUALIZADO: Overrides Shutdown da classe base
void UIARAudioSource::Shutdown()
{
    Super::Shutdown(); // Chama a implementação da classe base
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSource: Shutdown concluído."));
}
