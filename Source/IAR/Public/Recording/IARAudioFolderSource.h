// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "IARAudioSource.h" // Herda de UIARMediaSource para integração no pipeline
#include "Core/IAR_Types.h" // Contém structs FIAR_AudioStreamSettings, FIAR_MIDIEvent, etc.
#include "Core/IARFramePool.h" // Para gerenciamento de memória de frames (se necessário)
#include <string> // <<-- ADICIONADO: Necessário para std::string em ConvertMIDIToAudio/ConvertAudioToMIDI
#include "HAL/ThreadSafeBool.h"
#include "IARAudioFolderSource.generated.h" // <<-- ESTE INCLUIR DEVE SER O ÚLTIMO NO ARQUIVO .h


// Componentes de dependência (Forward Declarations)
// Para que o UIARFolderSource.h possa ser compilado sem incluir todos os headers pesados
class UIARAudioEncoder;
class UIARAudioToMIDITranscriber;
class UIARMIDIToAudioSynthesizer;
class UIARMIDIFileSource;
class UIARFeatureProcessor; // <-- Adicionei aqui, pois UIARBasicAudioFeatureProcessor herda dele

// Delegates para feedback de progresso e conclusão
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFolderProcessingCompleted, const FString&, OutputFolderPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFolderProcessingError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFolderProcessingProgress, const FString&, CurrentFileName, float, ProgressRatio);

/**
 * @brief UIARFolderSource: Uma fonte de mídia para processamento em lote de arquivos de áudio/MIDI em diretórios.
 * Lê arquivos de entrada, converte entre áudio (.wav/.mp3) e MIDI (.mid), e salva em um diretório de saída.
 * Projetado para a criação de datasets para pesquisa em MIR, garantindo alta qualidade de áudio (.wav).
 */
UCLASS(BlueprintType, meta=(DisplayName="IAR Folder Media Source"))
class IAR_API UIARFolderSource : public UIARMediaSource
{
    GENERATED_BODY()

public:
    // Construtor
    UIARFolderSource();
    // Destrutor virtual para garantir limpeza adequada
    virtual ~UIARFolderSource();

    /**
     * @brief Inicializa a fonte de diretório, escaneando o diretório de entrada.
     * @param StreamSettings Configurações gerais de áudio/mídia, incluindo tipo de conteúdo.
     * @param InFramePool Referência opcional ao FramePool.
     */
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool) override;

    /**
     * @brief Inicia o processamento em lote dos arquivos no diretório de entrada.
     * Esta operação é assíncrona e não bloqueará a Game Thread.
     */
    virtual void StartCapture() override;

    /**
     * @brief Sinaliza para interromper o processamento em lote (se estiver em andamento).
     */
    virtual void StopCapture() override;

    /**
     * @brief Desliga e limpa os recursos da fonte de diretório.
     */
    virtual void Shutdown() override;

    // --- Propriedades Configuráveis no Blueprint ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Folder Source Settings",
              meta = (DisplayName = "Input Folder Path", Tooltip = "Caminho absoluto ou relativo para a pasta contendo os arquivos de mídia de entrada (.wav, .mp3, .mid)."))
    FString InputFolderPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Folder Source Settings",
              meta = (DisplayName = "Output Folder Path", Tooltip = "Caminho absoluto ou relativo para a pasta onde os arquivos convertidos serão salvos. A saída de áudio será sempre em .wav."))
    FString OutputFolderPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Folder Source Settings",
              meta = (DisplayName = "Overwrite Existing Files", Tooltip = "Se verdadeiro, arquivos de saída existentes com o mesmo nome serão sobrescritos. Caso contrário, serão ignorados."))
    bool bOverwriteExistingFiles = false;

    // --- Delegates para Feedback de Progresso e Conclusão ---
    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Source Events")
    FOnFolderProcessingCompleted OnFolderProcessingCompleted;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Source Events")
    FOnFolderProcessingError OnFolderProcessingError;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Folder Source Events")
    FOnFolderProcessingProgress OnFolderProcessingProgress;

protected:
    // --- Funções Auxiliares de Lógica para callbacks ---
    // Estes métodos são UFUNCTIONs para serem usados como handlers de delegates do UIARAudioComponent
    UFUNCTION()
    void HandleFolderProcessingCompleted(const FString& pOutputFolderPath);

    UFUNCTION()
    void HandleFolderProcessingError(const FString& ErrorMessage);

    UFUNCTION()
    void HandleFolderProcessingProgress(const FString& CurrentFileName, float ProgressRatio);

private:
    // --- Membros Internos para Gerenciamento de Processamento ---
    TArray<FString>      FilesToProcess;    // Lista de caminhos absolutos dos arquivos a serem processados
    FThreadSafeCounter  CurrentFileIndex;   // Índice do arquivo atual sendo processado (para progresso e controle)
    FThreadSafeBool     bProcessingActive;   // Flag atômica para indicar/controlar o estado ativo do processamento assíncrono

     // NOVO: Adicione estas duas propriedades para gerenciar o transcritor e o feature processor
    UPROPERTY()
    TObjectPtr<UIARAudioToMIDITranscriber> Transcriber; // <-- Agora é um membro da classe

    UPROPERTY()
    TObjectPtr<UIARFeatureProcessor> FeatureProcessor; // <-- Agora é um membro da classe
                                                      //     Use UIARFeatureProcessor pois você pode ter o Basic ou Advanced
    // --- Funções Auxiliares de Lógica ---

    /**
     * @brief O método principal de execução da tarefa assíncrona de processamento de arquivos.
     * Itera sobre a lista de arquivos, determinando o tipo de conversão e delegando.
     */
    void ProcessFileConversionAsyncTask();

    /**
     * @brief Converte um arquivo de áudio (.wav ou .mp3) para MIDI (.mid).
     * Decodifica áudio, transcreve para MIDI e salva o arquivo MIDI.
     * @param AudioFilePath Caminho completo do arquivo de áudio de entrada.
     * @param MIDIOutputFilePath Caminho completo para salvar o arquivo MIDI de saída.
     * @return true se a conversão foi bem-sucedida, false caso contrário.
     */
    bool ConvertAudioToMIDI(const FString& AudioFilePath, const FString& MIDIOutputFilePath);

    /**
     * @brief Converte um arquivo MIDI (.mid) para áudio (.wav).
     * Carrega o MIDI, sintetiza o áudio e salva o arquivo WAV.
     * A saída de áudio será sempre em formato .wav sem perdas.
     * @param MIDIFilePath Caminho completo do arquivo MIDI de entrada.
     * @param AudioOutputFilePath Caminho completo para salvar o arquivo de áudio WAV de saída.
     * @return true se a conversão foi bem-sucedida, false caso contrário.
     */
    bool ConvertMIDIToAudio(const FString& MIDIFilePath, const FString& AudioOutputFilePath);

    /**
     * @brief Constrói o caminho completo do arquivo de saída, determinando a extensão correta.
     * @param InputFilePath Caminho do arquivo de entrada.
     * @param bIsAudioToMIDIConversion Se verdadeiro, a extensão de saída será .mid; caso contrário, .wav.
     * @return O caminho completo do arquivo de saída com a extensão apropriada.
     */
    FString GetOutputFilePath(const FString& InputFilePath, bool bIsAudioToMIDIConversion) const;
};
