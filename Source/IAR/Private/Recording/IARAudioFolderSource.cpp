// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioFolderSource.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h" // <<-- ADICIONADO: Necessário para IFileManager::Get().FindFilesRecursive
#include "Async/Async.h" // Para AsyncTask e TAtomic
#include "Kismet/GameplayStatics.h" // Para GetWorld() em contextos UObject (se necessário)

// Inclusões dos componentes do IAR necessários para as conversões
#include "Recording/IARAudioEncoder.h"       // Para decodificação/codificação de áudio (dr_wav, FFmpeg)
#include "AudioAnalysis/IARAudioToMIDITranscriber.h" // Para transcrição de Áudio para MIDI
#include "AudioAnalysis/IARMIDIToAudioSynthesizer.h" // Para síntese de MIDI para Áudio
#include "Recording/IARMIDIFileSource.h"     // Para carregar arquivos MIDI (.mid)

// Dependências de terceiros
#include "MIDI/MidiFile.h"                   // <<-- ADICIONADO: Necessário para smf::MidiFile
#include "MIDI/MidiEvent.h"                  // <<-- ADICIONADO: Necessário para smf::MidiEvent

#include "GlobalStatics.h"
#include "AudioAnalysis/IARBasicAudioFeatureProcessor.h" 

// Para auxiliar na depuração (pode ser substituído por LogIAR para logs mais específicos)
DEFINE_LOG_CATEGORY_STATIC(LogIARFolderSource, Log, All);

// Define o namespace smf para evitar a necessidade de prefixar tudo com smf::
using namespace smf; // <<-- ADICIONADO

UIARFolderSource::UIARFolderSource()
    : UIARMediaSource()
    , CurrentFileIndex(0)
    , bProcessingActive(false)
{
    // Define caminhos padrão que podem ser sobrescritos via Blueprint ou API
    InputFolderPath  = UIARGlobalStatics::GetIARRecordingRootPath() / TEXT("IAR_InputMedia"); // Pasta de entrada no projeto
    OutputFolderPath = UIARGlobalStatics::GetIARRecordingRootPath() / TEXT("IAR_ConvertedMedia"); // Pasta de saída nos Saved
}

UIARFolderSource::~UIARFolderSource()
{
    Shutdown(); // Garante que o processamento seja parado e recursos liberados
}

void UIARFolderSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    Super::Initialize(StreamSettings, InFramePool); // Chama a inicialização da classe base

    // Normaliza e garante que os caminhos são tratados consistentemente pelo sistema de arquivos
    FPaths::NormalizeDirectoryName(InputFolderPath);
    FPaths::NormalizeDirectoryName(OutputFolderPath);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile(); // Não é usado diretamente para FindFilesRecursive
    IFileManager& FileManager = IFileManager::Get(); // <<-- ADICIONADO

    // Cria o diretório de saída se ele não existir
    if (!PlatformFile.CreateDirectoryTree(*OutputFolderPath))
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao criar diretório de saída: %s"), *OutputFolderPath);
        OnFolderProcessingError.Broadcast(FString::Printf(TEXT("Falha ao criar diretório de saída: %s"), *OutputFolderPath));
        return; // A inicialização falha se não conseguir criar o diretório de saída
    }

    // NOVO: Inicializa o Transcriber e o Feature Processor AQUI na Game Thread
    if (!Transcriber)
    {
        Transcriber = NewObject<UIARAudioToMIDITranscriber>(this);
        if (Transcriber)
        {
            Transcriber->Initialize(StreamSettings.SampleRate); // Use SampleRate da StreamSettings de UIARFolderSource
        }
        else
        {
            UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao criar UIARAudioToMIDITranscriber na inicialização."));
        }
    }
    else // Já existe, apenas reinicialize se necessário
    {
        Transcriber->Initialize(StreamSettings.SampleRate);
    }

    if (!FeatureProcessor)
    {
        // Instancie o Feature Processor específico que você quer usar
        FeatureProcessor = NewObject<UIARBasicAudioFeatureProcessor>(this); // Ou UIARAdvancedAudioFeatureProcessor
        if (FeatureProcessor)
        {
            FeatureProcessor->Initialize();
        }
        else
        {
            UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao criar FeatureProcessor na inicialização."));
        }
    }
    else // Já existe, apenas reinicialize
    {
        FeatureProcessor->Initialize();
    }

    // Escaneia o diretório de entrada para listar todos os arquivos suportados
    FilesToProcess.Empty();
    // Suporte para áudio WAV e MP3
    FileManager.FindFilesRecursive(FilesToProcess, *InputFolderPath, TEXT("*.wav"), true, false,false); // <<-- CORRIGIDO
    FileManager.FindFilesRecursive(FilesToProcess, *InputFolderPath, TEXT("*.mp3"), true, false,false); // <<-- CORRIGIDO
    // Suporte para MIDI
    FileManager.FindFilesRecursive(FilesToProcess, *InputFolderPath, TEXT("*.mid"), true, false,false); // <<-- CORRIGIDO

    if (FilesToProcess.Num() == 0)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Nenhum arquivo de áudio/MIDI suportado encontrado no diretório de entrada: %s"), *InputFolderPath);
        OnFolderProcessingError.Broadcast(FString::Printf(TEXT("Nenhum arquivo de mídia suportado encontrado em: %s"), *InputFolderPath));
    }
    
    UE_LOG(LogIARFolderSource, Log, TEXT("Inicializado. Encontrados %d arquivos para processar em: %s."), FilesToProcess.Num(), *InputFolderPath);
}

void UIARFolderSource::StartCapture()
{
    // Verifica se o processamento já está ativo para evitar chamadas duplicadas
    if (bProcessingActive)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Processamento já ativo. Ignorando StartCapture."));
        return;
    }

    // Se não houver arquivos para processar, sinaliza conclusão imediatamente
    if (FilesToProcess.Num() == 0)
    {
        UE_LOG(LogIARFolderSource, Log, TEXT("Nenhum arquivo para processar. Concluindo imediatamente."));
        OnFolderProcessingCompleted.Broadcast(OutputFolderPath);
        return;
    }

    // Define o estado do processamento como ativo
    bProcessingActive.AtomicSet(true);
    // Reinicia o índice do arquivo atual
    CurrentFileIndex.Set(0);
    
    Super::StartCapture(); // Chama o método base, que define bIsCapturing = true

    UE_LOG(LogIARFolderSource, Log, TEXT("Iniciando processamento em lote de %d arquivos."), FilesToProcess.Num());
    
    // Inicia a tarefa de processamento em uma thread de background para não bloquear a Game Thread
    // O `[this]` na lambda captura a instância atual de UIARFolderSource para uso na AsyncTask.
    AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
    {
        // Chama o método que contém a lógica de processamento síncrona/em lote
        ProcessFileConversionAsyncTask();
    });
}

void UIARFolderSource::StopCapture()
{
    // Verifica se há um processamento ativo para interromper
    if (!bProcessingActive)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Nenhum processamento ativo para parar."));
        return;
    }

    // Sinaliza à tarefa assíncrona para parar sua execução
    bProcessingActive.AtomicSet(false);
    UE_LOG(LogIARFolderSource, Log, TEXT("StopCapture chamado. Sinalizando tarefa de background para terminar."));
    // A chamada a Super::StopCapture() (que define bIsCapturing = false) será feita na Game Thread
    // após o término limpo da AsyncTask (ou seja, quando ProcessFileConversionAsyncTask() finalizar).
}

void UIARFolderSource::Shutdown()
{
    StopCapture(); // Garante que qualquer processamento em andamento seja interrompido
    
    // NOVO: Desliga e limpa o Transcriber e o Feature Processor AQUI
    if (Transcriber)
    {
        Transcriber->Shutdown();
        // Não defina Transcriber = nullptr; pois é UPROPERTY() e será limpo pelo GC
    }
    if (FeatureProcessor)
    {
        FeatureProcessor->Shutdown();
        // Não defina FeatureProcessor = nullptr; pois é UPROPERTY() e será limpo pelo GC
    }
    
    
    FilesToProcess.Empty(); // Limpa a lista de arquivos para processamento
    Super::Shutdown(); // Chama o método base para liberar recursos comuns
    UE_LOG(LogIARFolderSource, Log, TEXT("Desligado e recursos liberados."));
}

// Handlers para os Delegates do UIARFolderSource
void UIARFolderSource::HandleFolderProcessingCompleted(const FString& pInOutputFolderPath)
{
    // Retransmite o evento para o UIARAudioComponent's delegate público
    OnFolderProcessingCompleted.Broadcast(pInOutputFolderPath);
}

void UIARFolderSource::HandleFolderProcessingError(const FString& InErrorMessage)
{
    // Retransmite o evento para o UIARAudioComponent's delegate público
    OnFolderProcessingError.Broadcast(InErrorMessage);
}

void UIARFolderSource::HandleFolderProcessingProgress(const FString& InCurrentFileName, float InProgressRatio)
{
    // Retransmite o evento para o UIARAudioComponent's delegate público
    OnFolderProcessingProgress.Broadcast(InCurrentFileName, InProgressRatio);
}

/**
 * @brief Método principal da tarefa assíncrona.
 * Itera sobre os arquivos, determina o tipo de conversão e a executa.
 * Esta função é executada em um thread de background.
 */
void UIARFolderSource::ProcessFileConversionAsyncTask()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    for (int32 i = 0; i < FilesToProcess.Num(); ++i)
    {
        // Verifica a flag de interrupção em cada iteração para permitir que StopCapture() funcione
        if (!bProcessingActive)
        {
            UE_LOG(LogIARFolderSource, Log, TEXT("Processamento interrompido pela requisição do usuário."));
            break; // Sai do loop se for solicitada a interrupção
        }

        FString InputFilePath = FilesToProcess[i];
        FString CurrentFileName = FPaths::GetBaseFilename(InputFilePath);
        float ProgressRatio = (float)(i + 1) / FilesToProcess.Num();

        // Broadcast do progresso para a Game Thread (UI, logs, etc.)
        AsyncTask(ENamedThreads::GameThread, [this, CurrentFileName, ProgressRatio]()
        {
            OnFolderProcessingProgress.Broadcast(CurrentFileName, ProgressRatio);
        });

        UE_LOG(LogIARFolderSource, Log, TEXT("Processando arquivo: %s (%d/%d)"), *InputFilePath, i + 1, FilesToProcess.Num());

        // Determina o tipo de arquivo com base na extensão
        FString FileExtension = FPaths::GetExtension(InputFilePath).ToLower();
        bool bIsAudioFile = (FileExtension == TEXT("wav") || FileExtension == TEXT("mp3"));
        bool bIsMIDIFile = (FileExtension == TEXT("mid"));

        if (bIsAudioFile)
        {
            FString MIDIOutputFilePath = GetOutputFilePath(InputFilePath, true); // True para áudio -> MIDI
            if (!PlatformFile.FileExists(*MIDIOutputFilePath) || bOverwriteExistingFiles)
            {
                if (!ConvertAudioToMIDI(InputFilePath, MIDIOutputFilePath))
                {
                    // Reporta o erro para a Game Thread
                    AsyncTask(ENamedThreads::GameThread, [this, InputFilePath]()
                    {
                        OnFolderProcessingError.Broadcast(FString::Printf(TEXT("Falha ao converter Áudio para MIDI para: %s"), *InputFilePath));
                    });
                }
            }
            else
            {
                UE_LOG(LogIARFolderSource, Warning, TEXT("Pulando %s: arquivo MIDI de saída já existe e sobrescrita desabilitada."), *InputFilePath);
            }
        }
        else if (bIsMIDIFile)
        {
            FString AudioOutputFilePath = GetOutputFilePath(InputFilePath, false); // False para MIDI -> Áudio (será .wav)
            if (!PlatformFile.FileExists(*AudioOutputFilePath) || bOverwriteExistingFiles)
            {
                if (!ConvertMIDIToAudio(InputFilePath, AudioOutputFilePath))
                {
                    // Reporta o erro para a Game Thread
                    AsyncTask(ENamedThreads::GameThread, [this, InputFilePath]()
                    {
                        OnFolderProcessingError.Broadcast(FString::Printf(TEXT("Falha ao converter MIDI para Áudio para: %s"), *InputFilePath));
                    });
                }
            }
            else
            {
                 UE_LOG(LogIARFolderSource, Warning, TEXT("Pulando %s: arquivo de Áudio de saída já existe e sobrescrita desabilitada."), *InputFilePath);
            }
        }
        else
        {
            UE_LOG(LogIARFolderSource, Warning, TEXT("Tipo de arquivo não suportado para conversão: %s"), *InputFilePath);
        }
        CurrentFileIndex.Add(1); // <<-- CORRIGIDO: Usando TAtomic::Add(1)
    }

    // Sinaliza o término do processamento na Game Thread, independentemente de ter sido concluído ou interrompido
    AsyncTask(ENamedThreads::GameThread, [this]()
    {
        bProcessingActive.AtomicSet(false); // Define o estado como inativo
        Super::StopCapture(); // Chama o método base para finalizar (define bIsCapturing = false)
        OnFolderProcessingCompleted.Broadcast(OutputFolderPath); // Dispara o delegate de conclusão
        UE_LOG(LogIARFolderSource, Log, TEXT("Processamento em lote concluído (ou interrompido)."));
    });
}

/**
 * @brief Constrói o caminho completo do arquivo de saída, determinando a extensão correta.
 * @param InputFilePath Caminho do arquivo de entrada.
 * @param bIsAudioToMIDIConversion Se verdadeiro, a extensão de saída será .mid; caso contrário, .wav.
 * @return O caminho completo do arquivo de saída com a extensão apropriada.
 */
FString UIARFolderSource::GetOutputFilePath(const FString& InputFilePath, bool bIsAudioToMIDIConversion) const
{
    FString BaseFileName = FPaths::GetBaseFilename(InputFilePath);
    FString OutputExtension = bIsAudioToMIDIConversion ? TEXT("mid") : TEXT("wav"); // Se Áudio->MIDI, é .mid; caso contrário, é .wav
    
    return FPaths::Combine(OutputFolderPath, BaseFileName + TEXT(".") + OutputExtension);
}

/**
 * @brief Implementa a lógica para converter um arquivo de áudio (.wav ou .mp3) para MIDI (.mid).
 * Utiliza UIARAudioEncoder para decodificação e UIARAudioToMIDITranscriber + smf::MidiFile para transcrição e salvamento.
 */
bool UIARFolderSource::ConvertAudioToMIDI(const FString& AudioFilePath, const FString& MIDIOutputFilePath)
{
    UE_LOG(LogIARFolderSource, Log, TEXT("Iniciando conversão Áudio para MIDI: %s -> %s"), *AudioFilePath, *MIDIOutputFilePath);

    // --- 1. Decodificar o arquivo de audio para PCM bruto (float) ---
    TArray<float> RawAudioSamples;
    int32 ActualSampleRate = 0;
    int32 ActualNumChannels = 0;

    FString FileExtension = FPaths::GetExtension(AudioFilePath).ToLower();
    if (FileExtension == TEXT("wav"))
    {
        // Decodificação otimizada para WAV usando dr_wav
        if (!UIARAudioEncoder::DecodeWaveFileToRawPCM_DrWav(AudioFilePath, RawAudioSamples, ActualSampleRate, ActualNumChannels))
        {
            UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao decodificar arquivo WAV com DrWav: %s"), *AudioFilePath);
            return false;
        }
    }
    else if (FileExtension == TEXT("mp3"))
    {
        // Decodificação para MP3 (ou outros formatos que o FFmpeg suporta) via FFmpeg
        // FFmpeg decodifica para s16le (int16), que precisa ser convertido para float.
        TArray<uint8> RawPCMUint8;
        // Taxa de amostragem e canais alvo para a decodificação (pode ser ajustado se o FFmpeg tiver preferência)
        int32 TargetSR = 44100;
        int32 TargetCH = 2; // Estéreo por padrão, se o source tiver mais de 2, será mixado
        if (!UIARAudioEncoder::DecodeAudioFileToRawPCM(AudioFilePath, TargetSR, TargetCH, RawPCMUint8))
        {
            UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao decodificar arquivo MP3 com FFmpeg: %s"), *AudioFilePath);
            return false;
        }

        // Converte o PCM s16le (uint8) para float (TArray<float>)
        RawAudioSamples.SetNumUninitialized(RawPCMUint8.Num() / sizeof(int16));
        for (int32 i = 0; i < RawAudioSamples.Num(); ++i)
        {
            int16 SampleS16 = (int16)((RawPCMUint8[i * 2 + 1] << 8) | RawPCMUint8[i * 2]); // Reconstitui int16 (little-endian)
            RawAudioSamples[i] = SampleS16 / 32768.0f; // Normaliza para float entre -1.0 e 1.0
        }
        ActualSampleRate = TargetSR;
        ActualNumChannels = TargetCH;
    }
    else
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Formato de áudio de entrada não suportado para conversão para MIDI: %s"), *FileExtension);
        return false;
    }

    if (RawAudioSamples.Num() == 0 || ActualSampleRate == 0 || ActualNumChannels == 0)
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Amostras de áudio decodificadas vazias ou metadados inválidos para %s."), *AudioFilePath);
        return false;
    }

    // --- 2. Transcrição de Áudio para MIDI ---
    // Transcriber e FeatureProcessor devem ser membros da classe e inicializados em UIARFolderSource::Initialize()
    if (!Transcriber || !FeatureProcessor)
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Transcriber ou FeatureProcessor não inicializados. Conversão Áudio para MIDI falhou."));
        return false;
    }

    // Vetor para coletar os eventos MIDI transcritos
    TArray<FIAR_MIDIEvent> TranscribedMIDIEvents;
    
    // Adiciona uma lambda ao delegate do transcritor para coletar os eventos para esta conversão específica.
    // Usamos FDelegateHandle para poder remover o bind ao final da função e evitar binds múltiplos.
    FDelegateHandle TranscriberDelegateHandle = Transcriber->OnMIDITranscriptionEventGenerated.AddLambda([&](const FIAR_MIDIEvent& MIDIEvent)
    {
        TranscribedMIDIEvents.Add(MIDIEvent);
    });

    // Simular o processamento frame a frame para o transcritor (chunks de 100ms)
    int32 FrameSizeSamples = ActualSampleRate / 10; 
    for (int32 i = 0; i < RawAudioSamples.Num(); i += FrameSizeSamples * ActualNumChannels) 
    {
        int32 CurrentChunkNumSamples = FMath::Min(FrameSizeSamples * ActualNumChannels, RawAudioSamples.Num() - i);
        TArray<float> CurrentChunkSamples;
        CurrentChunkSamples.SetNumUninitialized(CurrentChunkNumSamples);
        FMemory::Memcpy(CurrentChunkSamples.GetData(), RawAudioSamples.GetData() + i, CurrentChunkNumSamples * sizeof(float));

        // Mixer para mono (se necessário)
        TArray<float> MonoSamples;
        if (ActualNumChannels > 1)
        {
            MonoSamples.SetNumUninitialized(CurrentChunkNumSamples / ActualNumChannels);
            for (int32 k = 0; k < MonoSamples.Num(); ++k)
            {
                float Sum = 0.0f;
                for (int32 c = 0; c < ActualNumChannels; ++c)
                {
                    Sum += CurrentChunkSamples[k * ActualNumChannels + c];
                }
                MonoSamples[k] = Sum / ActualNumChannels;
            }
        }
        else
        {
            MonoSamples = CurrentChunkSamples;
        }

        // Cria um FIAR_AudioFrameData para o FeatureProcessor
        TSharedPtr<FIAR_AudioFrameData> CurrentAudioFrame = MakeShared<FIAR_AudioFrameData>();
        *(CurrentAudioFrame->RawSamplesPtr) = MonoSamples; 
        CurrentAudioFrame->SampleRate = ActualSampleRate;
        CurrentAudioFrame->NumChannels = 1; // O FeatureProcessor trabalha com mono
        CurrentAudioFrame->Timestamp = (float)i / ActualSampleRate; 

        UTexture2D* DummySpectrogramTexture = nullptr; // Necessário para a assinatura, mesmo que não usado aqui
        FIAR_AudioFeatures ExtractedFeatures = FeatureProcessor->ProcessFrame(CurrentAudioFrame, DummySpectrogramTexture);
        
        // Passa as features completas para o transcritor
        float FrameDuration = (float)MonoSamples.Num() / ActualSampleRate; 
        Transcriber->ProcessAudioFeatures(ExtractedFeatures, CurrentAudioFrame->Timestamp, FrameDuration);
    }

    // Desliga e garante que as notas pendentes sejam finalizadas pelos processadores
    FeatureProcessor->Shutdown(); 
    Transcriber->Shutdown();     

    // Remove o bind do delegate. MUITO IMPORTANTE para evitar múltiplos binds em futuras conversões.
    Transcriber->OnMIDITranscriptionEventGenerated.Remove(TranscriberDelegateHandle);


    if (TranscribedMIDIEvents.Num() == 0)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Nenhum evento MIDI significativo transcrito para %s."), *AudioFilePath);
        return false;
    }

    // --- 3. Salvar Eventos MIDI Transcritos para Arquivo (.mid) ---
    smf::MidiFile MidiFile; 
    MidiFile.addTrack(1); 
    int32 TicksPerQuarterNote = 480; 
    MidiFile.setTicksPerQuarterNote(TicksPerQuarterNote);

    // Adiciona eventos à MidiFile
    for (const FIAR_MIDIEvent& Event : TranscribedMIDIEvents)
    {
        int32 Tick = FMath::RoundToInt(Event.Timestamp * TicksPerQuarterNote);
        std::vector<uchar> MidiDataBytes = { static_cast<uchar>(Event.Status), static_cast<uchar>(Event.Data1), static_cast<uchar>(Event.Data2) }; 
        MidiFile.addEvent(0, Tick, MidiDataBytes);
    }
    
    MidiFile.sortTracks();       
    MidiFile.makeDeltaTicks();   

    std::string MIDIOutputFilePathStd = TCHAR_TO_UTF8(*MIDIOutputFilePath);
    if (!MidiFile.write(MIDIOutputFilePathStd)) 
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao escrever arquivo MIDI: %s"), *MIDIOutputFilePath);
        return false;
    }

    UE_LOG(LogIARFolderSource, Log, TEXT("Conversão Áudio para MIDI bem-sucedida: %s -> %s"), *AudioFilePath, *MIDIOutputFilePath);
    return true;
}

/**
 * @brief Implementa a lógica para converter um arquivo MIDI (.mid) para áudio (.wav).
 * Utiliza UIARMIDIFileSource para carregar MIDI e UIARMIDIToAudioSynthesizer para sintetizar,
 * salvando o resultado como WAV usando UIARAudioEncoder.
 */
bool UIARFolderSource::ConvertMIDIToAudio(const FString& MIDIFilePath, const FString& AudioOutputFilePath)
{
    UE_LOG(LogIARFolderSource, Log, TEXT("Iniciando conversão MIDI para Áudio: %s -> %s"), *MIDIFilePath, *AudioOutputFilePath);

    // --- 1. Carregar Eventos MIDI do Arquivo ---
    UIARMIDIFileSource* MIDIFileSource = NewObject<UIARMIDIFileSource>();
    if (!MIDIFileSource)
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao criar UIARMIDIFileSource."));
        return false;
    }
    
    // Configurações para o stream MIDI (importante para SampleRate e NumChannels da síntese)
    FIAR_AudioStreamSettings SynthSettings; 
    SynthSettings.FilePath = MIDIFilePath; // Define o caminho do arquivo MIDI
    SynthSettings.SampleRate = 48000;      // Taxa de amostragem alvo para a síntese
    SynthSettings.NumChannels = 2;         // Canais alvo para a síntese (estéreo)
    MIDIFileSource->Initialize(SynthSettings, this->FramePool); // Inicializa o MIDIFileSource (sem FramePool pois não é áudio)

    // Carrega o arquivo MIDI de forma bloqueante (método síncrono para background thread)
    if (!MIDIFileSource->Internal_LoadMIDIFileBlocking())
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao carregar arquivo MIDI: %s"), *MIDIFilePath);
        return false;
    }

    // --- 2. Sintetizar Áudio a partir de Eventos MIDI ---
    UIARMIDIToAudioSynthesizer* Synthesizer = NewObject<UIARMIDIToAudioSynthesizer>();
    if (!Synthesizer)
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao criar UIARMIDIToAudioSynthesizer."));
        return false;
    }
    // Inicializa o sintetizador com as configurações de saída de áudio
    Synthesizer->Initialize(SynthSettings.SampleRate, SynthSettings.NumChannels);

    // Vetor para coletar os samples de áudio sintetizados
    TArray<float> SynthesizedRawAudio;
    // Adiciona uma lambda ao delegate do sintetizador para coletar os frames de áudio gerados
    Synthesizer->OnSynthesizedAudioFrameReady.AddLambda([&](const TArray<float>& AudioBuffer)
    {
        SynthesizedRawAudio.Append(AudioBuffer);
    });

    // Obtém os eventos MIDI carregados do MIDIFileSource
    const TArray<FIAR_MIDIEvent> LoadedEventsPtr = MIDIFileSource->GetLoadedMIDIEvents();
    if (LoadedEventsPtr.Num() == 0)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Nenhum evento MIDI carregado para sintetizar de %s."), *MIDIFilePath);
        Synthesizer->Shutdown(); // Limpa recursos do sintetizador
        return false;
    }

    // Processa eventos MIDI sequencialmente através do sintetizador, simulando a passagem do tempo.
    // O sintetizador gera áudio em chunks periódicos (AudioBufferInterval).
    // Precisamos garantir que o sintetizador processe os eventos no "tempo" correto.
    float CurrentSimulatedTime = 0.0f; // Tempo atual simulado para a síntese
    float SynthesizerProcessingInterval = Synthesizer->GetAudioBufferInterval(); // Intervalo de geração do sintetizador (e.g., 0.02s)

    // Itera sobre todos os eventos MIDI
    for (const FIAR_MIDIEvent& Event : LoadedEventsPtr)
    {
        // Gera áudio para o tempo decorrido até o evento atual
        float TimeUntilNextEvent = Event.Timestamp - CurrentSimulatedTime;
        if (TimeUntilNextEvent > 0)
        {
            // Calcula quantos "frames" de áudio o sintetizador precisa gerar para cobrir esse intervalo
            int32 FramesToGenerate = FMath::CeilToInt(TimeUntilNextEvent / SynthesizerProcessingInterval);
            for (int32 i = 0; i < FramesToGenerate; ++i)
            {
                Synthesizer->GenerateAudioBuffer(); // Dispara o delegate OnSynthesizedAudioFrameReady
            }
        }
        
        Synthesizer->ProcessMIDIEvent(Event); // Processa o evento MIDI atual no sintetizador
        CurrentSimulatedTime = Event.Timestamp; // Avança o tempo simulado para o timestamp do evento
    }

    // Gera áudio adicional no final para capturar as "caudas" de release de notas
    float TailDurationSeconds = 2.0f; // Adiciona 2 segundos de áudio no final
    int32 FramesForTail = FMath::CeilToInt(TailDurationSeconds / SynthesizerProcessingInterval);
    for (int32 i = 0; i < FramesForTail; ++i)
    {
        Synthesizer->GenerateAudioBuffer(); // Continua gerando áudio
    }

    Synthesizer->Shutdown(); // Finaliza o sintetizador e libera seus recursos

    if (SynthesizedRawAudio.Num() == 0)
    {
        UE_LOG(LogIARFolderSource, Warning, TEXT("Nenhum áudio sintetizado foi gerado para %s."), *MIDIFilePath);
        return false;
    }

    // --- 3. Salvar Áudio Sintetizado para Arquivo (.wav) ---
    // Conforme a diretriz, a saída de áudio será sempre em .wav (PCM 16-bit).
    if (!UIARAudioEncoder::EncodeRawPCMToFile(SynthesizedRawAudio, SynthSettings.SampleRate, SynthSettings.NumChannels, AudioOutputFilePath, 16))
    {
        UE_LOG(LogIARFolderSource, Error, TEXT("Falha ao codificar áudio sintetizado para WAV: %s"), *AudioOutputFilePath);
        return false;
    }

    UE_LOG(LogIARFolderSource, Log, TEXT("Conversão MIDI para Áudio bem-sucedida: %s -> %s"), *MIDIFilePath, *AudioOutputFilePath);
    return true;
}
