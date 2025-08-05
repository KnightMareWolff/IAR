// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------

#include "Recording/IARECFactory.h"
#include "../IAR.h"
#include "HAL/PlatformProcess.h" // Para FPlatformProcess::IsWindows() etc.
#include "Misc/Paths.h"          // Para FPaths::ConvertRelativePathToFull

UIARECFactory::UIARECFactory()
{
    // Lógica do Construtor
    UE_LOG(LogIAR, Log, TEXT("IARECFactory: Construtor chamado."));
}

UIARECFactory::~UIARECFactory()
{
    // Lógica do Destrutor (para limpeza de não-UObjects, se houver)
    UE_LOG(LogIAR, Log, TEXT("IARECFactory: Desinicializando..."));
}

void UIARECFactory::Initialize()
{
    // Lógica de Inicialização
    UE_LOG(LogIAR, Log, TEXT("IARECFactory: Inicializado com sucesso."));
}

void UIARECFactory::Shutdown()
{
    // Lógica de Desligamento
    UE_LOG(LogIAR, Log, TEXT("IARECFactory: Desligado."));
}

FString UIARECFactory::BuildAudioEncodeCommand(const FIAR_AudioStreamSettings& StreamSettings, const FString& InputPipeName, const FString& OutputFilePath)
{
    FString QuotedInputPipeName;
#if PLATFORM_WINDOWS
    // Para Windows, FFmpeg espera \.\pipe\PipeName. Aspas para segurança.
    QuotedInputPipeName = FString::Printf(TEXT("%s"), *InputPipeName); 
#else // POSIX (Linux, Mac)
    // Para POSIX, FIFOs são caminhos de arquivo, devem ser entre aspas.
    QuotedInputPipeName = FString::Printf(TEXT(""%s""), *FPaths::ConvertRelativePathToFull(InputPipeName));
#endif

    // Constrói a linha de comando parte a parte
    // Adicionados -probesize e -analyzeduration para otimizar entrada de stream
    FString CommandLine = FString::Printf(TEXT("-f s16le -ar %d -ac %d -probesize 32 -analyzeduration 0 -thread_queue_size 8192 -i %s"), 
        StreamSettings.SampleRate, StreamSettings.NumChannels, *QuotedInputPipeName);

    // Adiciona codec de saída
    CommandLine += TEXT(" -c:a pcm_s16le"); // Espaço antes de -c:a

    // Caminho do arquivo de saída (entre aspas para segurança)
    CommandLine += FString::Printf(TEXT(" %s"), *FPaths::ConvertRelativePathToFull(OutputFilePath)); 

    UE_LOG(LogIAR, Log, TEXT("FFmpeg Command Generated: ffmpeg %s"), *CommandLine);

    return CommandLine;
}

FString UIARECFactory::BuildKillProcessCommand(int32 ProcessId)
{
    if (ProcessId <= 0)
    {
        return TEXT(""); // Processo inválido
    }
    
    FString Command;
#if PLATFORM_WINDOWS
    // Para Windows: taskkill /F /PID <ProcessId>
    Command = FString::Printf(TEXT("taskkill /F /PID %d"), ProcessId);
#elif PLATFORM_LINUX || PLATFORM_MAC
    // Para Linux/Mac: kill <ProcessId>
    Command = FString::Printf(TEXT("kill %d"), ProcessId);
#else
    UE_LOG(LogIAR, Warning, TEXT("BuildKillProcessCommand não implementado para esta plataforma."));
    Command = TEXT("");
#endif
    UE_LOG(LogIAR, Log, TEXT("Kill Process Command Generated: %s"), *Command);
    return Command;
}

FString UIARECFactory::BuildAudioConversionCommand(const FString& InSourceAudioPath, const FString& OutConvertedAudioPath, const FIAR_AudioConversionSettings& ConversionSettings)
{
    FString CommandLine;

    // Adiciona o flag -y para sobrescrever o arquivo de saída sem perguntar
    CommandLine += TEXT("-y ");

    // Parâmetro de entrada
    CommandLine += FString::Printf(TEXT("-i %s "), *FPaths::ConvertRelativePathToFull(InSourceAudioPath)); // Aspas para segurança

    // Parâmetros de áudio
    FString CodecArgs = TEXT("");
    if (ConversionSettings.Codec.Equals(TEXT("MP3"), ESearchCase::IgnoreCase))
    {
        CodecArgs = FString::Printf(TEXT("-c:a libmp3lame -b:a %d"), ConversionSettings.Bitrate);
    }
    else if (ConversionSettings.Codec.Equals(TEXT("AAC"), ESearchCase::IgnoreCase))
    {
        CodecArgs = FString::Printf(TEXT("-c:a aac -b:a %d"), ConversionSettings.Bitrate);
    }
    else if (ConversionSettings.Codec.Equals(TEXT("FLAC"), ESearchCase::IgnoreCase))
    {
        CodecArgs = TEXT("-c:a flac"); // FLAC é lossless, bitrate é menos relevante
    }
    else if (ConversionSettings.Codec.Equals(TEXT("PCM"), ESearchCase::IgnoreCase))
    {
        // Para PCM, assumimos WAV como contêiner, forçamos pcm_s16le
        CodecArgs = TEXT("-c:a pcm_s16le");
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARECFactory: Codec de conversão '%s' não suportado. Usando padrão (AAC)."), *ConversionSettings.Codec);
        CodecArgs = FString::Printf(TEXT("-c:a aac -b:a %d"), ConversionSettings.Bitrate);
    }
    CommandLine += CodecArgs;

    // Opções adicionais de sample rate e canais (se diferentes de 0)
    if (ConversionSettings.SampleRate > 0)
    {
        CommandLine += FString::Printf(TEXT(" -ar %d"), ConversionSettings.SampleRate);
    }
    if (ConversionSettings.NumChannels > 0)
    {
        CommandLine += FString::Printf(TEXT(" -ac %d"), ConversionSettings.NumChannels);
    }

    // Otimização para tamanho (para codecs lossy)
    if (ConversionSettings.bOptimizeForSize && (ConversionSettings.Codec.Equals(TEXT("MP3"), ESearchCase::IgnoreCase) || ConversionSettings.Codec.Equals(TEXT("AAC"), ESearchCase::IgnoreCase)))
    {
        CommandLine += TEXT(" -q:a 0"); // Alta qualidade para MP3/AAC para menor tamanho, mas pode ser lento.
    }

    // Caminho do arquivo de saída (com aspas para segurança)
    CommandLine += FString::Printf(TEXT(" %s"), *FPaths::ConvertRelativePathToFull(OutConvertedAudioPath));

    UE_LOG(LogIAR, Log, TEXT("FFmpeg Conversion Command Generated: ffmpeg %s"), *CommandLine);

    return CommandLine;
}
