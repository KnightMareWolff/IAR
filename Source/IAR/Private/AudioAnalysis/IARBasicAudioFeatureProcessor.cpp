// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "AudioAnalysis/IARBasicAudioFeatureProcessor.h"
#include "Core/IARMIDITable.h" // Inclui a tabela MIDI para uso
#include "../IAR.h" // Para LogIAR
#include "Math/UnrealMathUtility.h" // Para FMath
#include "Algo/Accumulate.h" // Para Algo::Accumulate
#include "Containers/Map.h" // Para contagem de frequ�ncia de notas
// A inclus�o de "Engine/Texture2D.h" n�o � mais estritamente necess�ria aqui se for apenas um par�metro de sa�da n�o usado,
// mas a assinatura precisa ser respeitada.

UIARBasicAudioFeatureProcessor::UIARBasicAudioFeatureProcessor()
{
    UE_LOG(LogIAR, Log, TEXT("UIARBasicAudioFeatureProcessor: Construtor chamado."));
}

UIARBasicAudioFeatureProcessor::~UIARBasicAudioFeatureProcessor()
{
    UE_LOG(LogIAR, Log, TEXT("UIARBasicAudioFeatureProcessor: Destrutor chamado."));
}

void UIARBasicAudioFeatureProcessor::Initialize()
{
    Super::Initialize(); // Chama a inicializa��o da classe base
    DetectedNotesHistory.Empty();
    CurrentBuildingNote = FIAR_AudioNoteFeature();
    NoteOnsetTimestamp = 0.0f;
    bIsNoteActive = false;
    UE_LOG(LogIAR, Log, TEXT("UIARBasicAudioFeatureProcessor: Inicializado com sucesso."));
}

void UIARBasicAudioFeatureProcessor::Shutdown()
{
    Super::Shutdown(); // Chama o desligamento da classe base
    DetectedNotesHistory.Empty();
    UE_LOG(LogIAR, Log, TEXT("UIARBasicAudioFeatureProcessor: Desligado."));
}

// SINTAXE CORRIGIDA: Explicitando ESPMode::ThreadSafe e mantendo UTexture2D*&
FIAR_AudioFeatures UIARBasicAudioFeatureProcessor::ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture)
{
    FIAR_AudioFeatures Features;

    // Nesta implementa��o b�sica, n�o geramos um espectrograma, ent�o OutSpectrogramTexture � nullptr.
    // Isso � seguro, pois � uma refer�ncia a um ponteiro, e atribuir nullptr a ele � v�lido.
    OutSpectrogramTexture = nullptr;

    // Valida��o do frame de �udio
    if (!AudioFrame.IsValid() || !AudioFrame->RawSamplesPtr.IsValid() || AudioFrame->RawSamplesPtr->Num() == 0)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARBasicAudioFeatureProcessor: Frame de �udio inv�lido ou vazio recebido para processamento."));
        return Features; // Retorna features vazias
    }

    // CORRIGIDO: Acessa os membros do FIAR_AudioFrameData diretamente do TSharedPtr
    const TArray<float>& Samples = *(AudioFrame->RawSamplesPtr);
    const int32 NumSamples = Samples.Num();
    const int32 SampleRate = AudioFrame->SampleRate;
    const int32 NumChannels = AudioFrame->NumChannels;
    const float FrameDuration = (float)NumSamples / (float)SampleRate / (float)NumChannels; // Dura��o do frame real

    // --- 1. M�tricas B�sicas de Dom�nio do Tempo ---
    float SumOfSquares = 0.0f;
    float MaxAmplitude = 0.0f;
    int32 ZeroCrossings = 0;

    // Converte para mono para c�lculo de RMS e ZCR (se for multi-canal)
    TArray<float> MonoSamples;
    if (NumChannels > 1)
    {
        MonoSamples.SetNumUninitialized(NumSamples / NumChannels);
        for (int32 i = 0; i < MonoSamples.Num(); ++i)
        {
            float Sum = 0.0f;
            for (int32 c = 0; c < NumChannels; ++c)
            {
                Sum += Samples[i * NumChannels + c];
            }
            MonoSamples[i] = Sum / NumChannels; // Simples m�dia para mono
        }
    }
    else
    {
        MonoSamples = Samples;
    }

    if (MonoSamples.Num() > 0)
    {
        for (int32 i = 0; i < MonoSamples.Num(); ++i)
        {
            float Sample = MonoSamples[i];
            SumOfSquares += Sample * Sample;
            MaxAmplitude = FMath::Max(MaxAmplitude, FMath::Abs(Sample));

            if (i > 0) // Compara com a amostra anterior para ZCR
            {
                if ((MonoSamples[i - 1] >= 0 && Sample < 0) || (MonoSamples[i - 1] < 0 && Sample >= 0))
                {
                    ZeroCrossings++;
                }
            }
        }
        Features.RMSAmplitude = FMath::Sqrt(SumOfSquares / MonoSamples.Num());
        Features.PeakAmplitude = MaxAmplitude;
        Features.ZeroCrossingRate = (float)ZeroCrossings / MonoSamples.Num(); // ZCR para amostras mono
    }

    // --- 2. Estimativa de Pitch e Detec��o Rudimentar de Notas ---
    Features.PitchEstimate = CalculateZeroCrossingRatePitchEstimate(MonoSamples, SampleRate); // Agora da classe base

    // Rudimentary Note Detection and Population of DetectedNotes
    // CORRIGIDO: Passa AudioFrame->Timestamp
    if (RudimentaryNoteDetection(Features.PitchEstimate, Features.RMSAmplitude, AudioFrame->Timestamp, FrameDuration))
    {
        // A RudimentaryNoteDetection j� adiciona a nota ao DetectedNotesHistory e atualiza LastDetectedNote.
        // Aqui, adicionamos a LastDetectedNote (que � a nota que acabou de ser "finalizada" ou "iniciada")
        // �s Features.DetectedNotes deste frame para o Blueprint.
        Features.DetectedNotes.Add(LastDetectedNote); 
    }
    
    // --- 3. C�lculo das Caracter�sticas do Attitude-Gram ---
    // Estas caracter�sticas s�o calculadas a partir do hist�rico de notas.
    // Recalculamos a cada frame para visualiza��o em tempo real.
    // Para performance em longas execu��es, um hist�rico limitado ou amostragem seria melhor.

    if (DetectedNotesHistory.Num() > 0)
    {
        TSet<int32> UniqueOctaves;
        TSet<int32> UniqueMidiNotes;
        float TotalNoteDuration = 0.0f;
        TMap<int32, int32> MidiNoteCounts;
        int32 CurrentConsecutiveRepeats = 0;
        int32 MaxConsecutiveRepeatsLocal = 0;
        int32 PreviousMidiNote = -1;

        for (const FIAR_AudioNoteFeature& Note : DetectedNotesHistory)
        {
            UniqueOctaves.Add(Note.Octave);
            UniqueMidiNotes.Add(Note.MIDINoteNumber);
            TotalNoteDuration += Note.Duration;

            MidiNoteCounts.FindOrAdd(Note.MIDINoteNumber)++;

            if (Note.MIDINoteNumber == PreviousMidiNote)
            {
                CurrentConsecutiveRepeats++;
            }
            else
            {
                CurrentConsecutiveRepeats = 1;
            }
            MaxConsecutiveRepeatsLocal = FMath::Max(MaxConsecutiveRepeatsLocal, CurrentConsecutiveRepeats);
            PreviousMidiNote = Note.MIDINoteNumber;
        }

        Features.OctavesUsed = UniqueOctaves.Num();
        Features.UniqueMidiNotesCount = UniqueMidiNotes.Num();
        Features.AverageNoteDuration = (DetectedNotesHistory.Num() > 0) ? (TotalNoteDuration / DetectedNotesHistory.Num()) : 0.0f;

        int32 MostUsedNote = 0;
        int32 MaxCount = 0;
        for (auto& Pair : MidiNoteCounts)
        {
            if (Pair.Value > MaxCount)
            {
                MaxCount = Pair.Value;
                MostUsedNote = Pair.Key;
            }
        }
        Features.MostUsedMidiNote = MostUsedNote;
        Features.MaxConsecutiveRepeats = MaxConsecutiveRepeatsLocal;

        int32 AccidentalCount = 0;
        for (int32 NoteNum : UniqueMidiNotes)
        {
            FIAR_DiatonicNoteEntry NoteEntry = UIARMIDITable::GetNoteEntryByMIDIPitch(NoteNum);
            if (NoteEntry.bIsBemol || NoteEntry.bIsSharp)
            {
                AccidentalCount++;
            }
        }
        Features.AccidentalsUsed = AccidentalCount;

        Features.AverageBPM = (Features.AverageNoteDuration > 0.0f) ? (60.0f / Features.AverageNoteDuration) : 0.0f;
        if (Features.AverageBPM > 240.0f) Features.AverageBPM = 240.0f; // Limitar a um valor razo�vel
        // Atitude do M�sico (baseado nas f�rmulas do livro 4.4 e 4.5)
        if (Features.MaxConsecutiveRepeats > 0)
        {
            Features.AttitudeScore = Features.AverageNoteDuration / Features.MaxConsecutiveRepeats;
        }
        else if (Features.UniqueMidiNotesCount > 0)
        {
            Features.AttitudeScore = Features.AverageNoteDuration / Features.UniqueMidiNotesCount;
        }
        else
        {
            Features.AttitudeScore = 0.0f;
        }
    }

    UE_LOG(LogIAR, Log, TEXT("UIARBasicAudioFeatureProcessor: Frame processado. RMS: %.4f, Pitch: %.2f Hz, Note: %s (%d), Oct: %d, AttScore: %.2f"),
           Features.RMSAmplitude, Features.PitchEstimate, Features.DetectedNotes.Num() > 0 ? *(Features.DetectedNotes.Last().NoteName) : TEXT("N/A"),
           Features.DetectedNotes.Num() > 0 ? Features.DetectedNotes.Last().MIDINoteNumber : -1,
           Features.DetectedNotes.Num() > 0 ? Features.DetectedNotes.Last().Octave : -1, Features.AttitudeScore);

    return Features;
}

bool UIARBasicAudioFeatureProcessor::RudimentaryNoteDetection(float CurrentPitchHz, float CurrentRMS, float CurrentTimestamp, float FrameDuration)
{
    bool bNewNoteDetected = false;
    
    // Converte a frequ�ncia para o n�mero de nota MIDI mais pr�ximo
    int32 CurrentMIDINote = FMath::RoundToInt(69.0f + 12.0f * (FMath::Loge(CurrentPitchHz / 440.0f) / FMath::Loge(2.0f)));
    CurrentMIDINote = FMath::Clamp(CurrentMIDINote, 0, 127); // Garante que esteja no range MIDI

    FIAR_DiatonicNoteEntry CurrentNoteEntry = UIARMIDITable::GetNoteEntryByMIDIPitch(CurrentMIDINote);

    // Se o �udio est� abaixo do limiar de sil�ncio, consideramos que n�o h� nota ativa
    if (CurrentRMS < SilenceThresholdRMS)
    {
        if (bIsNoteActive)
        {
            // A nota ativa terminou
            CurrentBuildingNote.Duration = CurrentTimestamp - CurrentBuildingNote.StartTime;
            if (CurrentBuildingNote.Duration >= MinNoteDuration)
            {
                DetectedNotesHistory.Add(CurrentBuildingNote); // Finaliza e adiciona a nota
                bNewNoteDetected = true; // Sinaliza que uma nota foi detectada/finalizada
                UE_LOG(LogIAR, Log, TEXT("Note ended: %s (%d), Dur: %.3f"), *CurrentBuildingNote.NoteName, CurrentBuildingNote.MIDINoteNumber, CurrentBuildingNote.Duration);
            }
        }
        bIsNoteActive = false;
        CurrentBuildingNote = FIAR_AudioNoteFeature(); // Resetar
        return bNewNoteDetected;
    }

    // Se uma nota j� est� ativa
    if (bIsNoteActive)
    {
        // Verificar se o pitch mudou significativamente para considerar uma nova nota
        // Usamos uma combina��o de mudan�a de frequ�ncia em Hz e mudan�a de n�mero MIDI para robustez.
        if (FMath::Abs(CurrentPitchHz - CurrentBuildingNote.PitchHz) > PitchChangeThresholdHz || CurrentMIDINote != CurrentBuildingNote.MIDINoteNumber)
        {
            // O pitch mudou, a nota anterior terminou.
            CurrentBuildingNote.Duration = CurrentTimestamp - CurrentBuildingNote.StartTime;
            if (CurrentBuildingNote.Duration >= MinNoteDuration)
            {
                DetectedNotesHistory.Add(CurrentBuildingNote); // Finaliza e adiciona a nota anterior
                bNewNoteDetected = true; // Sinaliza a nota anterior
                UE_LOG(LogIAR, Log, TEXT("Note changed (prev): %s (%d), Dur: %.3f"), *CurrentBuildingNote.NoteName, CurrentBuildingNote.MIDINoteNumber, CurrentBuildingNote.Duration);
            }

            // Iniciar a nova nota
            CurrentBuildingNote = FIAR_AudioNoteFeature();
            CurrentBuildingNote.PitchHz = CurrentPitchHz;
            CurrentBuildingNote.MIDINoteNumber = CurrentMIDINote;
            CurrentBuildingNote.NoteName = CurrentNoteEntry.NoteName; // Preenche da tabela
            CurrentBuildingNote.bIsBemol = CurrentNoteEntry.bIsBemol; // Preenche da tabela
            CurrentBuildingNote.bIsSharp = CurrentNoteEntry.bIsSharp; // Preenche da tabela
            CurrentBuildingNote.Octave = CurrentNoteEntry.Octave; // Preenche da tabela
            CurrentBuildingNote.StartTime = CurrentTimestamp;
            CurrentBuildingNote.Velocity = CurrentRMS; // Usamos RMS como uma forma de "velocidade"

            if (bHasPreviousNote)
            {
                CurrentBuildingNote.SemitonesFromPrevious = (float)CurrentBuildingNote.MIDINoteNumber - LastDetectedNote.MIDINoteNumber;
            }
            LastDetectedNote = CurrentBuildingNote; // Atualiza a �ltima nota detectada para o pr�ximo frame
            bHasPreviousNote = true;
            bNewNoteDetected = true; // Sinaliza a nova nota
            UE_LOG(LogIAR, Log, TEXT("Note started (new): %s (%d), Oct: %d"), *CurrentBuildingNote.NoteName, CurrentBuildingNote.MIDINoteNumber, CurrentBuildingNote.Octave);
        }
        // Se o pitch n�o mudou, a nota atual continua, apenas atualiza o timestamp da detec��o
        // A dura��o ser� calculada quando ela terminar.
    }
    else // Nenhuma nota ativa, e o �udio est� acima do limiar de sil�ncio
    {
        // Iniciar uma nova nota
        bIsNoteActive = true;
        CurrentBuildingNote = FIAR_AudioNoteFeature();
        CurrentBuildingNote.PitchHz = CurrentPitchHz;
        CurrentBuildingNote.MIDINoteNumber = CurrentMIDINote;
        CurrentBuildingNote.NoteName = CurrentNoteEntry.NoteName; // Preenche da tabela
        CurrentBuildingNote.bIsBemol = CurrentNoteEntry.bIsBemol; // Preenche da tabela
        CurrentBuildingNote.bIsSharp = CurrentNoteEntry.bIsSharp; // Preenche da tabela
        CurrentBuildingNote.Octave = CurrentNoteEntry.Octave; // Preenche da tabela
        CurrentBuildingNote.StartTime = CurrentTimestamp;
        CurrentBuildingNote.Velocity = CurrentRMS; // Usamos RMS como uma forma de "velocidade"

        if (bHasPreviousNote)
        {
            CurrentBuildingNote.SemitonesFromPrevious = (float)CurrentBuildingNote.MIDINoteNumber - LastDetectedNote.MIDINoteNumber;
        }
        LastDetectedNote = CurrentBuildingNote; // Atualiza a �ltima nota detectada
        bHasPreviousNote = true;
        bNewNoteDetected = true; // Sinaliza a nova nota
        UE_LOG(LogIAR, Log, TEXT("Note started (first): %s (%d), Oct: %d"), *CurrentBuildingNote.NoteName, CurrentBuildingNote.MIDINoteNumber, CurrentBuildingNote.Octave);
    }

    return bNewNoteDetected;
}
