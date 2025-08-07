// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h" // Para LogIAR
#include "Core/IAR_Types.h" // Para FIAR_AudioStreamSettings, FIAR_AudioConversionSettings

#include "IARECFactory.generated.h" // Deve ser o último include para UObjects/UStructs

/**
 * @brief Fábrica responsável por gerar strings de comando para ferramentas externas, como o FFmpeg.
 * Centraliza a lógica de construção de comandos para gravação e processamento de áudio.
 */
UCLASS()
class IAR_API UIARECFactory : public UObject
{
    GENERATED_BODY()

public:
    UIARECFactory(); // Construtor
    virtual ~UIARECFactory(); // Destrutor (para limpeza de não-UObjects)

    // Funções de exemplo, a serem substituídas pela implementação real
    void Initialize();
    void Shutdown();

    /**
     * @brief Constrói o comando FFmpeg para codificar um stream de áudio bruto (PCM)
     * lido de um pipe e salvá-lo em um arquivo.
     * @param StreamSettings As configurações do stream de áudio (codec, sample rate, canais, bitrate).
     * @param InputPipeName O nome completo do Named Pipe/FIFO de onde o FFmpeg deve ler o áudio bruto.
     * @param OutputFilePath O caminho completo para o arquivo de saída.
     * @return A string de comando FFmpeg completa.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder Commands")
    static FString BuildAudioEncodeCommand(const FIAR_AudioStreamSettings& StreamSettings, const FString& InputPipeName, const FString& OutputFilePath);

    /**
     * @brief Constrói o comando para encerrar um processo pelo seu ID.
     * @param ProcessId O ID do processo a ser encerrado.
     * @return A string de comando para encerrar o processo.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|System Commands")
    static FString BuildKillProcessCommand(int32 ProcessId);

    /**
     * @brief Constrói o comando FFmpeg para converter um arquivo de áudio para outro formato.
     * @param InSourceAudioPath Caminho para o arquivo de áudio de entrada.
     * @param OutConvertedAudioPath Caminho para o arquivo de áudio de saída.
     * @param ConversionSettings Configurações para a conversão (codec, bitrate, sample rate, canais).
     * @return A string de comando FFmpeg completa para a conversão.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Encoder Commands")
    static FString BuildAudioConversionCommand(const FString& InSourceAudioPath, const FString& OutConvertedAudioPath, const FIAR_AudioConversionSettings& ConversionSettings);

    // Notas sobre Thread-Safety:
    // - Use FThreadSafeBool, FThreadSafeCounter, TQueue para estado e comunicação thread-safe.
    // - Seções críticas/mutexes (FScopeLock) estão disponíveis, mas minimize seu uso para abordagens lock-free sempre que possível.
    // - Operações em UObjects a partir de threads que não sejam a Game Thread exigem cuidado específico (e.g., usando AsyncTask ou EnqueueRenderCommand).
    
    // Notas sobre Multiplataforma:
    // - Use FPlatformProcess, FPaths, IFileManager, FPlatformFileManager para operações agnósticas do OS.
    // - Compilação condicional (#if PLATFORM_WINDOWS) para APIs específicas da plataforma (ex: API Win32 para named pipes).

    // Notas sobre Lock-Free:
    // - Busque algoritmos que não exijam locks explícitos (ex: Produtor/Consumidor com TQueue).
    // - Utilize operações atômicas (std::atomic) para atualizações simples de variáveis entre threads.
};
