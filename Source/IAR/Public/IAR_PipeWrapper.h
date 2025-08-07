// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "../IAR.h"
#include "Core/IAR_Types.h" // Inclui a USTRUCT FIAR_PipeSettings

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <stdio.h> // Para GetLastError
#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <sys/stat.h>   // Para mkfifo
#include <fcntl.h>      // Para open, O_RDWR, O_CREAT
#include <unistd.h>     // Para close, write
#endif

// Forward declarations para tipos OS-específicos
#if PLATFORM_WINDOWS
typedef void* IAR_NativePipeHandle; // HANDLE é void*
#else
typedef int IAR_NativePipeHandle; // file descriptor é int
#endif

// Define um valor inválido para o handle nativo
#define IAR_INVALID_NATIVE_PIPE_HANDLE ((IAR_NativePipeHandle)-1)

// Definição de LogCategory para IAR_PipeWrapper
DECLARE_LOG_CATEGORY_EXTERN(LogIARPipeWrapper, Log, All);

/**
 * @brief Encapsula um Named Pipe multiplataforma (HANDLE no Windows, file descriptor no Linux/Mac).
 * Garante thread-safety e compatibilidade multiplataforma para comunicação inter-processos.
 */
struct IAR_API IAR_PipeWrapper
{
public:

    IAR_PipeWrapper();
    ~IAR_PipeWrapper();

    /**
    * @brief Cria e abre o Named Pipe (Windows) ou FIFO (POSIX).
    * @param Settings As configurações para criar o pipe (e.g., blocking mode, duplex access).
    * @param SessionID Um ID único para esta sessão, para garantir nomes de pipe únicos.
    * @return true se o pipe foi criado e aberto com sucesso, false caso contrário.
    * Multiplataforma: Utiliza chamadas de API específicas da plataforma internamente.
    */
    bool Create(const FIAR_PipeSettings& Settings, const FString& SessionID);

    /**
     * @brief Tenta conectar-se ao Named Pipe. Este método bloqueará até que um cliente
     * (e.g., FFmpeg) se conecte ao pipe.
     * Deve ser chamado DEPOIS que o pipe é criado (com Create()) e DEPOIS que o cliente
     * que vai ler/escrever no pipe é lançado.
     * @return true se a conexão foi bem-sucedida, false caso contrário.
     * Multiplataforma: Apenas implementado para Windows (ConnectNamedPipe). FIFOs POSIX
     *                 gerenciam a conexão via open().
     */
    bool Connect(); 

    /**
     * @brief Escreve dados no pipe.
     * @param Data Ponteiro para os dados a serem escritos.
     * @param NumBytes O número de bytes a serem escritos.
     * @return O número de bytes realmente escritos, ou -1 em caso de erro.
     * Thread-safety: Operações de escrita básicas geralmente são atômicas para pequenos buffers, mas
     *                escritas maiores devem ser tratadas por uma worker thread dedicada para evitar bloqueios.
     * Multiplataforma: Utiliza chamadas de escrita específicas da plataforma.
     */
    int32 Write(const uint8* Data, int32 NumBytes);

    /**
     * @brief Fecha o pipe e libera seus recursos.
     * Para FIFOs POSIX, isso também remove o arquivo do sistema de arquivos.
     * Multiplataforma: Utiliza chamadas de fechamento específicas da plataforma.
     */
    void Close();

    /**
     * @brief Verifica se o pipe está aberto e válido.
     * @return true se o handle do pipe é válido e ele está marcado como criado/conectado.
     * Thread-safety: Acessa booleano thread-safe para o estado de conexão.
     */
    bool IsValid() const;

    /**
     * @brief Retorna o caminho completo do pipe (e.g., "\.\pipe\MyPipe" ou "/tmp/MyPipe").
     */
    FString GetFullPipeName() const;

private:

#if PLATFORM_WINDOWS
    HANDLE PipeHandle; // HANDLE para Windows Named Pipe
#elif PLATFORM_LINUX || PLATFORM_MAC
    int FileDescriptor; // Descritor de arquivo para FIFO POSIX
#endif

    FIAR_PipeSettings PipeSettings; // Armazena a configuração do pipe
    FString FullPipePath; // Caminho completo para o pipe
    bool bIsCreatedAndConnected; // Indica se o pipe foi criado e está pronto para uso

    // Desabilita cópia e atribuição para prevenir comportamento indesejado com handles do OS
    IAR_PipeWrapper(const IAR_PipeWrapper&) = delete;
    IAR_PipeWrapper& operator=(const IAR_PipeWrapper&) = delete;
};


