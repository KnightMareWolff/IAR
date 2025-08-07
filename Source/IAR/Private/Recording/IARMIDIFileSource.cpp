// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARMIDIFileSource.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "../GlobalStatics.h"
#include "Engine/World.h"

// Converte FString para std::string
#include <string>

UIARMIDIFileSource::UIARMIDIFileSource()
    : UIARMediaSource()
    , CurrentMIDIEventIndex(0)
    , bIsFileLoaded(false)
    , CurrentPlaybackTime(0.0f)
{
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Construtor chamado."));
}

UIARMIDIFileSource::~UIARMIDIFileSource()
{
    Shutdown();
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Destrutor chamado."));
}

void UIARMIDIFileSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    Super::Initialize(StreamSettings, InFramePool);

    if (!FramePool || !IsValid(FramePool))
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDIFileSource: FramePool inválido. Embora não seja essencial para MIDI, pode indicar um problema no pipeline de áudio."));
    }

    if (CurrentStreamSettings.FilePath.IsEmpty())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: FilePath vazio. Não é possível carregar arquivo MIDI."));
        return;
    }

    FullDiskFilePathInternal = FPaths::Combine(UIARGlobalStatics::GetIARRecordingRootPath(), CurrentStreamSettings.FilePath);
    FPaths::NormalizeFilename(FullDiskFilePathInternal);

    bIsFileLoaded = false;
    LoadedMIDIEvents.Empty();
    CurrentMIDIEventIndex = 0;
    CurrentPlaybackTime = 0.0f;

    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Inicializado para carregar arquivo MIDI '%s' (PATH: %s)."), *CurrentStreamSettings.FilePath, *FullDiskFilePathInternal);
}

void UIARMIDIFileSource::StartCapture()
{
    if (!bIsFileLoaded)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: StartCapture chamado antes do arquivo MIDI ser carregado. Verifique o fluxo da StartRecording do IARAudioComponent."));
        return;
    }

    if (bIsCapturing)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDIFileSource: Captura MIDI já está ativa."));
        return;
    }

    if (LoadedMIDIEvents.Num() == 0 || !GetWorld())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: Eventos MIDI carregados vazios ou World inválido. Não é possível iniciar a captura."));
        return;
    }

    Super::StartCapture();

    GetWorld()->GetTimerManager().SetTimer(
        MIDIPlaybackTimerHandle,
        this,
        &UIARMIDIFileSource::ProcessMIDIEventFrame,
        0.005f, // Intervalo de 5ms para despacho de eventos
        true // Repetir
    );
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Captura de eventos MIDI iniciada."));
}

void UIARMIDIFileSource::StopCapture()
{
    if (!bIsCapturing)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDIFileSource: Captura MIDI já está inativa."));
        return;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(MIDIPlaybackTimerHandle);
    }

    Super::StopCapture();
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Captura de eventos MIDI parada."));
}

void UIARMIDIFileSource::Shutdown()
{
    StopCapture();
    ResetFileSource();
    Super::Shutdown();
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Desligado e recursos liberados."));
}

void UIARMIDIFileSource::ResetFileSource()
{
    LoadedMIDIEvents.Empty();
    CurrentMIDIEventIndex = 0;
    bIsFileLoaded = false;
    FullDiskFilePathInternal = TEXT("");
    CurrentPlaybackTime = 0.0f;
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Fonte de arquivo MIDI resetada."));
}

bool UIARMIDIFileSource::Internal_LoadMIDIFileBlocking()
{
    if (bIsFileLoaded)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Eventos MIDI já carregados. Pulando carregamento."));
        return true;
    }

    if (FullDiskFilePathInternal.IsEmpty())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: Caminho do arquivo interno vazio. Não é possível carregar."));
        return false;
    }

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullDiskFilePathInternal))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: Arquivo MIDI não encontrado no caminho: %s. Verifique o FilePath relativo em suas StreamSettings."), *FullDiskFilePathInternal);
        return false;
    }

    std::string FilePathStd = TCHAR_TO_UTF8(*FullDiskFilePathInternal);
    smf::MidiFile MidiFile;

    if (!MidiFile.read(FilePathStd))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARMIDIFileSource: Falha ao ler o arquivo MIDI: %s"), *FullDiskFilePathInternal);
        return false;
    }

    // A biblioteca smf já lida com delta/absolute ticks e calcula o tempo em segundos.
    // Garantimos que esteja em ticks absolutos e fazemos a análise de tempo.
    MidiFile.makeAbsoluteTicks();
    MidiFile.doTimeAnalysis(); // Calcula os segundos para cada evento.

    LoadedMIDIEvents.Empty();
    for (int track = 0; track < MidiFile.getTrackCount(); ++track)
    {
        for (int i = 0; i < MidiFile[track].getEventCount(); ++i)
        {
            smf::MidiEvent& MidiEvent = MidiFile[track][i];

            // Ignoramos meta e sysex events para o sintetizador simples.
            // O sintetizador (UIARMIDIToAudioSynthesizer) espera apenas Note On/Off/Controller/PitchBend
            // e outros comandos de status. A biblioteca smf tem um método isTimbre() (patch change)
            // mas que é diferente da forma que o UIARMIDIToAudioSynthesizer está esperando (Command, P1, P2).
            if (MidiEvent.isMeta() || MidiEvent.isController() || MidiEvent.isTimbre() || MidiEvent.isPitchbend() || MidiEvent.isAftertouch())
            {
                // Ignorar meta events e outros para o sintetizador simples.
                // Eventos de Patch Change e Controller seriam tratados se o sintetizador os suportasse
                // e o objetivo fosse reprodução precisa do MIDI.
                if (MidiEvent.isNoteOn() || MidiEvent.isNoteOff()) {
                    FIAR_MIDIEvent NewEvent;
                    NewEvent.Status = MidiEvent.getCommandByte(); // Status (0x90, 0x80)
                    NewEvent.Data1 = MidiEvent.getP1(); // Note Number
                    NewEvent.Data2 = MidiEvent.getP2(); // Velocity (NoteOn), 0 (NoteOff)
                    NewEvent.Timestamp = MidiEvent.seconds; // Tempo em segundos
                    LoadedMIDIEvents.Add(NewEvent);
                } else if(MidiEvent.isController()) {
                    FIAR_MIDIEvent NewEvent;
                    NewEvent.Status = MidiEvent.getCommandByte();
                    NewEvent.Data1 = MidiEvent.getP1();
                    NewEvent.Data2 = MidiEvent.getP2();
                    NewEvent.Timestamp = MidiEvent.seconds;
                    LoadedMIDIEvents.Add(NewEvent);
                } else if (MidiEvent.isPitchbend()) {
                    FIAR_MIDIEvent NewEvent;
                    NewEvent.Status = MidiEvent.getCommandByte();
                    NewEvent.Data1 = MidiEvent.getP1();
                    NewEvent.Data2 = MidiEvent.getP2();
                    NewEvent.Timestamp = MidiEvent.seconds;
                    LoadedMIDIEvents.Add(NewEvent);
                } else if (MidiEvent.isTimbre()) { // Patch Change
                    FIAR_MIDIEvent NewEvent;
                    NewEvent.Status = MidiEvent.getCommandByte();
                    NewEvent.Data1 = MidiEvent.getP1();
                    NewEvent.Data2 = 0; // Patch change não tem Data2 no sentido de velocity
                    NewEvent.Timestamp = MidiEvent.seconds;
                    LoadedMIDIEvents.Add(NewEvent);
                } else if (MidiEvent.isAftertouch()) {
                    FIAR_MIDIEvent NewEvent;
                    NewEvent.Status = MidiEvent.getCommandByte();
                    NewEvent.Data1 = MidiEvent.getP1();
                    NewEvent.Data2 = MidiEvent.getP2();
                    NewEvent.Timestamp = MidiEvent.seconds;
                    LoadedMIDIEvents.Add(NewEvent);
                }
            } else if (!MidiEvent.isMeta() && MidiEvent.size() >= 3) // Mensagens de 3 bytes (Note On/Off, Controller, Pitch Bend)
            {
                FIAR_MIDIEvent NewEvent;
                NewEvent.Status = MidiEvent.getCommandByte();
                NewEvent.Data1 = MidiEvent.getP1();
                NewEvent.Data2 = MidiEvent.getP2();
                NewEvent.Timestamp = MidiEvent.seconds;
                LoadedMIDIEvents.Add(NewEvent);
            } else if (!MidiEvent.isMeta() && MidiEvent.size() == 2) // Mensagens de 2 bytes (Patch Change, Channel Pressure)
            {
                FIAR_MIDIEvent NewEvent;
                NewEvent.Status = MidiEvent.getCommandByte();
                NewEvent.Data1 = MidiEvent.getP1();
                NewEvent.Data2 = 0; // Assumimos 0 para o Data2, pois a mensagem só tem 2 bytes (ex: patch change)
                NewEvent.Timestamp = MidiEvent.seconds;
                LoadedMIDIEvents.Add(NewEvent);
            }
        }
    }

    if (LoadedMIDIEvents.Num() == 0)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDIFileSource: Nenhum evento MIDI significativo encontrado ou carregado do arquivo %s."), *FullDiskFilePathInternal);
        bIsFileLoaded = false;
        return false;
    }

    LoadedMIDIEvents.Sort([](const FIAR_MIDIEvent& A, const FIAR_MIDIEvent& B) {
        return A.Timestamp < B.Timestamp;
    });

    bIsFileLoaded = true;
    CurrentMIDIEventIndex = 0;
    CurrentPlaybackTime = 0.0f;

    UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Arquivo MIDI '%s' carregado com %d eventos."), *FullDiskFilePathInternal, LoadedMIDIEvents.Num());
    return true;
}

void UIARMIDIFileSource::ProcessMIDIEventFrame()
{
    if (!bIsCapturing || LoadedMIDIEvents.Num() == 0 || !GetWorld())
    {
        StopCapture();
        return;
    }

    CurrentPlaybackTime += GetWorld()->GetDeltaSeconds() * CurrentStreamSettings.PlaybackSpeed;
    
    TSharedPtr<FIAR_MIDIFrame> MIDIFrame = MakeShared<FIAR_MIDIFrame>();
    MIDIFrame->Timestamp = CurrentPlaybackTime;
    MIDIFrame->Duration = GetWorld()->GetDeltaSeconds(); 

    bool bEventsDispatchedInThisFrame = false;

    while (CurrentMIDIEventIndex < LoadedMIDIEvents.Num())
    {
        const FIAR_MIDIEvent& NextEvent = LoadedMIDIEvents[CurrentMIDIEventIndex];

        if (NextEvent.Timestamp <= CurrentPlaybackTime)
        {
            MIDIFrame->Events.Add(NextEvent);
            bEventsDispatchedInThisFrame = true;
            CurrentMIDIEventIndex++;
        }
        else
        {
            break;
        }
    }

    if (bEventsDispatchedInThisFrame)
    {
        OnMIDIFrameAcquired.Broadcast(MIDIFrame);
    }

    if (CurrentMIDIEventIndex >= LoadedMIDIEvents.Num())
    {
        if (CurrentStreamSettings.bLoopPlayback)
        {
            UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Loop de playback MIDI. Resetando eventos."));
            CurrentMIDIEventIndex = 0;
            CurrentPlaybackTime = 0.0f;
            GetWorld()->GetTimerManager().SetTimer(MIDIPlaybackTimerHandle, this, &UIARMIDIFileSource::ProcessMIDIEventFrame, 0.005f, true);
        }
        else
        {
            UE_LOG(LogIAR, Log, TEXT("UIARMIDIFileSource: Fim do arquivo MIDI. Parando a captura."));
            StopCapture();
        }
    }
}
