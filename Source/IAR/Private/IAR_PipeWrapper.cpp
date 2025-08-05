// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "IAR_PipeWrapper.h"
#include "../IAR.h"
#include "GlobalStatics.h" // Para tratamento de erros (e.g., GetLastSystemErrorDetails)
#include "Misc/Guid.h"     // Para geração de IDs únicos
#include "Misc/FileHelper.h" // Para FFileHelper::DeleteFile (usado para limpeza de FIFO POSIX)

// Definição da Categoria de Log
DEFINE_LOG_CATEGORY(LogIARPipeWrapper);

// Construtor
IAR_PipeWrapper::IAR_PipeWrapper()
    : FullPipePath(TEXT(""))
    , bIsCreatedAndConnected(false)
{
#if PLATFORM_WINDOWS
    PipeHandle = INVALID_HANDLE_VALUE;
#elif PLATFORM_LINUX || PLATFORM_MAC
    FileDescriptor = -1;
#endif
    UE_LOG(LogIARPipeWrapper, Log, TEXT("IAR_PipeWrapper: Construtor chamado."));
}

// Destrutor
IAR_PipeWrapper::~IAR_PipeWrapper()
{
    Close(); // Garante que o pipe é fechado quando o objeto é destruído
    UE_LOG(LogIARPipeWrapper, Log, TEXT("IAR_PipeWrapper: Destrutor chamado."));
}

// Criar Pipe
bool IAR_PipeWrapper::Create(const FIAR_PipeSettings& Settings, const FString& SessionID)
{
    if (IsValid())
    {
        UE_LOG(LogIARPipeWrapper, Warning, TEXT("Pipe '%s' já criado. Fechando e recriando."), *FullPipePath);
        Close();
    }

    PipeSettings = Settings;
    FString BasePipeName = Settings.BasePipeName;

    // Constrói o caminho completo do pipe.
    // Para Windows: \.\pipe\<BasePipeName><SessionID> (sem sublinhado extra)
    // Para POSIX: /tmp/<BasePipeName><SessionID> (sem sublinhado extra)
    // CORREÇÃO AQUI: Removendo o sublinhado fixo para consistência com IVR e evitar possíveis problemas de nome.
    FString UniquePipeName = FString::Printf(TEXT("%s%s"), *BasePipeName , *SessionID); 

#if PLATFORM_WINDOWS
    FullPipePath = FString::Printf(TEXT("\\\\.\\pipe\\%s"), *UniquePipeName); // Duas barras extras para escapar uma barra na string

    // Determina OpenMode baseado em bDuplexAccess
    DWORD OpenMode = PIPE_ACCESS_OUTBOUND; // Padrão para IAR: UE escreve, FFmpeg lê
    if (Settings.bDuplexAccess)
    {
        OpenMode = PIPE_ACCESS_DUPLEX; // Ambos leitura e escrita
    }

    // Determina PipeMode
    DWORD PipeMode = PIPE_TYPE_BYTE; // MUITO IMPORTANTE PARA ÁUDIO: Modo byte stream.
    if (Settings.bMessageMode)
    {
        PipeMode = PIPE_TYPE_MESSAGE; // Modo mensagem (não recomendado para áudio bruto)
    }
    if (Settings.bBlockingMode)
    {
        PipeMode |= PIPE_WAIT; // Modo bloqueante (recomendado para garantir que todos os dados sejam escritos)
    }
    else
    {
        PipeMode |= PIPE_NOWAIT; // Modo não-bloqueante (requer tratamento de erros EAGAIN/ERROR_NO_DATA)
    }

    // Cria a instância do Named Pipe
    PipeHandle = CreateNamedPipe(
        *FullPipePath,            // Nome do pipe
        OpenMode,                 // Modo de abertura
        PipeMode,                 // Modo do pipe
        Settings.MaxInstances,    // Máximo de instâncias (-1 para ilimitado, limitado a 255 pelo Windows)
        Settings.OutBufferSize,   // Tamanho do buffer de saída (para áudio)
        Settings.InBufferSize,    // Tamanho do buffer de entrada (se houver duplex)
        NMPWAIT_USE_DEFAULT_WAIT, // Timeout padrão para cliente conectar
        NULL);                    // Atributos de segurança (padrão)

    // Verifica erros na criação
    if (PipeHandle == INVALID_HANDLE_VALUE)
    {
        FIAR_SystemErrorDetails Det = UIARGlobalStatics::GetLastSystemErrorDetails();
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao criar NamedPipe %s. Erro: %d Descrição: %s"), *FullPipePath, Det.ErrorCode,*Det.ErrorDescription);
        return false;
    }

    // Define bIsCreatedAndConnected como true, pois o pipe foi criado com sucesso.
    bIsCreatedAndConnected = true;
    UE_LOG(LogIARPipeWrapper, Log, TEXT("Named Pipe Windows '%s' criado com sucesso e aguardando conexão do cliente."), *FullPipePath);

#elif PLATFORM_LINUX || PLATFORM_MAC
    // FIFOs são arquivos no sistema de arquivos. Usa um diretório temporário seguro.
    FullPipePath = FPaths::Combine(FPaths::GetTempDir(), UniquePipeName);

    // Tenta deletar o FIFO se já existir, para garantir um novo começo limpo.
    // mkfifo falharia se o arquivo já existisse.
    if (access(TCHAR_TO_UTF8(*FullPipePath), F_OK) == 0) // F_OK: testa se o arquivo existe
    {
        unlink(TCHAR_TO_UTF8(*FullPipePath)); // Remove o arquivo
        UE_LOG(LogIARPipeWrapper, Warning, TEXT("FIFO existente '%s' removido para recriação."), *FullPipePath);
    }

    // Cria o FIFO (mkfifo)
    // Permissões 0666: leitura/escrita para proprietário, grupo, outros.
    if (mkfifo(TCHAR_TO_UTF8(*FullPipePath), 0666) == -1)
    {
        // Se mkfifo falhar e o erro for EEXIST (já existe), isso pode indicar uma race condition ou erro anterior.
        // Tentamos novamente, mas em geral, unlink deveria resolver.
        if (errno == EEXIST)
        {
             UE_LOG(LogIARPipeWrapper, Warning, TEXT("FIFO '%s' já existia (race condition ou limpeza anterior falhou). Tentando abrir assim mesmo."), *FullPipePath);
        }
        else
        {
            UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao criar FIFO '%s'. Erro: %s"), *FullPipePath, UTF8_TO_TCHAR(strerror(errno)));
            return false;
        }
    }
    
    // Abre o FIFO para escrita. O_WRONLY para escrita (UE escreve), O_NONBLOCK para não bloquear no open.
    // Se bBlockingMode for true, o O_NONBLOCK será removido, o que fará open() bloquear até um leitor se conectar.
    int OpenFlags = O_WRONLY; // Padrão para saída (UE escreve)
    if (Settings.bDuplexAccess)
    {
        OpenFlags = O_RDWR; // Para acesso duplex (leitura e escrita)
    }

    if (!Settings.bBlockingMode)
    {
        OpenFlags |= O_NONBLOCK; // Opera em modo não-bloqueante
    }

    // Abre o FIFO. Isso pode bloquear se bBlockingMode for true e não houver leitor ainda.
    // O ideal é que o Connect() seja chamado logo após o lançamento do processo FFmpeg.
    UE_LOG(LogIARPipeWrapper, Log, TEXT("Abrindo FIFO '%s' para escrita... (pode bloquear se não houver leitor e em modo bloqueante)"), *FullPipePath);
    FileDescriptor = open(TCHAR_TO_UTF8(*FullPipePath), OpenFlags);

    if (FileDescriptor == -1)
    {
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao abrir FIFO '%s' para escrita. Erro: %s"), *FullPipePath, UTF8_TO_TCHAR(strerror(errno)));
        unlink(TCHAR_TO_UTF8(*FullPipePath)); // Limpa o FIFO que foi criado
        return false;
    }
    bIsCreatedAndConnected = true;
    UE_LOG(LogIARPipeWrapper, Log, TEXT("FIFO '%s' aberto com sucesso para escrita."), *FullPipePath);

#else // Outras plataformas
    UE_LOG(LogIARPipeWrapper, Error, TEXT("IAR_PipeWrapper::Create não implementado para esta plataforma."));
    return false;
#endif

    return bIsCreatedAndConnected;
}

// NOVO MÉTODO DE CONEXÃO (Específico para Windows, FIFOs POSIX gerenciam a conexão via open)
bool IAR_PipeWrapper::Connect()
{
#if PLATFORM_WINDOWS
    if (!IsValid())
    {
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Não é possível conectar. O handle do pipe é inválido para '%s'."), *FullPipePath);
        return false;
    }

    UE_LOG(LogIARPipeWrapper, Log, TEXT("Aguardando conexão do cliente para o pipe: %s"), *FullPipePath);

    // ConnectNamedPipe irá bloquear até que um cliente se conecte.
    // ERROR_PIPE_CONNECTED significa que o pipe já estava conectado (em caso de chamadas sucessivas),
    // o que é um sucesso.
    BOOL bSuccess = ConnectNamedPipe(PipeHandle, NULL);
    if (!bSuccess && GetLastError() != ERROR_PIPE_CONNECTED)
    {
        DWORD ErrorCode = GetLastError();
        FIAR_SystemErrorDetails Det;
        Det.ErrorCode = ErrorCode;
        FPlatformMisc::GetSystemErrorMessage(Det.ErrorDescription.GetCharArray().GetData(), Det.ErrorDescription.GetCharArray().Max(), ErrorCode);
        Det.ErrorDescription.TrimEndInline(); // Limpa espaços e quebras de linha
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao conectar Named Pipe '%s'. Erro: %d Descrição: %s"), *FullPipePath, Det.ErrorCode, *Det.ErrorDescription);
        CloseHandle(PipeHandle);
        PipeHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    UE_LOG(LogIARPipeWrapper, Log, TEXT("Named Pipe '%s' conectado com sucesso."), *FullPipePath);
    return true;
#else // Para POSIX, a conexão é implicitamente gerenciada pelo 'open' em 'Create'.
    UE_LOG(LogIARPipeWrapper, Log, TEXT("Connect() não é explicitamente necessário para plataformas não-Windows (FIFOs POSIX conectam via open())."));
    // Se chegamos aqui, implica que o pipe já foi aberto com sucesso em Create().
    return true;
#endif
}


// Escrever no Pipe
int32 IAR_PipeWrapper::Write(const uint8* Data, int32 NumBytes)
{
    if (!IsValid())
    {
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Tentativa de escrever em um pipe inválido ou não inicializado. (%s)"), *FullPipePath);
        return -1; // Indica erro
    }

#if PLATFORM_WINDOWS
    DWORD BytesWritten=0;
    // WriteFile pode ser bloqueante ou não-bloqueante dependendo do modo do pipe (definido em Create)
    if (!WriteFile(PipeHandle, Data, NumBytes, &BytesWritten, NULL))
    {
        DWORD ErrorCode = GetLastError();
        // Lida com ERROR_NO_DATA para escritas não-bloqueantes se o pipe estiver cheio/buffer esgotado
        if (ErrorCode == ERROR_NO_DATA || ErrorCode == ERROR_PIPE_BUSY) // ERROR_PIPE_BUSY pode ocorrer em modo não-bloqueante se o cliente não está lendo
        {
            // Isso significa que o buffer do pipe está cheio, não é possível escrever mais agora.
            // Para um pipe não-bloqueante, você pode tentar novamente ou descartar dados.
            // Para este design, esperamos escritas bloqueantes para robustez ou que o encoder leia rápido.
            UE_LOG(LogIARPipeWrapper, Warning, TEXT("Buffer do Named Pipe Windows cheio ou ocupado. Não foi possível escrever %d bytes (Erro: %d)."), NumBytes, ErrorCode);
            return 0; // Escreveu 0 bytes
        }
        FIAR_SystemErrorDetails Det = UIARGlobalStatics::GetLastSystemErrorDetails();
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao escrever no Named Pipe Windows '%s'. Erro: %d Descrição: %s"), *FullPipePath, Det.ErrorCode, *Det.ErrorDescription);
        return -1; // Indica erro
    }

    // Não precisamos verificar se BytesWritten != NumBytes aqui, pois o WriteFile retorna false em caso de escrita incompleta
    // em modo bloqueante se o pipe não for fechado, e em não-bloqueante se o buffer for insuficiente.
    return (int32)BytesWritten;

#elif PLATFORM_LINUX || PLATFORM_MAC
    ssize_t BytesWritten = write(FileDescriptor, Data, NumBytes);
    if (BytesWritten == -1)
    {
        // Para pipes não-bloqueantes se o buffer estiver cheio
        if (errno == EAGAIN || errno == EWOULDBLOCK) 
        {
            UE_LOG(LogIARPipeWrapper, Warning, TEXT("Buffer FIFO cheio. Não foi possível escrever %d bytes (Erro: %s)."), NumBytes, UTF8_TO_TCHAR(strerror(errno)));
            return 0; // Escreveu 0 bytes
        }
        UE_LOG(LogIARPipeWrapper, Error, TEXT("Falha ao escrever no FIFO '%s'. Erro: %s"), *FullPipePath, UTF8_TO_TCHAR(strerror(errno)));
        return -1; // Indica erro
    }
    return (int32)BytesWritten;

#else // Outras plataformas
    UE_LOG(LogIARPipeWrapper, Error, TEXT("IAR_PipeWrapper::Write não implementado para esta plataforma."));
    return -1;
#endif
}

// Fechar Pipe
void IAR_PipeWrapper::Close()
{
    if (!IsValid())
    {
        return; // Já fechado ou nunca foi válido
    }

#if PLATFORM_WINDOWS
    if (PipeHandle != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(PipeHandle); // Garante que todos os dados sejam escritos antes de desconectar
        DisconnectNamedPipe(PipeHandle); // Desconecta o cliente (FFmpeg)
        CloseHandle(PipeHandle);         // Fecha o handle do pipe
        PipeHandle = INVALID_HANDLE_VALUE;
        UE_LOG(LogIARPipeWrapper, Log, TEXT("Named Pipe Windows '%s' fechado."), *FullPipePath);
    }
#elif PLATFORM_LINUX || PLATFORM_MAC
    if (FileDescriptor != -1)
    {
        close(FileDescriptor); // Fecha o descritor de arquivo
        FileDescriptor = -1;
        UE_LOG(LogIARPipeWrapper, Log, TEXT("Descritor de arquivo FIFO '%s' fechado."), *FullPipePath);
    }
    // Remove o arquivo FIFO do sistema de arquivos após fechar
    if (!FullPipePath.IsEmpty() && access(TCHAR_TO_UTF8(*FullPipePath), F_OK) == 0) // Verifica se o arquivo ainda existe
    {
        unlink(TCHAR_TO_UTF8(*FullPipePath)); // Desvincula/remove o arquivo FIFO
        UE_LOG(LogIARPipeWrapper, Log, TEXT("Arquivo FIFO '%s' desvinculado do sistema de arquivos."), *FullPipePath);
    }
#else // Outras plataformas
    // No-op para plataformas não implementadas
#endif
    bIsCreatedAndConnected = false; // Marca como não criado/conectado
    FullPipePath = TEXT(""); // Limpa o caminho para evitar referências inválidas
}

bool IAR_PipeWrapper::IsValid() const
{
#if PLATFORM_WINDOWS
    return PipeHandle != INVALID_HANDLE_VALUE && bIsCreatedAndConnected;
#elif PLATFORM_LINUX || PLATFORM_MAC
    return FileDescriptor != -1 && bIsCreatedAndConnected;
#else // Outras plataformas
    return false;
#endif
}

FString IAR_PipeWrapper::GetFullPipeName() const
{
    return FullPipePath;
}
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Private\IAR_PipeWrapper.cpp ---
