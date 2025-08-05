// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Core/IARKeyboard.h"
#include "../IAR.h" 
#include "Kismet/GameplayStatics.h" 
#include "TimerManager.h" 
#include "Engine/LatentActionManager.h" // Necess�rio para FLatentActionManager e FLatentActionInfo
#include "LatentActions.h" // Para FDelayAction
#include "DelayAction.h" // Para FDelayAction
#include "Engine/World.h"

// Adicione este include para usar FPlatformFileManager para verificar o arquivo
#include "HAL/PlatformFileManager.h"

// #include "Core/IARLambdaLatentAction.h" // J� inclu�do em IARKeyboard.h

UIARKeyboard::UIARKeyboard()
    : DefaultVelocity(90)
    , MIDIChannel(0) 
    , PlaybackTickRate(0.005f) // Inicializacaoo aqui tambem
    , InputMIDIDeviceController(nullptr)
    , OutputMIDIDeviceController(nullptr)
    , CurrentPlaybackEventIndex(0)
    , CurrentPlaybackTimeSeconds(0.0f)
    , bIsMidiPlaying(false)
{
    UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Construtor chamado."));
}

UIARKeyboard::~UIARKeyboard()
{
    UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Destrutor chamado."));
    // Desinscreve os delegates do controlador de entrada para evitar crashes
    if (InputMIDIDeviceController)
    {
        InputMIDIDeviceController->OnMIDINoteOn.RemoveDynamic(this, &UIARKeyboard::HandleReceivedNoteOn); 
        InputMIDIDeviceController->OnMIDINoteOff.RemoveDynamic(this, &UIARKeyboard::HandleReceivedNoteOff); 
    }
    // Garante que o timer de playback MIDI � limpo
    StopMidiFilePlayback(); 
    // Os controladores s�o UObjects e ser�o garbage-collected.
    InputMIDIDeviceController = nullptr;
    OutputMIDIDeviceController = nullptr;
}

bool UIARKeyboard::InitializeKeyboard(int32 InInputDeviceID, int32 InOutputDeviceID, uint8 InDefaultVelocity, uint8 InMIDIChannel)
{
    DefaultVelocity = FMath::Clamp(InDefaultVelocity, (uint8)0, (uint8)127);
    MIDIChannel = FMath::Clamp(InMIDIChannel, (uint8)0, (uint8)15);

    // Tenta criar o controlador de entrada, se um ID v�lido for fornecido
    if (InInputDeviceID != -1)
    {
        InputMIDIDeviceController = UMIDIDeviceManager::CreateMIDIDeviceInputController(InInputDeviceID);
        if (InputMIDIDeviceController)
        {
            UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Conectado ao dispositivo MIDI de entrada ID %d ('%s')."), InInputDeviceID, *InputMIDIDeviceController->GetDeviceName());
            // Inscreve os delegates para receber eventos MIDI de entrada
            InputMIDIDeviceController->OnMIDINoteOn.AddDynamic(this, &UIARKeyboard::HandleReceivedNoteOn); 
            InputMIDIDeviceController->OnMIDINoteOff.AddDynamic(this, &UIARKeyboard::HandleReceivedNoteOff); 
        }
        else
        {
            UE_LOG(LogIAR, Warning, TEXT("UIARKeyboard: Falha ao conectar ao dispositivo MIDI de entrada ID %d. Ele pode n�o existir ou j� estar em uso."), InInputDeviceID);
        }
    }

    // Tenta criar o controlador de sa�da
    if (InOutputDeviceID != -1)
    {
        OutputMIDIDeviceController = UMIDIDeviceManager::CreateMIDIDeviceOutputController(InOutputDeviceID);
        if (OutputMIDIDeviceController)
        {
            UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Conectado ao dispositivo MIDI de sa�da ID %d ('%s') no canal %d."), InOutputDeviceID, *OutputMIDIDeviceController->GetDeviceName(), MIDIChannel);
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Falha ao conectar ao dispositivo MIDI de sa�da ID %d. Ele pode n�o existir ou j� estar em uso. N�o ser� poss�vel enviar notas MIDI."), InOutputDeviceID);
            return false; 
        }
    }
    else 
    {
        TArray<FString> OutputNames;
        TArray<FFoundMIDIDevice> OutMIDIDevices;
        
        UMIDIDeviceManager::FindMIDIDevices(OutMIDIDevices);
        for (auto Device : OutMIDIDevices)
        {
            if (Device.bCanSendTo)
            {
                OutputNames.Add(Device.DeviceName);
            }
        }

        if (OutputNames.Num() > 0)
        {
            OutputMIDIDeviceController = UMIDIDeviceManager::CreateMIDIDeviceOutputController(0); 
            if (OutputMIDIDeviceController)
            {
                UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Conectado ao dispositivo MIDI de sa�da padr�o ID 0 ('%s') no canal %d."), *OutputMIDIDeviceController->GetDeviceName(), MIDIChannel);
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Nenhum dispositivo de sa�da especificado e falha ao conectar ao padr�o ID 0. N�o ser� poss�vel enviar notas MIDI."));
                return false;
            }
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Nenhum dispositivo de sa�da MIDI dispon�vel. N�o ser� poss�vel enviar notas MIDI."));
            return false;
        }
    }

    ActiveNotes.Empty(); 
    return true; 
}

void UIARKeyboard::GetAvailableMIDIDeviceNames(TArray<FString>& OutInputDeviceNames, TArray<FString>& OutOutputDeviceNames)
{
    OutInputDeviceNames.Empty();
    OutOutputDeviceNames.Empty();

    TArray<FString> TempInputNames;
    TArray<FString> TempOutputNames;
    TArray<FFoundMIDIDevice> OutMIDIDevices;
        
    UMIDIDeviceManager::FindMIDIDevices(OutMIDIDevices);
    for (auto Device : OutMIDIDevices)
    {
        if (Device.bCanReceiveFrom)
        {
            TempInputNames.Add(Device.DeviceName);
        }
        if (Device.bCanSendTo)
        {
            TempOutputNames.Add(Device.DeviceName);
        }
    }

    for (int32 i = 0; i < TempInputNames.Num(); ++i)
    {
        OutInputDeviceNames.Add(FString::Printf(TEXT("[%d] %s"), i, *TempInputNames[i]));
    }
    for (int32 i = 0; i < TempOutputNames.Num(); ++i)
    {
        OutOutputDeviceNames.Add(FString::Printf(TEXT("[%d] %s"), i, *TempOutputNames[i]));
    }
}


void UIARKeyboard::PressNote(int32 MIDINoteNumber, uint8 InVelocity)
{
    MIDINoteNumber = FMath::Clamp(MIDINoteNumber, 0, 127);
    uint8 ActualVelocity = (InVelocity == 0) ? DefaultVelocity : InVelocity;
    ActualVelocity = FMath::Clamp(ActualVelocity, (uint8)1, (uint8)127); 

    if (!OutputMIDIDeviceController)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARKeyboard: Controlador MIDI de sa�da n�o inicializado. N�o � poss�vel enviar Note On."));
        return;
    }

    if (!ActiveNotes.Contains(MIDINoteNumber))
    {
        OutputMIDIDeviceController->SendMIDINoteOn(MIDIChannel, MIDINoteNumber, ActualVelocity);
        ActiveNotes.Add(MIDINoteNumber);
        BroadcastMIDIEvent(0x90, (uint8)MIDINoteNumber, ActualVelocity); 
        UE_LOG(LogIAR, Verbose, TEXT("UIARKeyboard: Sent Note ON - MIDI: %d, Velocity: %d, Channel: %d"), MIDINoteNumber, ActualVelocity, MIDIChannel);
    }
}

void UIARKeyboard::ReleaseNote(int32 MIDINoteNumber)
{
    MIDINoteNumber = FMath::Clamp(MIDINoteNumber, 0, 127);

    if (!OutputMIDIDeviceController)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARKeyboard: Controlador MIDI de sa�da n�o inicializado. N�o � poss�vel enviar Note Off."));
        return;
    }

    if (ActiveNotes.Contains(MIDINoteNumber))
    {
        OutputMIDIDeviceController->SendMIDINoteOff(MIDIChannel, MIDINoteNumber, 0); 
        ActiveNotes.Remove(MIDINoteNumber);
        BroadcastMIDIEvent(0x80, (uint8)MIDINoteNumber, 0); 
        UE_LOG(LogIAR, Verbose, TEXT("UIARKeyboard: Sent Note OFF - MIDI: %d, Channel: %d"), MIDINoteNumber, MIDIChannel);
    }
}

void UIARKeyboard::PlayNoteOnce(UObject* WorldContextObject, FLatentActionInfo LatentInfo, int32 MIDINoteNumber, float DurationSeconds, uint8 InVelocity, int32 InOutputDeviceID, uint8 InMIDIChannel)
{
    if (!WorldContextObject || !WorldContextObject->GetWorld())
    {
        UE_LOG(LogIAR, Error, TEXT("PlayNoteOnce: WorldContextObject ou World inv�lido."));
        return;
    }

    // Criamos uma inst�ncia tempor�ria do teclado para esta fun��o est�tica.
    // O GetTransientPackage() garante que este objeto ser� garbage-collected automaticamente.
    UIARKeyboard* TempKeyboard = NewObject<UIARKeyboard>(GetTransientPackage(), UIARKeyboard::StaticClass());
    if (TempKeyboard)
    {
        if (!TempKeyboard->InitializeKeyboard(-1, InOutputDeviceID, InVelocity, InMIDIChannel))
        {
            UE_LOG(LogIAR, Error, TEXT("PlayNoteOnce: Falha ao inicializar teclado tempor�rio para tocar nota."));
            return;
        }

        TempKeyboard->PressNote(MIDINoteNumber, InVelocity);

        FLatentActionManager& LatentManager = WorldContextObject->GetWorld()->GetLatentActionManager();
        
        // A��O 1: O DELAY.
        // FDelayAction � uma FPendingLatentAction que espera por um tempo.
        // Ela usar� LatentInfo.ExecutionFunction e LatentInfo.Linkage como o "pr�ximo pino" quando terminar o delay.
        if (DurationSeconds > 0.0f)
        {
            // FDelayAction requer WorldContextObject, UUID e FLatentActionInfo para a conclus�o.
            LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(DurationSeconds, LatentInfo));
        }

        // A��O 2: A LIBERA��O DA NOTA.
        // Esta a��o ser� executada *imediatamente* ap�s a A��O 1 (o delay) terminar,
        // porque a A��O 1 est� configurada para triggar o mesmo pino de execu��o (LatentInfo.ExecutionFunction/Linkage)
        // que esta A��O 2 est� "escutando".
        LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FIAR_LambdaLatentAction(
            [TempKeyboard, MIDINoteNumber]() // Esta lambda libera a nota
            {
                if (IsValid(TempKeyboard)) // Garante que o TempKeyboard n�o foi destru�do pelo GC.
                {
                    TempKeyboard->ReleaseNote(MIDINoteNumber);
                }
                // O TempKeyboard ser� garbage-collected quando n�o houver mais refer�ncias.
                // N�o � necess�rio delet�-lo manualmente aqui, pois � um UObject.
            },
            LatentInfo // Passa a FLatentActionInfo completa
        ));
    }
}

bool UIARKeyboard::PlayMidiFile(const FString& FilePath)
{
    // Se j� estiver tocando, pare a reprodu��o anterior.
    if (bIsMidiPlaying)
    {
        StopMidiFilePlayback();
    }

    // 1. Verificar se o dispositivo de sa�da � v�lido
    if (!OutputMIDIDeviceController)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Controlador MIDI de sa�da n�o inicializado. N�o � poss�vel reproduzir arquivo MIDI."));
        return false;
    }

    // 2. Verificar se o arquivo existe
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Arquivo MIDI n�o encontrado: %s"), *FilePath);
        return false;
    }

    // 3. Carregar o arquivo MIDI usando smf::MidiFile
    // Use o namespace smf para a classe MidiFile
    // smf::MidiFile LoadedMidiFile; // N�o crie aqui, use o membro da classe
    std::string FilePathStd = TCHAR_TO_UTF8(*FilePath);

    if (!LoadedMidiFile.read(FilePathStd))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARKeyboard: Falha ao ler o arquivo MIDI: %s"), *FilePath);
        return false;
    }

    // 4. Converter para ticks absolutos e fazer an�lise de tempo para obter timestamps em segundos
    LoadedMidiFile.makeAbsoluteTicks();
    LoadedMidiFile.doTimeAnalysis(); // Isso popula o campo 'seconds' dos MidiEvents

    // 5. Popular a lista PlaybackEvents
    PlaybackEvents.Empty();
    for (int track = 0; track < LoadedMidiFile.getTrackCount(); ++track)
    {
        for (int i = 0; i < LoadedMidiFile[track].getEventCount(); ++i)
        {
            smf::MidiEvent& MidiEvent = LoadedMidiFile[track][i];

            // Ignorar meta-mensagens e mensagens vazias, ou converter apenas os tipos relevantes para reprodu��o
            if (MidiEvent.isMeta() || MidiEvent.size() == 0) 
            {
                continue;
            }

            // Mapear smf::MidiEvent para FIAR_MIDIEvent
            FIAR_MIDIEvent NewEvent;
            NewEvent.Status = MidiEvent.getCommandByte(); 
            // Para mensagens com 2 bytes de dados (Note On/Off, Controller, Pitch Bend)
            if (MidiEvent.size() >= 3)
            {
                NewEvent.Data1 = MidiEvent.getP1();           
                NewEvent.Data2 = MidiEvent.getP2();           
            }
            // Para mensagens com 1 byte de dado (Patch Change, Channel Pressure)
            else if (MidiEvent.size() == 2)
            {
                NewEvent.Data1 = MidiEvent.getP1();
                NewEvent.Data2 = 0; // Data2 � 0 para mensagens de 2 bytes sem segundo par�metro
            }
            else // Mensagens de 1 byte (System Common, Realtime)
            {
                NewEvent.Data1 = 0; // N�o h� Data1
                NewEvent.Data2 = 0; // N�o h� Data2
            }
            NewEvent.Timestamp = MidiEvent.seconds;       

            PlaybackEvents.Add(NewEvent);
        }
    }

    // Ordenar PlaybackEvents por Timestamp para garantir a ordem correta
    PlaybackEvents.Sort([](const FIAR_MIDIEvent& A, const FIAR_MIDIEvent& B) {
        return A.Timestamp < B.Timestamp;
    });

    // 6. Iniciar a reprodu��o
    CurrentPlaybackEventIndex = 0;
    CurrentPlaybackTimeSeconds = 0.0f;
    CurrentMidiFilePath = FilePath; // Guarda o path do arquivo que est� sendo tocado
    bIsMidiPlaying = true;

    // Inicia o timer para despachar os eventos
    GetWorld()->GetTimerManager().SetTimer(
        MidiPlaybackTimerHandle,
        this,
        &UIARKeyboard::ProcessMidiPlayback,
        PlaybackTickRate, // Use a propriedade PlaybackTickRate aqui
        true // Repetir
    );

    UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Iniciada reprodu��o de arquivo MIDI: %s"), *FilePath);
    return true;
}

void UIARKeyboard::StopMidiFilePlayback()
{
    if (!bIsMidiPlaying)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MidiPlaybackTimerHandle);
    }

    // Resetar o estado
    PlaybackEvents.Empty();
    CurrentPlaybackEventIndex = 0;
    CurrentPlaybackTimeSeconds = 0.0f;
    bIsMidiPlaying = false;

    UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Reprodu��o de arquivo MIDI parada: %s"), *CurrentMidiFilePath);
    OnMidiFilePlaybackCompleted.Broadcast(CurrentMidiFilePath); // Dispara o delegate de conclus�o
    CurrentMidiFilePath = TEXT("");
}


void UIARKeyboard::ProcessMidiPlayback()
{
    if (!bIsMidiPlaying || PlaybackEvents.Num() == 0 || !GetWorld())
    {
        StopMidiFilePlayback();
        return;
    }

    // CORRE��O AQUI: Avance o tempo de reprodu��o pelo intervalo FIXO do timer
    CurrentPlaybackTimeSeconds += PlaybackTickRate; 

    // Despacha todos os eventos que j� deveriam ter sido tocados
    while (CurrentPlaybackEventIndex < PlaybackEvents.Num())
    {
        const FIAR_MIDIEvent& EventToPlay = PlaybackEvents[CurrentPlaybackEventIndex];

        if (EventToPlay.Timestamp <= CurrentPlaybackTimeSeconds)
        {
            // O evento deve ser tocado agora
            if (OutputMIDIDeviceController)
            {
                uint8 StatusByte = EventToPlay.Status;
                // Para mensagens de canal (0x8x, 0x9x, 0xAx, 0xBx, 0xCx, 0xDx, 0xEx), o canal est� nos 4 bits menos significativos
                uint8 Channel = StatusByte & 0x0F; 

                // Determina o tipo de mensagem MIDI com base no nibble superior do byte de status
                switch (StatusByte & 0xF0) // M�scara para pegar apenas o nibble superior (tipo de mensagem)
                {
                    case 0x90: // Note On (0x90-0x9F)
                        // Um Note On com velocidade 0 � considerado um Note Off
                        if (EventToPlay.Data2 > 0) 
                        {
                            // NOME CORRIGIDO: SendNoteOn (sem "MIDI" no prefixo)
                            OutputMIDIDeviceController->SendMIDINoteOn(Channel, EventToPlay.Data1, EventToPlay.Data2);
                            UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Note ON - MIDI: %d, Vel: %d, Chan: %d"), EventToPlay.Data1, EventToPlay.Data2, Channel);
                        }
                        else 
                        {
                            // NOME CORRIGIDO: SendNoteOff (sem "MIDI" no prefixo)
                            OutputMIDIDeviceController->SendMIDINoteOff(Channel, EventToPlay.Data1, 0); // Velocity 0 para Note Off
                            UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Note OFF (Vel 0) - MIDI: %d, Chan: %d"), EventToPlay.Data1, Channel);
                        }
                        break;
                    case 0x80: // Note Off (0x80-0x8F)
                        // NOME CORRIGIDO: SendNoteOff (sem "MIDI" no prefixo)
                        OutputMIDIDeviceController->SendMIDINoteOff(Channel, EventToPlay.Data1, EventToPlay.Data2); // Data2 aqui � a release velocity, geralmente 0
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Note OFF - MIDI: %d, RelVel: %d, Chan: %d"), EventToPlay.Data1, EventToPlay.Data2, Channel);
                        break;
                    case 0xB0: // Control Change (0xB0-0xBF)
                        // NOME CORRIGIDO: SendControlChange (sem "MIDI" no prefixo)
                        OutputMIDIDeviceController->SendMIDIControlChange(Channel, EventToPlay.Data1, EventToPlay.Data2);
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Control Change - Ctrl: %d, Val: %d, Chan: %d"), EventToPlay.Data1, EventToPlay.Data2, Channel);
                        break;
                    case 0xC0: // Program Change / Patch Change (0xC0-0xCF)
                        // NOME CORRIGIDO: SendProgramChange (n�o SendMIDIPatchChange)
                        OutputMIDIDeviceController->SendMIDIProgramChange(Channel, EventToPlay.Data1); // Data1 � o n�mero do Program/Patch
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Patch Change - Patch: %d, Chan: %d"), EventToPlay.Data1, Channel);
                        break;
                    case 0xD0: // Channel Aftertouch / Pressure (0xD0-0xDF)
                        // NOME CORRIGIDO: SendChannelAftertouch (n�o SendMIDIAftertouch)
                        OutputMIDIDeviceController->SendMIDIChannelAftertouch(Channel, EventToPlay.Data1); // Data1 � a Press�o do canal
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Channel Aftertouch - Pressure: %d, Chan: %d"), EventToPlay.Data1, Channel);
                        break;
                    case 0xE0: // Pitch Bend (0xE0-0xEF)
                    { // <-- Bloco de escopo para a vari�vel PitchValue
                        // O pitch bend MIDI � um valor de 14 bits (Data1 = LSB, Data2 = MSB)
                        // A fun��o SendPitchBend espera um �nico inteiro de 14 bits.
                        // NOME CORRIGIDO: SendPitchBend (sem "MIDI" no prefixo)
                        int32 PitchValue = (EventToPlay.Data2 << 7) | EventToPlay.Data1;
                        OutputMIDIDeviceController->SendMIDIPitchBend(Channel, PitchValue);
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Pitch Bend - Value: %d (14-bit), Chan: %d"), PitchValue, Channel);
                    } 
                        break;
                    case 0xF0: // Mensagens de Sistema (SysEx, MTC, etc.) e Tempo Real (Clock, Start, Stop, etc.)
                        // NOME CORRIGIDO: SendGenericMsg (n�o SendMIDIGenericMsg)
                        // Este m�todo � mais gen�rico e funciona para mensagens de sistema e tempo real.
                        //OutputMIDIDeviceController->SendGenericMsg(StatusByte, EventToPlay.Data1, EventToPlay.Data2);
                        UE_LOG(LogIAR, Verbose, TEXT("MIDI Playback: Sent Generic System Message - Status: 0x%X, Data1: %d, Data2: %d"), StatusByte, EventToPlay.Data1, EventToPlay.Data2);
                        break;
                    default:
                        // Para qualquer outro status n�o reconhecido explicitamente.
                        // Usamos SendGenericMsg como fallback, pois ele � flex�vel.
                        //OutputMIDIDeviceController->SendGenericMsg(StatusByte, EventToPlay.Data1, EventToPlay.Data2);
                        UE_LOG(LogIAR, Warning, TEXT("MIDI Playback: Unhandled/Generic MIDI Message Type - Status: 0x%X. Sent via generic message."), StatusByte);
                        break;
                }
                
                // Dispara o delegate interno (e para logs) ap�s enviar para o dispositivo
                BroadcastMIDIEvent(EventToPlay.Status, EventToPlay.Data1, EventToPlay.Data2); 
            }
            CurrentPlaybackEventIndex++;
        }
        else
        {
            // O pr�ximo evento ainda n�o chegou ao seu tempo, ent�o paramos de processar eventos por agora
            break;
        }
    }

    // Se todos os eventos foram despachados, pare a reprodu��o
    if (CurrentPlaybackEventIndex >= PlaybackEvents.Num())
    {
        UE_LOG(LogIAR, Log, TEXT("UIARKeyboard: Todos os eventos do arquivo MIDI reproduzidos."));
        StopMidiFilePlayback(); // Isso tamb�m vai disparar o delegate de conclus�o e limpar o estado
    }
}



// Handler para eventos MIDI Note ON recebidos do UMIDIDeviceInputController
void UIARKeyboard::HandleReceivedNoteOn(UMIDIDeviceInputController* MIDIDeviceController, int32 Timestamp, int32 Channel, int32 Note, int32 Velocity)
{
    uint8 MidiNote = FMath::Clamp((uint8)Note, (uint8)0, (uint8)127);
    uint8 MidiVelocity = FMath::Clamp((uint8)Velocity, (uint8)0, (uint8)127); // Velocity j� vem como int32, clamp para uint8
// Podemos logar ou simplesmente retransmitir via nosso delegate
    // UE_LOG(LogIAR, Log, TEXT("MIDI IN: Note ON - Note: %d, Vel: %d, Device: %d, Channel: %d"), Note, MidiVelocity, MIDIDeviceController->GetDeviceID(), Channel);
    
    // Transmite o evento para o pipeline interno do IAR
    BroadcastMIDIEvent(0x90, MidiNote, MidiVelocity);
}

// Handler para eventos MIDI Note OFF recebidos do UMIDIDeviceInputController
void UIARKeyboard::HandleReceivedNoteOff(UMIDIDeviceInputController* MIDIDeviceController, int32 Timestamp, int32 Channel, int32 Note, int32 Velocity)
{
    uint8 MidiNote = FMath::Clamp((uint8)Note, (uint8)0, (uint8)127);
    // uint8 MidiVelocity = FMath::Clamp((uint8)Velocity, (uint8)0, (uint8)127); // NoteOff velocity is often 0, though a release velocity can exist.
    // UE_LOG(LogIAR, Log, TEXT("MIDI IN: Note OFF - Note: %d, Device: %d, Channel: %d"), Note, MIDIDeviceController->GetDeviceID(), Channel);
// Transmite o evento para o pipeline interno do IAR (Velocity 0 para Note Off)
    BroadcastMIDIEvent(0x80, MidiNote, 0); 
}

void UIARKeyboard::BroadcastMIDIEvent(uint8 Status, uint8 Data1, uint8 Data2)
{
    FIAR_MIDIEvent NewMIDIEvent;
    NewMIDIEvent.Status = Status;
    NewMIDIEvent.Data1 = Data1;
    NewMIDIEvent.Data2 = Data2;
    NewMIDIEvent.Timestamp = UGameplayStatics::GetTimeSeconds(this);

    OnMIDIEventGenerated.Broadcast(NewMIDIEvent);

    FIAR_DiatonicNoteEntry NoteEntry = UIARMIDITable::GetNoteEntryByMIDIPitch(Data1);
    if (Status == 0x90 && Data2 > 0) 
    {
        UE_LOG(LogIAR, Log, TEXT("MIDI Event (IAR Internal): Note On - %s%d (MIDI: %d, Freq: %.2f Hz, Vel: %d, Chan: %d)"),
            *NoteEntry.NoteName, NoteEntry.Octave, NoteEntry.NotePitch, NoteEntry.Frequency, Data2, MIDIChannel);
    }
    else if (Status == 0x80 || (Status == 0x90 && Data2 == 0)) 
    {
        UE_LOG(LogIAR, Log, TEXT("MIDI Event (IAR Internal): Note Off - %s%d (MIDI: %d, Chan: %d)"),
            *NoteEntry.NoteName, NoteEntry.Octave, NoteEntry.NotePitch, MIDIChannel);
    }
}
