// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioEncoder.h" // Deve ser sempre o primeiro include para o .cpp do UObject
#include "../IAR.h"
#include "HAL/PlatformProcess.h" // Para FPlatformProcess::CreateProc, TerminateProc, IsProcRunning
#include "Misc/Paths.h"          // Para FPaths::ProjectDir, FPaths::Combine
#include "Misc/Guid.h"           // Para FGuid::NewGuid()
#include "HAL/PlatformFileManager.h" // Para IPlatformFileManager
#include "Async/Async.h" // Para AsyncTask
#include "FFmpegLogReader.h"     // ADICIONADO: Include para FFMpegLogReader
#include "Engine/World.h"
#include "dr_wav.h" // Inclua o cabeçalho do dr_wav SEM A MACRO!!!

// Definição do LogCategory
DEFINE_LOG_CATEGORY(LogIARAudioEncoder);

// ==============================================================================
// ==============================================================================
// FIARAudioEncoderWorker Implementation (Implementações dos métodos da worker class)
// ==============================================================================

// Construtor do worker
FIARAudioEncoderWorker::FIARAudioEncoderWorker(IAR_PipeWrapper& InPipeWrapper, TQueue<TArray<uint8>, EQueueMode::Mpsc>& InDataQueue, FEvent* InDataAvailableEvent, FThreadSafeBool& InShouldStop)
    : PipeWrapper(InPipeWrapper) // Membro de referência: inicialização direta
    , DataQueue(InDataQueue)     // Membro de referência: inicialização direta
    , bShouldStop(InShouldStop)  // Membro de referência: inicialização direta
    , DataAvailableEvent(InDataAvailableEvent) // Membro de ponteiro: inicialização direta
    , bIsPipeCurrentlyCongested(false) // Inicializa a flag de congestionamento
{
}

// Destrutor do worker
FIARAudioEncoderWorker::~FIARAudioEncoderWorker()
{
    // Não precisa retornar DataAvailableEvent aqui, ele é de posse da UIARAudioEncoder
    // e será retornado no ShutdownEncoder.
    // Apenas garante que a thread não está esperando antes de destruir o objeto.
    if (DataAvailableEvent) { DataAvailableEvent->Trigger(); } 
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Destrutor chamado."));
}

bool FIARAudioEncoderWorker::Init()
{
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Inicializado."));
    return true;
}

uint32 FIARAudioEncoderWorker::Run()
{
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Iniciando loop de thread."));

    // A chamada para conectar o pipe agora acontece na thread do worker, não na Game Thread.
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Aguardando conexão do FFmpeg com o pipe de entrada de áudio..."));
    if (!PipeWrapper.Connect())
    {
        // Se a conexão falhar, loga o erro e sinaliza para a thread parar imediatamente.
        UE_LOG(LogIAR, Error, TEXT("FIARAudioEncoderWorker: Falha ao conectar Audio Named Pipe ao FFmpeg na thread do worker. Abortando codificação."));
        bShouldStop.AtomicSet(true); // Sinaliza para parar imediatamente
        return 0; // Sai da função Run(), terminando a thread do worker.
    }
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Pipe de entrada de áudio conectado com sucesso na thread do worker."));
// O loop principal da worker thread (onde os dados são escritos no pipe) começa aqui.
    while (!bShouldStop) // FThreadSafeBool implicitamente conversível para bool
    {
        TArray<uint8> DataToSend;
        // Tenta dequeuar dados da fila. Se não houver, espera pelo evento.
        if (DataQueue.Dequeue(DataToSend))
        {
            if (PipeWrapper.IsValid())
            {
                int32 BytesWritten = PipeWrapper.Write(DataToSend.GetData(), DataToSend.Num());
                if (BytesWritten == 0) // Pipe cheio/ocupado
                {
                    if (!bIsPipeCurrentlyCongested) // Se acabou de ficar congestionado
                    {
                        bIsPipeCurrentlyCongested.AtomicSet(true);
                        UE_LOG(LogIAR, Warning, TEXT("FIARAudioEncoderWorker: Pipe '%s' cheio/ocupado. Pausando escrita e sinalizando congestionamento."), *PipeWrapper.GetFullPipeName());
                    }
                    FPlatformProcess::Sleep(0.001f); // Pequena pausa antes de re-tentar
                }
                else if (BytesWritten > 0) // Dados escritos com sucesso
                {
                    if (bIsPipeCurrentlyCongested) // Se estava congestionado e agora conseguiu escrever
                    {
                        bIsPipeCurrentlyCongested.AtomicSet(false);
                        UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Pipe '%s' liberado. Retomando escrita e sinalizando liberação."), *PipeWrapper.GetFullPipeName());
                    }
                }
                else if (BytesWritten == -1) // Erro fatal
                {
                    UE_LOG(LogIAR, Error, TEXT("FIARAudioEncoderWorker: Erro fatal ao escrever no pipe '%s'. Encerrando worker."), *PipeWrapper.GetFullPipeName());
                    bShouldStop.AtomicSet(true); // Usar AtomicSet() para modificar FThreadSafeBool
                }
            }
            else
            {
                UE_LOG(LogIAR, Warning, TEXT("FIARAudioEncoderWorker: Pipe inválido. Descartando dados e encerrando worker."));
                bShouldStop.AtomicSet(true); // Se o pipe não é válido, não adianta continuar
            }
        }
        else
        {
            // Se a fila está vazia e a thread não deve parar, espera por novos dados.
            if (!bShouldStop) // FThreadSafeBool implicitamente conversível para bool
            {
                DataAvailableEvent->Wait(100); // Espera por 100ms, ou até ser sinalizado
            }
        }
    }
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Loop de thread encerrado."));
    return 0;
}

void FIARAudioEncoderWorker::Stop()
{
    bShouldStop.AtomicSet(true);
    if (DataAvailableEvent) { DataAvailableEvent->Trigger(); } // Sinaliza o evento para tirar a thread da espera
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Sinalizando parada da thread."));
}

void FIARAudioEncoderWorker::Exit()
{
    // Limpa quaisquer dados restantes na fila
    while (!DataQueue.IsEmpty())
    {
        TArray<uint8> RemainingData;
        DataQueue.Dequeue(RemainingData);
        UE_LOG(LogIAR, Warning, TEXT("FIARAudioEncoderWorker: Descartando %d bytes de dados restantes na fila ao sair."), RemainingData.Num());
    }
    UE_LOG(LogIAR, Log, TEXT("FIARAudioEncoderWorker: Exit chamado."));
}

void FIARAudioEncoderWorker::EnqueueData(TArray<uint8>&& Data)
{
    DataQueue.Enqueue(MoveTemp(Data));
    if (DataAvailableEvent) { DataAvailableEvent->Trigger(); } // Sinaliza que novos dados estão disponíveis
}

void FIARAudioEncoderWorker::StopProcessing()
{
    Stop(); // Chama o método Stop do FRunnable
}

bool FIARAudioEncoderWorker::IsPipeCurrentlyCongested() const
{
    return bIsPipeCurrentlyCongested;
}

// ==============================================================================
// UIARAudioEncoder Implementation (Implementações dos métodos da classe UObject)
// ==============================================================================

UIARAudioEncoder::UIARAudioEncoder()
    : FFmpegStdoutLogReader(nullptr)
    , FFmpegStderrLogReader(nullptr)
    , FFmpegReadPipeStdout(nullptr)
    , FFmpegWritePipeStdout(nullptr)
    , FFmpegReadPipeStderr(nullptr)
    , FFmpegWritePipeStderr(nullptr)
    , FFmpegProcessHandle(FProcHandle())
    , FFmpegProcessId(0)
    , EncoderWorker(nullptr)
    , EncoderWorkerThread(nullptr)
    , bIsEncodingActive(false)
    , bStopWorkerThread(false) // Inicialização explícita para FThreadSafeBool
    , bNoMoreFramesToEncode(false) // Inicialização explícita para FThreadSafeBool
    , bIsInitialized(false) // Inicialização explícita para FThreadSafeBool
    , bIsPipeCurrentlyCongestedInternal(false) // NOVO: Inicialização da flag interna do encoder
    , FramePool(nullptr) // Inicialização explícita
{
    NewFrameEvent = FPlatformProcess::GetSynchEventFromPool(false); // false para auto-reset

    // Crie o UIARECFactory apenas uma vez no construtor
    EncoderCommandFactory = NewObject<UIARECFactory>(this, UIARECFactory::StaticClass(), TEXT("FFmpegCommandFactory")); 
    if (!EncoderCommandFactory)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioEncoder: Falha ao criar UIARECFactory no construtor."));
    }
    UE_LOG(LogIAR, Log, TEXT("UIARAudioEncoder: Construtor chamado."));
}

UIARAudioEncoder::~UIARAudioEncoder()
{
    // O ShutdownEncoder já cuida da maioria da limpeza
    ShutdownEncoder(); 

    // O NewFrameEvent será retornado no BeginDestroy, mas se o destrutor for chamado
    // de alguma forma sem BeginDestroy (cenários incomuns de GC), é bom garantir.
    // Normalmente, o BeginDestroy é mais seguro para UObjects.
    if (NewFrameEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(NewFrameEvent);
        NewFrameEvent = nullptr;
    }
    UE_LOG(LogIAR, Log, TEXT("UIARAudioEncoder: Destrutor chamado."));
}

void UIARAudioEncoder::BeginDestroy()
{
    ShutdownEncoder();
    if (NewFrameEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(NewFrameEvent);
        NewFrameEvent = nullptr;
    }
    Super::BeginDestroy();
}

// Implementação da função static GetFFmpegExecutablePathInternal()
FString UIARAudioEncoder::GetFFmpegExecutablePathInternal()
{
    // Para uma função estática, não podemos acessar diretamente FFmpegExecutablePath (membro não estático).
    // Se queremos um caminho configurável por Blueprint, teríamos que pegar o CDO aqui.
    // Mas para manter simples e direto para um static helper, vamos gerar o caminho padrão.
    
    FString PluginDir = FPaths::ProjectPluginsDir() / TEXT("IAR");
    FString Path = FPaths::Combine(PluginDir, TEXT("ThirdParty"), TEXT("FFmpeg"), TEXT("Binaries"));

#if PLATFORM_WINDOWS
    Path = FPaths::Combine(Path, TEXT("Win64"), TEXT("ffmpeg.exe"));
#elif PLATFORM_LINUX
    Path = FPaths::Combine(Path, TEXT("Linux"), TEXT("ffmpeg")); 
#elif PLATFORM_MAC
    Path = FPaths::Combine(Path, TEXT("Mac"), TEXT("ffmpeg")); 
#else
    UE_LOG(LogIAR, Error, TEXT("FFmpeg executable path not defined for current platform!"));
    return FString();
#endif

    FPaths::NormalizeDirectoryName(Path);
    return Path;
}

bool UIARAudioEncoder::Initialize(const FIAR_AudioStreamSettings& Settings, const FString& InFFmpegExecutablePath, int32 InActualFrameWidth, int32 InActualFrameHeight, UIARFramePool* InFramePool)
{
    if (bIsInitialized) // FThreadSafeBool implicitamente conversível para bool
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioEncoder is already initialized. Call ShutdownEncoder() first."));
        return false;
    }

    CurrentStreamSettings = Settings;
    // O InFFmpegExecutablePath é ignorado aqui se GetFFmpegExecutablePathInternal() for estático,
    // pois o membro FFmpegExecutablePath não é acessado por uma função estática.
    // Se quisermos que o caminho configurado no BP seja usado aqui, teríamos que
    // passar o caminho diretamente para LaunchEncoder, ou ter FFmpegExecutablePath static.
    // Por enquanto, a função estática GetFFmpegExecutablePathInternal() fornece o caminho padrão.

    ActualProcessingWidth = InActualFrameWidth;
    ActualProcessingHeight = InActualFrameHeight;
    FramePool = InFramePool; // Acesso corrigido ao FramePool

    if (!FramePool) // Verifica se o FramePool é válido
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioEncoder::Initialize: FramePool is null. Cannot initialize."));
        return false;
    }

    // A factory já foi criada no construtor. Apenas verifica se é válida.
    if (!EncoderCommandFactory)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioEncoder: UIARECFactory é nulo. Falha na inicialização."));
        return false;
    }

    // 1. Gerar um nome de pipe único para esta sessão
    FString PipeSessionID = FGuid::NewGuid().ToString(EGuidFormats::Digits).Mid(0,5);
    FString AudioPipeBaseName = FString::Printf(TEXT("IARPipe%s"), *PipeSessionID);

    // 2. Configurar o Named Pipe para áudio
    FIAR_PipeSettings PipeSettings;
    PipeSettings.BasePipeName = AudioPipeBaseName; // Use o nome único gerado
    PipeSettings.bBlockingMode = true; // Escrita bloqueante para garantir dados sequenciais
    PipeSettings.bMessageMode = false; // Modo byte stream para dados de áudio raw
    PipeSettings.bDuplexAccess = false; // Apenas o UE escreve, FFmpeg lê
    
    // Tentar criar o Named Pipe
    if (!AudioPipe.Create(PipeSettings, TEXT("")))
    {
        UE_LOG(LogIAR, Error, TEXT("Failed to create audio Named Pipe: %s"), *AudioPipe.GetFullPipeName());
        return false;
    }
    UE_LOG(LogIAR, Log, TEXT("Audio Named Pipe created: %s"), *AudioPipe.GetFullPipeName());

    // Inicia a worker thread para escrever frames no pipe
    // Passe a referência das flags de controle e do evento.
    // NOTE: A interface FRunnableThread::Create espera um FRunnable*.
    // EncoderWorker é do tipo FIARAudioEncoderWorker*, que herda de FRunnable.
    // A conversão implícita de um tipo derivado para um tipo base é permitida aqui.
    EncoderWorker = new FIARAudioEncoderWorker(AudioPipe, DataQueue, NewFrameEvent, bStopWorkerThread);
    EncoderWorkerThread = FRunnableThread::Create(EncoderWorker, TEXT("IARAudioEncoderWorkerThread"), 0, TPri_Normal);

    if (!EncoderWorkerThread)
    {
        UE_LOG(LogIAR, Error, TEXT("Failed to create audio encoder worker thread. Cleaning up."));
        InternalCleanupEncoderResources(); // Cleanup the pipe
        if (EncoderWorker) { delete EncoderWorker; EncoderWorker = nullptr; } // Delete o worker mesmo se a thread não puder ser criada
        return false;
    }

    bIsInitialized.AtomicSet(true); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioEncoder initialized successfully."));
    return true;
}

bool UIARAudioEncoder::LaunchEncoder(const FString& LiveOutputFilePath)
{
    if (!bIsInitialized) // FThreadSafeBool implicitamente conversível para bool
    {
        UE_LOG(LogIAR, Error, TEXT("Encoder is not initialized. Call Initialize() first."));
        return false;
    }

    if (FFmpegProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(FFmpegProcessHandle))
    {
        UE_LOG(LogIAR, Warning, TEXT("FFmpeg process is already running. Please call ShutdownEncoder() first."));
        return false;
    }

    // Setta o caminho de saída do áudio ao vivo
    CurrentOutputFilePath = LiveOutputFilePath; // Atualiza o caminho de saída

    // NOVO CÓDIGO: Verificar e criar o diretório de saída
    FString OutputDirectory = FPaths::GetPath(CurrentOutputFilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.CreateDirectoryTree(*OutputDirectory))
    {
        UE_LOG(LogIAR, Error, TEXT("Falha ao criar o diretório de saída para gravação: %s. Não é possível lançar o encoder."), *OutputDirectory);
        return false;
    }
    UE_LOG(LogIAR, Log, TEXT("Diretório de saída garantido: %s"), *OutputDirectory);


    // Pega Executavel FFmpeg (agora usando a função static)
    FString ExecPath = UIARAudioEncoder::GetFFmpegExecutablePathInternal();
    if (ExecPath.IsEmpty() || !PlatformFile.FileExists(*ExecPath))
    {
        UE_LOG(LogIAR, Error, TEXT("FFmpeg executável não encontrado em: %s. Não é possível lançar encoder."), *ExecPath);
        return false;
    }

    // Constrói o Comando FFmpeg para a gravação ao vivo (agora para WAV/PCM S16LE)
    FString Arguments = EncoderCommandFactory->BuildAudioEncodeCommand(CurrentStreamSettings, AudioPipe.GetFullPipeName(), CurrentOutputFilePath);
    
    UE_LOG(LogIAR, Log, TEXT("Launching FFmpeg. Executable: %s , Arguments: %s"), *ExecPath, *Arguments);

    // Pipes de Saída e Erro
    FPlatformProcess::CreatePipe(FFmpegReadPipeStdout, FFmpegWritePipeStdout);
    FPlatformProcess::CreatePipe(FFmpegReadPipeStderr, FFmpegWritePipeStderr); 
    
    uint32 LaunchedProcessId = 0; 

    FFmpegProcessHandle = FPlatformProcess::CreateProc(
        *ExecPath,
        *Arguments,
        false,   // bLaunchDetached
        true,    // bLaunchHidden
        true,    // bLaunchReallyHidden
        &LaunchedProcessId,
        (int32)TPri_AboveNormal, // AUMENTADO PARA PRIORIDADE ACIMA DO NORMAL
        nullptr, // OptionalWorkingDirectory
        FFmpegWritePipeStdout, // stdout do FFmpeg vai para este pipe
        FFmpegWritePipeStderr  // stderr do FFmpeg vai para este pipe
    );

    if (!FFmpegProcessHandle.IsValid())
    {
        UE_LOG(LogIAR, Error, TEXT("Failed to create FFmpeg process. Check path and arguments."));
        // Fecha ambos os pares de pipes em caso de falha no lançamento
        FPlatformProcess::ClosePipe(FFmpegReadPipeStdout, FFmpegWritePipeStdout);
        FPlatformProcess::ClosePipe(FFmpegReadPipeStderr, FFmpegWritePipeStderr);
        FFmpegReadPipeStdout = nullptr; FFmpegWritePipeStdout = nullptr;
        FFmpegReadPipeStderr = nullptr; FFmpegWritePipeStderr = nullptr;
        return false;
    }

    UE_LOG(LogIAR, Log, TEXT("FFmpeg main process launched successfully. PID: %d"), LaunchedProcessId);
    
    // Cria dois FFMpegLogReader, um para stdout e outro para stderr
    FFmpegStdoutLogReader = new FFMpegLogReader(FFmpegReadPipeStdout, TEXT("FFmpeg STDOUT"));
    FFmpegStdoutLogReader->Start();

    FFmpegStderrLogReader = new FFMpegLogReader(FFmpegReadPipeStderr, TEXT("FFmpeg STDERR"));
    FFmpegStderrLogReader->Start();

    // Importante: Feche as extremidades de escrita dos pipes no processo pai, pois o FFmpeg as herdou.
    FPlatformProcess::ClosePipe(nullptr, FFmpegWritePipeStdout);
    FPlatformProcess::ClosePipe(nullptr, FFmpegWritePipeStderr);

    // REMOVIDO: AWAITING PIPE CONNECTION - AGORA GERENCIADO NA WORKER THREAD (FIARAudioEncoderWorker::Run())
    
    bIsEncodingActive = true;
    // Reseta as flags de worker thread
    bStopWorkerThread.AtomicSet(false);
    bNoMoreFramesToEncode.AtomicSet(false);

    // NOVO: Inicia o timer para verificar o status de congestionamento do pipe.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PipeCongestionCheckTimerHandle,
            this,
            &UIARAudioEncoder::CheckPipeCongestionStatus,
            0.1f, // Frequência de 10 vezes por segundo (100ms)
            true  // Repetir
        );
    }
    bIsPipeCurrentlyCongestedInternal.AtomicSet(false); // Reinicia o estado interno do encoder

    return true;
}

void UIARAudioEncoder::ShutdownEncoder()
{
    // Usar FThreadSafeBool implicitamente conversível para bool
    // Verificar se bIsInitialized ou se algum dos ponteiros de recurso existe para evitar warnings desnecessários.
    if (!bIsInitialized && !FFmpegProcessHandle.IsValid() && !EncoderWorkerThread && !FFmpegStdoutLogReader && !FFmpegStderrLogReader) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioEncoder is not initialized or already shut down."));
        return;
    }

    UE_LOG(LogIAR, Log, TEXT("Shutting down UIARAudioEncoder..."));

    // 1. Sinaliza à worker thread para parar
    bStopWorkerThread.AtomicSet(true); 
    if (NewFrameEvent) NewFrameEvent->Trigger(); // Acorda a thread caso esteja esperando por um evento

    // 2. Aguarda a conclusão da worker thread
    if (EncoderWorkerThread) // Verificar o ponteiro da thread
    {
        EncoderWorkerThread->WaitForCompletion();
        delete EncoderWorkerThread;
        EncoderWorkerThread = nullptr;
        // O EncoderWorker será deletado aqui, pois ele é "propriedade" do EncoderWorkerThread
        if (EncoderWorker) { delete EncoderWorker; EncoderWorker = nullptr; }
    }

    // 3. Limpa os recursos internos (pipes de entrada) e processo FFmpeg
    InternalCleanupEncoderResources();

    // NOVO: Limpa o timer de verificação de congestionamento
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PipeCongestionCheckTimerHandle);
    }

    bIsEncodingActive = false; // Garante que o estado seja false
    bIsInitialized.AtomicSet(false); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioEncoder shut down successfully."));
}

// AGORA RECEBE TSharedPtr<FIAR_AudioFrameData>
bool UIARAudioEncoder::EncodeFrame(TSharedPtr<FIAR_AudioFrameData> Frame)
{
    if (!bIsEncodingActive) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioEncoder is not active. Cannot encode frame."));
        // Libera o frame para o pool (se não estiver ativo, ele não será enfileirado)
        if (Frame.IsValid() && FramePool)
        {
            FramePool->ReleaseFrame(Frame); 
        }
        return false;
    }
    
    if (bNoMoreFramesToEncode) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioEncoder has been signaled that no more frames are coming. Frame dropped."));
        // Libera o frame para o pool (se não houver mais frames, ele não será enfileirado)
        if (Frame.IsValid() && FramePool)
        {
            FramePool->ReleaseFrame(Frame); 
        }
        return false;
    }

    // Converte os dados float do FIAR_AudioFrameData para uint8 (bytes s16le)
    TArray<uint8> RawBytes;
    RawBytes.Reserve(Frame->RawSamplesPtr->Num() * sizeof(int16)); 

    for (float Sample : *(Frame->RawSamplesPtr)) 
    {
        int16 ConvertedSample = FMath::Clamp(Sample, -1.0f, 1.0f) * 32767.0f;
        RawBytes.Add(static_cast<uint8>(ConvertedSample & 0xFF));        // Low byte
        RawBytes.Add(static_cast<uint8>((ConvertedSample >> 8) & 0xFF)); // High byte
    }
    
    // Enfileira os bytes brutos para o worker thread escrever no pipe
    DataQueue.Enqueue(MoveTemp(RawBytes));
    
    if (NewFrameEvent) { NewFrameEvent->Trigger(); } 

    // *********************************************************************************
    // CORREÇÃO CRÍTICA: LIBERAR O FIAR_AudioFrameData DE VOLTA PARA O POOL
    // *********************************************************************************
    // Agora que os dados brutos foram copiados e enfileirados, o 'Frame' (TSharedPtr<FIAR_AudioFrameData>)
    // pode ser devolvido ao pool para reuso. O TSharedPtr garante que o TArray<float> interno
    // será destruído quando não houver mais referências, mas o objeto FIAR_AudioFrameData em si
    // precisa ser retornado ao seu pool para otimização de memória.
    if (Frame.IsValid() && FramePool) // Garante que o Frame é válido e que há um FramePool para liberar
    {
        FramePool->ReleaseFrame(Frame); 
    }
    // *********************************************************************************

    return true;
}

bool UIARAudioEncoder::FinishEncoding()
{
     if (!bIsInitialized) // FThreadSafeBool implicitamente conversível para bool
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioEncoder is not initialized. Cannot finish encoding."));
        return false;
    }

    UE_LOG(LogIAR, Log, TEXT("Signaling UIARAudioEncoder to finish encoding..."));

    // Sinaliza que não haverá mais frames para codificar
    bNoMoreFramesToEncode.AtomicSet(true);
    if (NewFrameEvent) NewFrameEvent->Trigger(); // Acorda a thread para processar quaisquer frames remanescentes na fila
    // Aguarda ativamente a fila esvaziar para garantir que todos os frames sejam escritos no pipe
    while (!DataQueue.IsEmpty() && !bStopWorkerThread) // FThreadSafeBool implicitamente conversível para bool
    {
        FPlatformProcess::Sleep(0.01f); // Pequena pausa para permitir que o worker processe
    }

    // Fecha o pipe de entrada para sinalizar EOF ao FFmpeg.
    // É crucial fechar o pipe APENAS depois que todos os dados foram escritos.
    if (AudioPipe.IsValid())
    {
        AudioPipe.Close(); 
        UE_LOG(LogIAR, Log, TEXT("Audio input pipe closed, signaling EOF to FFmpeg."));
    }
    
    UE_LOG(LogIAR, Log, TEXT("UIARAudioEncoder finished sending audio data."));
    return true;
}

// Implementação da função static LaunchBlockingFFmpegProcess (STATIC)
bool UIARAudioEncoder::LaunchBlockingFFmpegProcess(const FString& ExecPath, const FString& Arguments)
{
    // Esta função será executada em um background thread.
    // Ela será bloqueante para este thread, mas não para a Game Thread.

    UE_LOG(LogIAR, Log, TEXT("Launching Blocking FFmpeg Process. Executable: %s, Arguments: %s"), *ExecPath, *Arguments);

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ExecPath))
    {
        UE_LOG(LogIAR, Error, TEXT("FFmpeg executable not found at: %s. Cannot launch blocking process."),*ExecPath);
        return false;
    }

    FProcHandle ProcHandle = FPlatformProcess::CreateProc(
        *ExecPath,
        *Arguments,
        false,   // bLaunchDetached
        true,    // bLaunchHidden
        true,    // bLaunchReallyHidden
        nullptr, // OutProcessID
        -1,      // PriorityModifier
        nullptr, // OptionalWorkingDirectory
        nullptr, // StdOutPipeWrite
        nullptr  // StdErrPipeWrite
    );

    if (!ProcHandle.IsValid())
    {
        UE_LOG(LogIAR, Error, TEXT("Failed to launch blocking FFmpeg process. Check path and arguments."));
        return false;
    }

    FPlatformProcess::WaitForProc(ProcHandle);
    int32 ReturnCode = -1;
    FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
    FPlatformProcess::CloseProc(ProcHandle);

    if (ReturnCode != 0)
    {
        UE_LOG(LogIAR, Error, TEXT("Blocking FFmpeg process exited with error code: %d. Command: %s"), ReturnCode, *Arguments);
        return false;
    }

    UE_LOG(LogIAR, Log, TEXT("Blocking FFmpeg process completed successfully. Command: %s"), *Arguments);
    return true;
}

// Implementação da função static para decodificação de áudio para PCM bruto usando FFmpeg
bool UIARAudioEncoder::DecodeAudioFileToRawPCM(const FString& InputFilePath, int32 TargetSampleRate, int32 TargetNumChannels, TArray<uint8>& OutRawPCMData)
{
    OutRawPCMData.Empty(); // Limpa quaisquer dados pré-existentes

    FString FFmpegExecPath = UIARAudioEncoder::GetFFmpegExecutablePathInternal();
    if (FFmpegExecPath.IsEmpty() || !FPlatformFileManager::Get().GetPlatformFile().FileExists(*FFmpegExecPath))
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("FFmpeg executable not found at: %s. Cannot decode audio file."), *FFmpegExecPath);
        return false;
    }

    FString FFmpegArguments = FString::Printf(TEXT("-nostdin -i %s -f s16le -ar %d -ac %d -"), // **ADICIONADO -nostdin e ESCAPE DE ASPAS**
        *FPaths::ConvertRelativePathToFull(InputFilePath), 
        TargetSampleRate,
        TargetNumChannels);

    UE_LOG(LogIARAudioEncoder, Log, TEXT("Decoding audio file '%s' with FFmpeg command: ffmpeg %s"), *InputFilePath, *FFmpegArguments);

    void* ReadPipeStdout = nullptr;
    void* WritePipeStdout = nullptr;
    FPlatformProcess::CreatePipe(ReadPipeStdout, WritePipeStdout); 

    void* ReadPipeStderr = nullptr;
    void* WritePipeStderr = nullptr;
    FPlatformProcess::CreatePipe(ReadPipeStderr, WritePipeStderr);

    // OBTEM O DIRETÓRIO DE TRABALHO PARA O FFmpeg (Onde o ffmpeg.exe está localizado)
    FString WorkingDirectory = FPaths::GetPath(FFmpegExecPath);
    FPaths::NormalizeDirectoryName(WorkingDirectory);

    FProcHandle ProcHandle = FPlatformProcess::CreateProc(
        *FFmpegExecPath,
        *FFmpegArguments,
        false,   // bLaunchDetached
        true,    // bLaunchHidden
        true,    // bLaunchReallyHidden
        nullptr, // OutProcessID
        -1,      // PriorityModifier
        *WorkingDirectory, 
        WritePipeStdout, 
        WritePipeStderr  
    );

    if (!ProcHandle.IsValid())
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("Failed to launch FFmpeg process to decode audio file '%s'. Check path and arguments."), *InputFilePath);
        FPlatformProcess::ClosePipe(ReadPipeStdout, WritePipeStdout);
        FPlatformProcess::ClosePipe(ReadPipeStderr, WritePipeStderr);
        return false;
    }

    FPlatformProcess::ClosePipe(nullptr, WritePipeStdout);
    FPlatformProcess::ClosePipe(nullptr, WritePipeStderr);

    TArray<uint8> StderrOutputData; 
    FFMpegLogReader* StdoutReader = new FFMpegLogReader(ReadPipeStdout, TEXT("FFmpeg DECODE STDOUT"), &OutRawPCMData);
    StdoutReader->Start();
    FFMpegLogReader* StderrReader = new FFMpegLogReader(ReadPipeStderr, TEXT("FFmpeg DECODE STDERR"), &StderrOutputData);
    StderrReader->Start();

    FPlatformProcess::WaitForProc(ProcHandle);
    int32 ReturnCode = -1;
    FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
    
    UE_LOG(LogIARAudioEncoder, Log, TEXT("FFmpeg process for '%s' exited with return code: %d."), *InputFilePath, ReturnCode);

    StdoutReader->EnsureCompletion();
    StderrReader->EnsureCompletion();

    delete StdoutReader; StdoutReader = nullptr;
    delete StderrReader; StderrReader = nullptr;

    FPlatformProcess::CloseProc(ProcHandle);

    if (StderrOutputData.Num() > 0)
    {
        FString StderrString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(StderrOutputData.GetData())));
        UE_LOG(LogIARAudioEncoder, Error, TEXT("FFmpeg Stderr Output for '%s':\n%s"), *InputFilePath, *StderrString);
    }
    else
    {
        UE_LOG(LogIARAudioEncoder, Log, TEXT("FFmpeg Stderr Output for '%s' was EMPTY."), *InputFilePath);
    }

    if (ReturnCode != 0)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("FFmpeg decoding process for '%s' exited with error code: %d. Command: ffmpeg %s"), *InputFilePath, ReturnCode, *FFmpegArguments);
        OutRawPCMData.Empty();
        return false;
    }
    if (OutRawPCMData.Num() == 0)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("FFmpeg decoding process for '%s' completed with return code 0, but no PCM data was captured."), *InputFilePath);
        return false;
    }

    UE_LOG(LogIARAudioEncoder, Log, TEXT("Audio file '%s' decoded to %d bytes of raw PCM data successfully."), *InputFilePath, OutRawPCMData.Num());
    return true;
}

// NOVO: Implementação da função estática para decodificação de WAV usando dr_wav
bool UIARAudioEncoder::DecodeWaveFileToRawPCM_DrWav(const FString& InputFilePath, TArray<float>& OutRawPCMData, int32& OutSampleRate, int32& OutNumChannels)
{
    OutRawPCMData.Empty();
    OutSampleRate = 0;
    OutNumChannels = 0;

    drwav wavInfo;
    // Tentar abrir o arquivo WAV para leitura
    if (!drwav_init_file(&wavInfo, TCHAR_TO_UTF8(*InputFilePath), NULL))
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("DrWav: Falha ao abrir ou ler informações do arquivo WAV: %s"), *InputFilePath);
        return false;
    }

    OutSampleRate = wavInfo.sampleRate;
    OutNumChannels = wavInfo.channels;
    drwav_uint64 totalPCMFrameCount = wavInfo.totalPCMFrameCount;

    // Redimensionar o TArray para armazenar todos os samples (dr_wav pode ler em float)
    // Multiplica por wavInfo.channels porque drwav_read_pcm_frames_f32 espera o número total de *samples* (frames * canais)
    OutRawPCMData.SetNumUninitialized(totalPCMFrameCount * wavInfo.channels);

    // Ler todos os samples para o TArray de float
    drwav_uint64 samplesRead = drwav_read_pcm_frames_f32(&wavInfo, totalPCMFrameCount * wavInfo.channels, OutRawPCMData.GetData());

    drwav_uninit(&wavInfo); // Libera os recursos do dr_wav

    if (samplesRead != totalPCMFrameCount * wavInfo.channels)
    {
        UE_LOG(LogIARAudioEncoder, Warning, TEXT("DrWav: Lidos %llu samples, esperados %llu. Arquivo pode estar corrompido ou truncado: %s"), samplesRead, totalPCMFrameCount * wavInfo.channels, *InputFilePath);
        OutRawPCMData.SetNum(samplesRead); // Ajusta o tamanho real para o que foi realmente lido
    }

    if (OutRawPCMData.Num() == 0)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("DrWav: Nenhuns dados PCM lidos do arquivo WAV: %s"), *InputFilePath);
        return false;
    }
    
    UE_LOG(LogIARAudioEncoder, Log, TEXT("DrWav: Arquivo '%s' carregado com sucesso. SampleRate: %d, Canais: %d, Samples: %d"),
        *InputFilePath, OutSampleRate, OutNumChannels, OutRawPCMData.Num());

    return true;
}

// NOVO: Implementação da função estática para codificação de PCM bruto para WAV usando dr_wav
bool UIARAudioEncoder::EncodeRawPCMToFile(const TArray<float>& RawPCMData, int32 SampleRate, int32 NumChannels, const FString& OutputFilePath, int32 BitDepth)
{
    if (RawPCMData.Num() == 0)
    {
        UE_LOG(LogIARAudioEncoder, Warning, TEXT("EncodeRawPCMToFile: Nenhum dado PCM bruto para codificar."));
        return false;
    }
    if (SampleRate <= 0 || NumChannels <= 0 || BitDepth <= 0)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("EncodeRawPCMToFile: Parâmetros de formato de audio inválidos. SR: %d, CH: %d, BD: %d"), SampleRate, NumChannels, BitDepth);
        return false;
    }

    // Atualmente, esta implementação foca em 16-bit PCM para WAV.
    // Expansão para outras profundidades de bit ou formatos exigiria mais lógica.
    if (BitDepth != 16)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("EncodeRawPCMToFile: A profundidade de bits suportada para dr_wav é atualmente apenas 16-bit para esta função."));
        return false;
    }

    drwav_data_format Format;
    Format.container = drwav_container_riff; // Contêiner WAV padrão (RIFF)
    Format.format = DR_WAVE_FORMAT_PCM;     // Formato PCM (não comprimido)
    Format.channels = NumChannels;
    Format.sampleRate = SampleRate;
    Format.bitsPerSample = BitDepth;

    drwav WavWriter;
    // Converte FString para std::string, necessário para dr_wav
    std::string OutputFilePathStd = TCHAR_TO_UTF8(*OutputFilePath);

    // Prepara os dados: dr_wav espera int16 para PCM_S16LE. Converte de float (-1.0 a 1.0) para int16.
    TArray<drwav_int16> PCM16BitAudio;
    PCM16BitAudio.SetNumUninitialized(RawPCMData.Num());
    for (int32 i = 0; i < RawPCMData.Num(); ++i)
    {
        // Clampa o float para o range de -1.0 a 1.0 e converte para int16
        PCM16BitAudio[i] = FMath::Clamp(RawPCMData[i], -1.0f, 1.0f) * 32767.0f;
    }

    // Inicializa o escritor de arquivos WAV
    if (!drwav_init_file_write(&WavWriter, OutputFilePathStd.c_str(), &Format, NULL))
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("EncodeRawPCMToFile: Falha ao inicializar o escritor WAV para %s"), *OutputFilePath);
        return false;
    }

    // Escreve os frames PCM no arquivo
    drwav_uint64 FramesWritten = drwav_write_pcm_frames(&WavWriter, PCM16BitAudio.Num() / Format.channels, PCM16BitAudio.GetData());
    drwav_uninit(&WavWriter); // Libera os recursos do dr_wav

    // Verifica se todos os frames foram escritos com sucesso
    if (FramesWritten != (drwav_uint64)PCM16BitAudio.Num() / Format.channels)
    {
        UE_LOG(LogIARAudioEncoder, Error, TEXT("EncodeRawPCMToFile: Falha ao escrever todos os frames de áudio para o arquivo WAV: %s"), *OutputFilePath);
        return false;
    }

    UE_LOG(LogIARAudioEncoder, Log, TEXT("EncodeRawPCMToFile: PCM bruto codificado com sucesso para WAV: %s"), *OutputFilePath);
    return true;
}


void UIARAudioEncoder::InternalCleanupEncoderResources()
{
    UE_LOG(LogIAR, Log, TEXT("Cleaning up audio encoder internal resources..."));

    // Apenas fecha o pipe se ele ainda estiver aberto (FinishEncoding já o faz).
    if (AudioPipe.IsValid())
    {
        AudioPipe.Close();
        UE_LOG(LogIAR, Log, TEXT("Audio input pipe explicitly closed during cleanup. "));
    }

    // Limpa e deleta os leitores de log do FFmpeg (stdout e stderr)
    if (FFmpegStdoutLogReader)
    {
        FFmpegStdoutLogReader->EnsureCompletion(); // Espera a thread de leitura terminar
        delete FFmpegStdoutLogReader;
        FFmpegStdoutLogReader = nullptr;
        // IMPORTANTE: Feche as extremidades de leitura dos pipes aqui, pois elas foram abertas
        // pelo FPlatformProcess::CreatePipe()
        FPlatformProcess::ClosePipe(FFmpegReadPipeStdout, nullptr);
        FFmpegReadPipeStdout = nullptr;
    }

    if (FFmpegStderrLogReader)
    {
        FFmpegStderrLogReader->EnsureCompletion();
        delete FFmpegStderrLogReader;
        FFmpegStderrLogReader = nullptr;
        FPlatformProcess::ClosePipe(FFmpegReadPipeStderr, nullptr);
        FFmpegReadPipeStderr = nullptr;
    }
    
    // Espera o processo FFmpeg terminar completamente
    if (FFmpegProcessHandle.IsValid())
    {
        UE_LOG(LogIAR, Log, TEXT("Waiting for main FFmpeg process to complete..."));
        FPlatformProcess::WaitForProc(FFmpegProcessHandle); 
        
        int32 ReturnCode = -1;
        FPlatformProcess::GetProcReturnCode(FFmpegProcessHandle, &ReturnCode); 
        UE_LOG(LogIAR, Log, TEXT("Main FFmpeg process finished with code: %d"), ReturnCode);
        // Força o fechamento do processo FFmpeg se ele ainda estiver rodando (caso não tenha terminado graciosamente)
        if (FPlatformProcess::IsProcRunning(FFmpegProcessHandle))
        {
            UE_LOG(LogIAR, Warning, TEXT("Main FFmpeg process did not terminate gracefully. Terminating forcefully."));
            FPlatformProcess::TerminateProc(FFmpegProcessHandle);
        }
        FPlatformProcess::CloseProc(FFmpegProcessHandle); // Libera o handle do processo
        FFmpegProcessHandle.Reset();
    }

    // Garante que os handles de escrita dos pipes sejam nulos após o processo FFmpeg ter sido terminado
    FFmpegWritePipeStdout = nullptr;
    FFmpegWritePipeStderr = nullptr;

    UE_LOG(LogIAR, Log, TEXT("Audio encoder internal resources cleaned up."));
}

void UIARAudioEncoder::CheckPipeCongestionStatus()
{
    // O timer pode disparar depois que o worker já foi deletado ou a codificação parou
    if (!EncoderWorker || !bIsEncodingActive) 
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(PipeCongestionCheckTimerHandle);
        }
        return;
    }

    bool bWorkerCongested = EncoderWorker->IsPipeCurrentlyCongested();

    // Usamos a flag interna do encoder para evitar broadcast excessivo
    // e garantir que o sinal seja enviado apenas na transição de estado.
    if (bWorkerCongested && !bIsPipeCurrentlyCongestedInternal)
    {
        bIsPipeCurrentlyCongestedInternal.AtomicSet(true);
        OnAudioPipeCongested.Broadcast(CurrentOutputFilePath);
    }
    else if (!bWorkerCongested && bIsPipeCurrentlyCongestedInternal)
    {
        bIsPipeCurrentlyCongestedInternal.AtomicSet(false);
        OnAudioPipeCleared.Broadcast(CurrentOutputFilePath);
    }
}
