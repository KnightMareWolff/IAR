// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "IARAudioSource.h" 
#include "Core/IARFramePool.h" // Incluído para poder referenciar IARAudioFileSource
#include "TimerManager.h"

#include "IARAudioFileSource.generated.h"

/**
 * @brief Fonte de áudio que lê samples de um arquivo de áudio (ex: .wav).
 * Alimenta o pipeline com FIAR_AudioFrameData lidos do arquivo.
 */
UCLASS(BlueprintType)
class IAR_API UIARAudioFileSource : public UIARAudioSource // Herda de UIARAudioSource
{
    GENERATED_BODY()

public:
    UIARAudioFileSource();
    virtual ~UIARAudioFileSource();

    /**
     * @brief Inicializa a fonte de áudio de arquivo.
     * @param StreamSettings As configurações do stream de áudio (SampleRate, NumChannels, FilePath).
     * @param InFramePool O FramePool a ser utilizado para aquisição de frames.
     */
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia a leitura e envio de frames do arquivo.
     */
    virtual void StartCapture() override;

    /**
     * @brief Para a leitura de frames do arquivo.
     */
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de áudio.
     */
    virtual void Shutdown() override;

    /**
     * @brief Verifica se o arquivo de áudio foi carregado com sucesso.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio File Source")
    bool IsFileLoaded() const { return bIsFileLoaded; }

    /**
     * @brief Reseta o estado da fonte de arquivo de áudio, liberando os dados carregados.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio File Source")
    void ResetFileSource();

    // --- INÍCIO DA ALTERAÇÃO ESPECÍFICA DO PROBLEMA LATENTE ---
    /**
     * @brief Carrega o arquivo de áudio do disco de forma bloqueante (para ser executado em uma thread de background).
     * Tornada pública para ser acessada pela ação latente.
     * @return true se o carregamento e a configuração foram bem-sucedidos.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio File Source|Internal") // Adicionado UFUNCTION e categoria
    bool Internal_LoadAudioFileBlocking(); 
    // --- FIM DA ALTERAÇÃO ESPECÍFICA DO PROBLEMA LATENTE ---

    void ProcessFileFrame();

public: // Alterado de private para public para acesso da ação latente
    // Armazena o caminho absoluto completo do arquivo no disco
    FString FullDiskFilePathInternal; 

private:
    TArray<float> RawAudioSamples; // Samples do arquivo de áudio convertidos para float

    int32 CurrentSampleIndex; // Índice do sample atual sendo lido no RawAudioSamples
    int32 NumSamplesPerFrame; // Número de samples a serem processados por frame

    FTimerHandle AudioFileProcessTimerHandle; // Timer para controlar a leitura do arquivo

    bool bIsFileLoaded = false; // Indica se o arquivo foi carregado com sucesso
};
