// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h" // Para LogIAR
#include "Engine/Texture2D.h" // Para UTexture2D
#include "Blueprint/UserWidget.h" // Essencial para UMG
#include "Components/IARAudioComponent.h" // Inclui para poder referenciar UIARAudioComponent

#include "IARDisplayWidget.generated.h" // Deve ser o último include para UObjects/UStructs

/**
 * @brief Widget de UI base para exibir informações e visualizações do plugin IAR.
 * Destinado a servir como exemplo para os usuários conectarem dados de features em tempo real.
 */
UCLASS()
class IAR_API UIARDisplayWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UIARDisplayWidget(const FObjectInitializer& ObjectInitializer);
    virtual ~UIARDisplayWidget();

    // Métodos de ciclo de vida do widget (boas práticas)
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- PROPRIEDADE E FUNÇÃO PARA RECEBER A TEXTURA DO ESPECTROGRAMA ---
    UPROPERTY(BlueprintReadWrite, Category = "IAR|Display")
    UTexture2D* CurrentSpectrogramTexture; // Textura que será exibida (geralmente ligada a um UImage no BP)

    // --- PROPRIEDADE E FUNÇÃO PARA RECEBER A TEXTURA DO Waveform ---
    UPROPERTY(BlueprintReadWrite, Category = "IAR|Display")
    UTexture2D* CurrentWaveFormTexture; // Textura que será exibida (geralmente ligada a um UImage no BP)

    // --- PROPRIEDADE E FUNÇÃO PARA RECEBER A TEXTURA DO Waveform ---
    UPROPERTY(BlueprintReadWrite, Category = "IAR|Display")
    UTexture2D* CurrentFilterTexture; // Textura que será exibida (geralmente ligada a um UImage no BP)

    /**
     * @brief Atualiza a textura do espectrograma exibida no widget.
     * Esta função é para o usuário chamar, passando a textura do FIAR_JustRTFrame.
     * @param NewTexture A nova textura UTexture2D contendo os dados do espectrograma.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Display")
    void UpdateSpectrogram(UTexture2D* NewTexture);

    /**
     * @brief Atualiza a textura do espectrograma exibida no widget.
     * Esta função é para o usuário chamar, passando a textura do FIAR_JustRTFrame.
     * @param NewTexture A nova textura UTexture2D contendo os dados do espectrograma.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Display")
    void UpdateWaveform(UTexture2D* NewTexture);

    /**
     * @brief Atualiza a textura do espectrograma exibida no widget.
     * Esta função é para o usuário chamar, passando a textura do FIAR_JustRTFrame.
     * @param NewTexture A nova textura UTexture2D contendo os dados do espectrograma.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Display")
    void UpdateFilter(UTexture2D* NewTexture);

    /**
     * @brief Define a referência para o componente de áudio que está emitindo os dados.
     * @param InAudioComponent O componente UIARAudioComponent que este widget deve referenciar.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Display")
    void SetAudioComponentReference(UIARAudioComponent* InAudioComponent);

    // NOVO: Blueprint Implementable Event para retornar o FIAR_JustRTFrame completo
    /**
     * @brief Evento disparado quando um novo FIAR_JustRTFrame é recebido e processado.
     * Pode ser implementado em Blueprint para reagir aos dados em tempo real.
     * @param FrameData O frame de áudio em tempo real contendo dados brutos, features e texturas.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "IAR|Display Events", DisplayName = "On New Real-Time Audio Frame Data")
    void OnNewRealTimeFrameData(const FIAR_JustRTFrame& FrameData);


private:

    // Referência privada para o componente de captura, para gerenciar a inscrição/desinscrição.
    UPROPERTY()
    UIARAudioComponent* OwningAudioComponent;

    // Manipulador para o delegate OnRealTimeFrameReady do IVRCaptureComponent.
    // Este método receberá a estrutura FIVR_JustRTFrame que contém a LiveTexture.
    UFUNCTION()
    void OnRealTimeFrameReadyHandler(const FIAR_JustRTFrame& FrameData);
};

