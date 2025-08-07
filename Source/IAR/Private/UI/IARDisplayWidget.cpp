// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------

#include "UI/IARDisplayWidget.h"
#include "../IAR.h"

UIARDisplayWidget::UIARDisplayWidget(const FObjectInitializer& ObjectInitializer)
    : UUserWidget(ObjectInitializer)
    , CurrentSpectrogramTexture(nullptr)
    , OwningAudioComponent(nullptr)
{
    UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: Construtor chamado."));
}

UIARDisplayWidget::~UIARDisplayWidget()
{
    UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: Destrutor chamado."));
}

void UIARDisplayWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Se o componente de captura já foi definido (via Blueprint ou em outra parte do C++),
    // inscreva-se no delegate. O SetCaptureComponent já lida com isso, mas é bom ter uma
    // verificação aqui para garantir caso o componente seja setado antes do NativeConstruct.
    if (OwningAudioComponent && !OwningAudioComponent->OnRealTimeAudioFrameReady.IsBound())
    {
        OwningAudioComponent->OnRealTimeAudioFrameReady.AddDynamic(this, &UIARDisplayWidget::OnRealTimeFrameReadyHandler);
    }

    UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: NativeConstruct chamado."));
}

void UIARDisplayWidget::NativeDestruct()
{
    UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: NativeDestruct chamado."));
    // É CRUCIAL desinscrever-se do delegate para evitar vazamentos de memória e crashes
    // quando o widget é destruído, mas o componente de captura ainda está ativo.
    if (OwningAudioComponent)
    {
        OwningAudioComponent->OnRealTimeAudioFrameReady.RemoveDynamic(this, &UIARDisplayWidget::OnRealTimeFrameReadyHandler);
    }
    CurrentSpectrogramTexture = nullptr; // Limpe a referência da textura ao destruir
    Super::NativeDestruct();
}

void UIARDisplayWidget::UpdateSpectrogram(UTexture2D* NewTexture)
{
    if (NewTexture)
    {
        CurrentSpectrogramTexture = NewTexture;
        UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: Espectrograma atualizado."));
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARDisplayWidget: Tentativa de atualizar espectrograma com textura nula."));
        CurrentSpectrogramTexture = nullptr;
    }
}

void UIARDisplayWidget::UpdateWaveform(UTexture2D* NewTexture)
{
    if (NewTexture)
    {
        CurrentWaveFormTexture = NewTexture;
        UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: Waveform atualizado."));
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARDisplayWidget: Tentativa de atualizar waveform com textura nula."));
        CurrentWaveFormTexture = nullptr;
    }
}

void UIARDisplayWidget::UpdateFilter(UTexture2D* NewTexture)
{
    if (NewTexture)
    {
        CurrentFilterTexture = NewTexture;
        UE_LOG(LogIAR, Log, TEXT("UIARDisplayWidget: Waveform atualizado."));
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARDisplayWidget: Tentativa de atualizar waveform com textura nula."));
        CurrentFilterTexture = nullptr;
    }
}

void UIARDisplayWidget::SetAudioComponentReference(UIARAudioComponent* InAudioComponent)
{
    // Verifica se o componente de captura está mudando.
    if (OwningAudioComponent != InAudioComponent)
    {
        // Primeiro, desinscreva-se do componente antigo, se houver, para evitar inscrições duplicadas
        // ou chamadas de delegate para um objeto inválido.
        if (OwningAudioComponent)
        {
            OwningAudioComponent->OnRealTimeAudioFrameReady.RemoveDynamic(this, &UIARDisplayWidget::OnRealTimeFrameReadyHandler);
        }
        
        // Atribui o novo componente.
        OwningAudioComponent = InAudioComponent;
        
        // Se o novo componente é válido, inscreva-se no delegate.
        // A textura CurrentLiveTexture será preenchida pela primeira vez quando o OnRealTimeFrameReadyHandler for chamado.
        if (OwningAudioComponent)
        {
            OwningAudioComponent->OnRealTimeAudioFrameReady.AddDynamic(this, &UIARDisplayWidget::OnRealTimeFrameReadyHandler);
        }
        else
        {
            // Se o componente foi desvinculado (passaram um nullptr), limpe a textura.
            CurrentSpectrogramTexture = nullptr;
        }
    }
}

void UIARDisplayWidget::OnRealTimeFrameReadyHandler(const FIAR_JustRTFrame& FrameData)
{
    // Este método é chamado a cada vez que o UIVRCaptureComponent broadcasting um novo frame.
    // A textura UTexture2D* que precisamos está dentro de FrameData.LiveTexture.
    // Basta atribuí-la à nossa propriedade CurrentLiveTexture.
    CurrentSpectrogramTexture = FrameData.SpectrogramTexture;
    CurrentWaveFormTexture = FrameData.WaveformTexture;
    CurrentFilterTexture = FrameData.FilteredSpectrogramTexture;
    // IMPORTANTE: O UMG detecta automaticamente que a textura para a qual CurrentLiveTexture
    // aponta foi atualizada (pois o UIVRCaptureComponent escreve novos pixels nela) e redesenha o Image.
    // Não é necessário fazer mais nada aqui para que a imagem no UMG se atualize.

    // Se você precisasse exibir metadados do frame (como largura, altura, ou dados de features),
    // você poderia usar FrameData.Width, FrameData.Height, FrameData.Features, etc., para
    // atualizar Text Blocks ou outros elementos da UI aqui.

    // NOVO: Dispara o Blueprint Implementable Event
    // Passamos o FrameData diretamente para o evento Blueprint.
    OnNewRealTimeFrameData(FrameData);
}
