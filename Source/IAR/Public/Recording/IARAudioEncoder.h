// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h"
#include "Core/IAR_Types.h"      // Para FIAR_AudioStreamSettings, FIAR_AudioFrameData, FIAR_AudioConversionSettings
#include "IAR_PipeWrapper.h"   // Para IAR_PipeWrapper
#include "IARECFactory.h"      // Para UIARECFactory
#include "HAL/Runnable.h"       // Para FRunnable (worker thread)
#include "HAL/RunnableThread.h"  // Para FRunnableThread
#include "Containers/Queue.h"    // Para TQueue (thread-safe)
#include "Misc/ScopeLock.h"      // Para FScopeLock
#include "Core/IARFramePool.h"   // ADICIONADO: Include para UIARFramePool
#include "FFmpegLogReader.h"     // ADICIONADO: Include para FFMpegLogReader
#include <string> // <<-- ADICIONADO: Necessário para std::string em EncodeRawPCMToFile, ConvertMIDIToAudio
#include "TimerManager.h"

#include "IARAudioEncoder.generated.h"
// --- IMPORTANTE: NÃO INCLUIR dr_wav.h AQUI NO .h ---

// Definição do LogCategory para IAR_PipeWrapper
DECLARE_LOG_CATEGORY_EXTERN(LogIARAudioEncoder, Log, All);

// Declaração do delegate para notificar o sucesso/falha da gravação
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioEncodingFinished, const FString&, OutputFilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioEncodingError, const FString&, ErrorMessage);

// NOVOS DELEGATES PARA FEEDBACK DE CONGESTIONAMENTO DO PIPE
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioPipeCongested, const FString&, OutputFilePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioPipeCleared, const FString&, OutputFilePath);


// DECLARAÇÃO: FIARAudioEncoderWorker.
// Apenas a declaração da classe e seus protótipos de método.
class FIARAudioEncoderWorker : public FRunnable
{
public:
    // Construtor
    FIARAudioEncoderWorker(IAR_PipeWrapper& InPipeWrapper, TQueue<TArray<uint8>, EQueueMode::Mpsc>& InDataQueue, FEvent* InDataAvailableEvent, FThreadSafeBool& InShouldStop);
    
    // IMPORTANTE: O destrutor virtual é crucial para a liberação correta de recursos de classes base.
    virtual ~FIARAudioEncoderWorker();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    /** Enfileira dados para serem escritos no pipe. */
    void EnqueueData(TArray<uint8>&& Data);
    /** Sinaliza para o worker thread parar de processar e sair. */
    void StopProcessing(); // Apenas um wrapper para Stop()
    
    // NOVO: Getter para o status de congestionamento do pipe.
    bool IsPipeCurrentlyCongested() const;

private:
    // Membros da classe worker.
    // Usamos referências (&) para evitar cópias e garantir que trabalhamos com os objetos reais.
    IAR_PipeWrapper& PipeWrapper; // Referência para o pipe
    TQueue<TArray<uint8>, EQueueMode::Mpsc>& DataQueue; // Referência para a fila de dados de áudio a serem escritos
    FThreadSafeBool& bShouldStop;    // Referência para a flag de sinalização de parada da thread
    FEvent* DataAvailableEvent; // Referência para o evento para sinalizar que há dados na fila

    // NOVO: Flag atômica para indicar se o pipe está atualmente congestionado
    FThreadSafeBool bIsPipeCurrentlyCongested;
};


/**
 * @brief Gerencia a codificação de áudio para um arquivo usando um processo externo (FFmpeg).
 * Utiliza Named Pipes/FIFOs para comunicação e uma worker thread para escrita eficiente.
 */
UCLASS()
class IAR_API UIARAudioEncoder : public UObject
{
    GENERATED_BODY()

public:
    // ATENÇÃO: UIARECFactory deve ser UPROPERTY para ser gerenciado pelo GC
    UPROPERTY()
    UIARECFactory* EncoderCommandFactory; // Fábrica para gerar comandos FFmpeg

    // Caminho completo para o executável do FFmpeg
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|Encoder Settings")
    FString FFmpegExecutablePath; // Agora gerado automaticamente, mas pode ser sobrescrito pelo BP

    UIARAudioEncoder();
    virtual ~UIARAudioEncoder();

    // Sobrescreve o BeginDestroy para garantir limpeza de recursos quando o UObject é destruído
    virtual void BeginDestroy() override;

     /**
     * @brief Inicializa o codificador de áudio, configura o pipe de entrada e o worker thread.
     * @param Settings Configurações de áudio para a codificação.
     * @param InFFmpegExecutablePath Caminho para o executável FFmpeg.
     * @param InActualFrameWidth Largura real dos frames que serão processados. (Não usado para áudio, manter para compatibilidade).
     * @param InActualFrameHeight Altura real dos frames que serão processados. (Não usado para áudio, manter para compatibilidade).
     * @param InFramePool Referência ao pool de frames para gerenciamento de buffers. 
     * @return true se a inicialização foi bem-sucedida, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder")
    bool Initialize(const FIAR_AudioStreamSettings& Settings, const FString& InFFmpegExecutablePath, int32 InActualFrameWidth, int32 InActualFrameHeight, UIARFramePool* InFramePool); 

    /**
     * @brief Lança o processo principal do FFmpeg para iniciar a gravação.
     * @param LiveOutputFilePath Caminho completo para o arquivo de áudio de saída.
     * @return true se o processo FFmpeg foi lançado com sucesso, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder")
    bool LaunchEncoder(const FString& LiveOutputFilePath);

    /**
     * @brief Encerra o codificador e limpa todos os recursos (pipes, processo FFmpeg, threads).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder")
    void ShutdownEncoder();

    /**
    * @brief Adiciona um frame de áudio à fila de codificação.
    * @param Frame O frame de áudio a ser codificado.
    * @return true se o frame foi adicionado com sucesso, false caso contrário.
    */
    // AGORA RECEBE TSharedPtr<FIAR_AudioFrameData>
    bool EncodeFrame(TSharedPtr<FIAR_AudioFrameData> Frame);

    /**
     * @brief Sinaliza que não haverá mais frames para codificar e aguarda a conclusão da escrita no pipe.
* Isso fecha o pipe de entrada e sinaliza EOF ao FFmpeg.
     * @return true se a finalização foi bem-sucedida, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder")
    bool FinishEncoding();

    /**
     * @brief Executa um processo FFmpeg em um background thread e aguarda sua conclusão.
     * Esta função é bloqueante para o thread que a chama, mas não para a Game Thread se chamada via AsyncTask.
     * @param ExecPath Caminho completo para o executável FFmpeg.
     * @param Arguments Argumentos da linha de comando para o FFmpeg.
     * @return true se o processo foi executado com sucesso e retornou código 0, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder")
    static bool LaunchBlockingFFmpegProcess(const FString& ExecPath, const FString& Arguments);

    /**
     * @brief Decodifica um arquivo de áudio de entrada para dados PCM brutos (s16le) usando FFmpeg.
* @param InputFilePath Caminho completo para o arquivo de áudio de entrada no disco.
     * @param TargetSampleRate A taxa de amostragem desejada para o PCM de saída.
     * @param TargetNumChannels O número de canais desejado para o PCM de saída.
     * @param OutRawPCMData O TArray<uint8> onde os dados PCM brutos decodificados serão armazenados.
     * @return true se a decodificação foi bem-sucedida, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder",
              meta = (DisplayName = "Decode Audio File to Raw PCM",
                      Tooltip = "Decodes an audio file from disk to raw 16-bit signed little-endian PCM using FFmpeg."))
    static bool DecodeAudioFileToRawPCM(const FString& InputFilePath, int32 TargetSampleRate, int32 TargetNumChannels, TArray<uint8>& OutRawPCMData);

    /**
     * @brief Decodifica um arquivo WAV de entrada para dados PCM brutos (float) usando a biblioteca dr_wav.
* Esta função é uma alternativa ao FFmpeg para decodificar arquivos WAV.
     * @param InputFilePath Caminho completo para o arquivo WAV de entrada no disco.
     * @param OutRawPCMData O TArray<float> onde os dados PCM brutos decodificados serão armazenados.
     * @param OutSampleRate A taxa de amostragem do áudio decodificado.
     * @param OutNumChannels O número de canais do áudio decodificado.
     * @return true se a decodificação foi bem-sucedida, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder",
              meta = (DisplayName = "Decode Wave File to Raw PCM (DrWav)",
                      Tooltip = "Decodes a WAV file from disk to raw float PCM using the dr_wav library."))
    static bool DecodeWaveFileToRawPCM_DrWav(const FString& InputFilePath, TArray<float>& OutRawPCMData, int32& OutSampleRate, int32& OutNumChannels);

    /**
     * @brief Codifica dados PCM brutos em float para um arquivo de áudio especificado (ex: WAV) usando dr_wav.
     * @param RawPCMData O TArray<float> contendo os dados PCM brutos normalizados entre -1.0 e 1.0.
     * @param SampleRate A taxa de amostragem do áudio.
     * @param NumChannels O número de canais do áudio.
     * @param OutputFilePath O caminho completo para o arquivo de saída (ex: "C:/path/output.wav").
     * @param BitDepth A profundidade de bits desejada para a saída (ex: 16 para PCM_S16LE). Apenas 16-bit suportado na implementação atual.
     * @return true se a codificação foi bem-sucedida, false caso contrário.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder",
              meta = (DisplayName = "Encode Raw PCM to File",
                      Tooltip = "Encodes raw float PCM data to a specified audio file format (e.g., WAV) using dr_wav."))
    static bool EncodeRawPCMToFile(const TArray<float>& RawPCMData, int32 SampleRate, int32 NumChannels, const FString& OutputFilePath, int32 BitDepth = 16);

    UFUNCTION(BlueprintPure, Category = "IAR")
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * @brief Verifica se a codificação está ativa.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Audio Encoder")
    bool IsEncodingActive() const { return bIsEncodingActive; }

    /**
     * @brief Retorna o caminho completo do arquivo de saída que este encoder está gravando.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|Audio Encoder")
    FString GetCurrentOutputFilePath() const { return CurrentOutputFilePath; }

    // Delegates para notificação
    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Encoder|Delegates")
    FOnAudioEncodingFinished OnAudioEncodingFinished;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Encoder|Delegates")
    FOnAudioEncodingError OnAudioEncodingError;

    // NOVOS DELEGATES PARA FEEDBACK DE CONGESTIONAMENTO DO PIPE
    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Encoder|Delegates")
    FOnAudioPipeCongested OnAudioPipeCongested;

    UPROPERTY(BlueprintAssignable, Category = "IAR|Audio Encoder|Delegates")
    FOnAudioPipeCleared OnAudioPipeCleared;

    // Declaração de GetFFmpegExecutablePathInternal como public para acesso de UIARAudioComponent
    // Agora é um método const static para evitar criar um objeto Default e permitir fácil acesso.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|Encoder")
    static FString GetFFmpegExecutablePathInternal(); 

    UIARFramePool* GetFramePool() { return FramePool; } // Getter para o FramePool

private:
    FIAR_AudioStreamSettings CurrentStreamSettings;
    FString CurrentOutputFilePath; // Mantenha como private, o getter é público.

    IAR_PipeWrapper AudioPipe; // O pipe para enviar dados brutos ao FFmpeg
    
    // Ponteiros para os leitores de log do FFmpeg e seus pipes.
    FFMpegLogReader* FFmpegStdoutLogReader; 
    FFMpegLogReader* FFmpegStderrLogReader; 
    void* FFmpegReadPipeStdout;       
    void* FFmpegWritePipeStdout;      
    void* FFmpegReadPipeStderr;       
    void* FFmpegWritePipeStderr;      

    FProcHandle FFmpegProcessHandle; // Handle para o processo FFmpeg
    uint32 FFmpegProcessId; // ID do processo FFmpeg

    // NOTE: EncoderWorker é um ponteiro para a classe FIARAudioEncoderWorker, que é o FRunnable.
    // O FRunnableThread espera um FRunnable*, então a conversão implícita é válida.
    FIARAudioEncoderWorker* EncoderWorker; 
    FRunnableThread* EncoderWorkerThread; // A thread de execução

    TQueue<TArray<uint8>, EQueueMode::Mpsc> DataQueue; // Fila de dados de áudio para o worker thread

    bool bIsEncodingActive; // Flag para controlar o estado da codificação
    
    FThreadSafeBool bStopWorkerThread; // Flag atômica para sinalizar ao worker thread para parar
    FThreadSafeBool bNoMoreFramesToEncode; // Flag atômica para sinalizar que todos os frames foram submetidos (não haverá mais Enqueue)
    FThreadSafeBool bIsInitialized; // Estado interno para controle de inicialização (FThreadSafeBool implicitamente conversível para bool)
    FThreadSafeBool bIsPipeCurrentlyCongestedInternal; // NOVO: Flag interna para o Encoder para evitar broadcast repetitivo.
    
    FEvent* NewFrameEvent; // Evento para sinalizar que novos frames estão disponíveis ou que a thread deve verificar o estado (shutdown/finish)
    FTimerHandle PipeCongestionCheckTimerHandle; // NOVO: Timer para verificar o status de congestionamento do pipe.

     // Largura e altura reais dos frames a serem processados (podem ser 0 para áudio)
    int32 ActualProcessingWidth;
    int32 ActualProcessingHeight;

    UPROPERTY() 
    UIARFramePool* FramePool; // Referência ao pool de frames
    
    /**
     * @brief Função auxiliar para limpar os recursos do encoder (terminar FFmpeg e fechar pipe).
     */
    void InternalCleanupEncoderResources();

    // NOVO: Método para verificar o status de congestionamento do pipe.
    void CheckPipeCongestionStatus();
};
