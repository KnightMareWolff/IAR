// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "AudioAnalysis/IARAudioToMIDITranscriber.h"
#include "../IAR.h"
#include "Math/UnrealMathUtility.h" 
#include "Kismet/GameplayStatics.h" 
#include "Containers/Set.h" // Para TSet
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogIARTranscriber);

UIARAudioToMIDITranscriber::UIARAudioToMIDITranscriber()
    : SampleRate(0)
    , SilenceThresholdRMS(0.005f)
    , PitchChangeThresholdHz(5.0f)
    , MinNoteDuration(0.05f)
    , NoteOffToleranceFrames(5) // Default to 5 frames of tolerance
{
    UE_LOG(LogIARTranscriber, Log, TEXT("UIARAudioToMIDITranscriber: Construtor chamado."));
}

UIARAudioToMIDITranscriber::~UIARAudioToMIDITranscriber()
{
    UE_LOG(LogIARTranscriber, Log, TEXT("UIARAudioToMIDITranscriber: Destrutor chamado."));
}

void UIARAudioToMIDITranscriber::Initialize(int32 InSampleRate)
{
    SampleRate = InSampleRate;
    PreviousPitchHz = 0.0f;
    ActiveNotesMap.Empty(); // Limpa todas as notas ativas
    NoteOffToleranceCounters.Empty(); // Limpa todos os contadores de tolerância
    UE_LOG(LogIARTranscriber, Log, TEXT("UIARAudioToMIDITranscriber: Inicializado com SampleRate: %d."), InSampleRate);
}

void UIARAudioToMIDITranscriber::ProcessAudioFeatures(const FIAR_AudioFeatures& AudioFeatures, float Timestamp, float FrameDuration)
{
    // Aplica suavização ao pitch estimado (para a nota principal ou como base)
    float CurrentPitchHz = AudioFeatures.PitchEstimate;
    if (PreviousPitchHz == 0.0f)
    {
        PreviousPitchHz = CurrentPitchHz;
    }
    else
    {
        CurrentPitchHz = FMath::Lerp(CurrentPitchHz, PreviousPitchHz, PitchSmoothingFactor);
        PreviousPitchHz = CurrentPitchHz;
    }

    // Conjunto para armazenar as notas detectadas NESTE frame
    TSet<int32> NotesDetectedInThisFrame;
    for (const FIAR_AudioNoteFeature& DetectedNote : AudioFeatures.DetectedNotes)
    {
        NotesDetectedInThisFrame.Add(DetectedNote.MIDINoteNumber);
    }

    // Lógica para detecção e manutenção de notas ativas
    // Percorre as notas que o IARAdvancedAudioFeatureProcessor (via FFT) detectou como "top notes"
    for (const FIAR_AudioNoteFeature& DetectedNoteFeature : AudioFeatures.DetectedNotes)
    {
        int32 CurrentMIDINote = DetectedNoteFeature.MIDINoteNumber;

        // <--- NOVO: Clampeia a nota MIDI para uma faixa audível mais razoável
        CurrentMIDINote = FMath::Clamp(CurrentMIDINote, 21, 108); // Ex: A0 (27.5 Hz) a C8 (4186 Hz)
        // NOVO FIM --->

        float NoteVelocity = FMath::Clamp(DetectedNoteFeature.Velocity * 127.0f, 0.0f, 127.0f); // Usar a "velocidade" da feature

        // Se o áudio está abaixo do limiar de silêncio para esta nota (opcional, ou o total RMS)
        // Por simplicidade, vamos usar o RMS geral para o silêncio.
        if (AudioFeatures.RMSAmplitude < SilenceThresholdRMS)
        {
            // O frame está em silêncio. Lógica de desligamento de notas será tratada abaixo.
            continue; 
        }

        // Caso 1: A nota já está ativa e foi detectada novamente
        if (ActiveNotesMap.Contains(CurrentMIDINote))
        {
            // Atualiza a nota ativa (pode ser útil para pitch bend ou outras modulações futuras)
            FIAR_AudioNoteFeature& ActiveNote = ActiveNotesMap.FindOrAdd(CurrentMIDINote);
            ActiveNote.Velocity = NoteVelocity; // Atualiza a velocidade
            ActiveNote.PitchHz = DetectedNoteFeature.PitchHz; // Atualiza o pitch

            // Reseta o contador de tolerância, mantendo a nota ligada
            NoteOffToleranceCounters.FindOrAdd(CurrentMIDINote) = NoteOffToleranceFrames;
        }
        // Caso 2: A nota é nova e foi detectada pela primeira vez
        else if (NoteVelocity > 0) // Só considera uma nova nota se tiver alguma velocidade
        {
            FIAR_AudioNoteFeature NewActiveNote = DetectedNoteFeature;
            NewActiveNote.MIDINoteNumber = CurrentMIDINote; // Garante que a nota clampeada seja usada
            NewActiveNote.StartTime = Timestamp;
            NewActiveNote.Duration = 0.0f; // Duração será calculada no Note Off
            NewActiveNote.Velocity = NoteVelocity; // Usa a velocidade detectada

            // Envia Note On
            FIAR_MIDIEvent NoteOnEvent;
            NoteOnEvent.Status = 0x90; 
            NoteOnEvent.Data1 = NewActiveNote.MIDINoteNumber;
            NoteOnEvent.Data2 = FMath::RoundToInt(NewActiveNote.Velocity);
            NoteOnEvent.Timestamp = Timestamp;
            OnMIDITranscriptionEventGenerated.Broadcast(NoteOnEvent);
            UE_LOG(LogIARTranscriber, Log, TEXT("MIDI Transcriber: Note ON - MIDI: %d, Vel: %d, Freq: %.2f Hz"), 
                NewActiveNote.MIDINoteNumber, FMath::RoundToInt(NewActiveNote.Velocity), NewActiveNote.PitchHz);

            ActiveNotesMap.Add(CurrentMIDINote, NewActiveNote);
            NoteOffToleranceCounters.Add(CurrentMIDINote, NoteOffToleranceFrames); // Inicia o contador de tolerância
        }
    }

    // Lógica para desligar notas (polyphonic Note Off)
    // Cria uma cópia das chaves para poder modificar o mapa durante a iteração
    TArray<int32> NotesCurrentlyActive;
    ActiveNotesMap.GetKeys(NotesCurrentlyActive);

    for (int32 ActiveMIDINote : NotesCurrentlyActive)
    {
        // Se o RMS geral está muito baixo, força o Note Off.
        // Ou se a nota não foi detectada neste frame.
        if (AudioFeatures.RMSAmplitude < SilenceThresholdRMS || !NotesDetectedInThisFrame.Contains(ActiveMIDINote))
        {
            int32& ToleranceCounter = NoteOffToleranceCounters.FindOrAdd(ActiveMIDINote);
            ToleranceCounter--; // Decrementa o contador de tolerância

            // Se o contador de tolerância chegou a zero, ou a nota está muito silenciosa
            if (ToleranceCounter <= 0)
            {
                FIAR_AudioNoteFeature& NoteToDeactivate = ActiveNotesMap.FindOrAdd(ActiveMIDINote);
                NoteToDeactivate.Duration = Timestamp - NoteToDeactivate.StartTime;

                // Envia Note Off se a duração mínima foi atingida
                if (NoteToDeactivate.Duration >= MinNoteDuration)
                {
                    FIAR_MIDIEvent NoteOffEvent;
                    NoteOffEvent.Status = 0x80; 
                    NoteOffEvent.Data1 = NoteToDeactivate.MIDINoteNumber;
                    NoteOffEvent.Data2 = 0; // Velocity 0
                    NoteOffEvent.Timestamp = Timestamp;
                    OnMIDITranscriptionEventGenerated.Broadcast(NoteOffEvent);
                    UE_LOG(LogIARTranscriber, Log, TEXT("MIDI Transcriber: Note OFF - MIDI: %d, Dur: %.3f"), 
                        NoteToDeactivate.MIDINoteNumber, NoteToDeactivate.Duration);
                }

                ActiveNotesMap.Remove(ActiveMIDINote);
                NoteOffToleranceCounters.Remove(ActiveMIDINote);
            }
        }
    }
}

void UIARAudioToMIDITranscriber::Shutdown()
{
    // Desliga todas as notas que ainda estão ativas no shutdown
    TArray<int32> NotesToTurnOff;
    ActiveNotesMap.GetKeys(NotesToTurnOff);

    for (int32 ActiveMIDINote : NotesToTurnOff)
    {
        FIAR_AudioNoteFeature& NoteToDeactivate = ActiveNotesMap.FindOrAdd(ActiveMIDINote);
        // NOVO: Verifica se o GetWorld() é válido para evitar crash no shutdown
        float CurrentTime = 0.0f;
        if (UWorld* World = GetWorld())
        {
            CurrentTime = World->GetTimeSeconds();
        }
        NoteToDeactivate.Duration = CurrentTime - NoteToDeactivate.StartTime;
        // NOVO FIM --->

        if (NoteToDeactivate.Duration >= MinNoteDuration)
        {
            FIAR_MIDIEvent NoteOffEvent;
            NoteOffEvent.Status = 0x80; 
            NoteOffEvent.Data1 = NoteToDeactivate.MIDINoteNumber;
            NoteOffEvent.Data2 = 0; 
            NoteOffEvent.Timestamp = CurrentTime; // Usa o CurrentTime obtido
            OnMIDITranscriptionEventGenerated.Broadcast(NoteOffEvent);
            UE_LOG(LogIARTranscriber, Log, TEXT("MIDI Transcriber: Note OFF (Shutdown) - MIDI: %d, Dur: %.3f"), 
                NoteToDeactivate.MIDINoteNumber, NoteToDeactivate.Duration);
        }
    }
    ActiveNotesMap.Empty();
    NoteOffToleranceCounters.Empty();
    PreviousPitchHz = 0.0f; 
    UE_LOG(LogIARTranscriber, Log, TEXT("UIARAudioToMIDITranscriber: Desligado."));
}

int32 UIARAudioToMIDITranscriber::FreqToMidi(float FreqHz) const
{
    if (FreqHz <= 0.0f) return 0; // Frequência inválida ou silenciosa, mapeia para MIDI 0 (C-1)

    int32 MidiNote = FMath::RoundToInt(69.0f + 12.0f * (FMath::Loge(FreqHz / 440.0f) / FMath::Loge(2.0f)));
    // <--- NOVO: Clampeia a nota MIDI aqui também (dupla proteção)
    MidiNote = FMath::Clamp(MidiNote, 21, 108); // Mantém dentro de uma faixa audível de A0 a C8
    // NOVO FIM --->
    return FMath::Clamp(MidiNote, 0, 127); 
}
