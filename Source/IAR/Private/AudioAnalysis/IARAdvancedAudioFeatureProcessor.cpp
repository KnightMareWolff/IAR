// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "AudioAnalysis/IARAdvancedAudioFeatureProcessor.h"
#include "Core/IARMIDITable.h"
#include "../IAR.h"
#include "Math/UnrealMathUtility.h"
#include "Algo/Accumulate.h" // Para Algo::Accumulate para calcular RMS
#include "Containers/Map.h"
#include "Engine/Texture2D.h" 
#include "Rendering/Texture2DResource.h"

UIARAdvancedAudioFeatureProcessor::UIARAdvancedAudioFeatureProcessor()
    : WaveformDisplayWidth(512) 
    , WaveformDisplayHeight(128) 
    , FFTWindowSize(1024) // Ex: 1024 amostras para FFT (garantir pot�ncia de 2)
    , SpectrogramHeight(FFTWindowSize / 2 + 1) // N�mero de bins de frequ�ncia �nicos (Nyquist frequency: N/2 + 1)
    , MaxSpectrogramHistoryFrames(128) // Quantos frames de espectro manter no hist�rico para visualiza��o
    , bEnableContextualFrequencyFiltering(false) // Inicializa��o da nova flag
    , ContextualFilterSemitoneRange(12)          // Inicializa��o da nova propriedade
    , ContextualFilterAttenuationFactor(0.05f)   // Inicializa��o da nova propriedade
{
    // Garante que FFTWindowSize seja uma pot�ncia de 2 (se n�o for, arredonda para a pr�xima maior)
    // Para simplificar, vou assumir 1024 � sempre a pot�ncia de 2. Caso contr�rio, seria:
    // FFTWindowSize = FMath::Pow(2, FMath::CeilToInt(FMath::Log2(FFTWindowSize)));
    UE_LOG(LogIAR, Log, TEXT("UIARAdvancedAudioFeatureProcessor: Construtor chamado. FFTWindowSize: %d"), FFTWindowSize);
}

UIARAdvancedAudioFeatureProcessor::~UIARAdvancedAudioFeatureProcessor()
{
    UE_LOG(LogIAR, Log, TEXT("UIARAdvancedAudioFeatureProcessor: Destrutor chamado."));
}

void UIARAdvancedAudioFeatureProcessor::Initialize()
{
    Super::Initialize(); 
    SpectrogramDataHistory.Empty();
    FilteredSpectrogramDataHistory.Empty(); // NOVO: Limpa o hist�rico filtrado
    CurrentSpectrogramPixels.Empty();
    CurrentFilteredSpectrogramPixels.Empty(); // NOVO: Limpa os pixels filtrados
    CurrentWaveformPixels.Empty();
    // Re-initialize LastDetectedNote and bHasPreviousNote from base class
    LastDetectedNote = FIAR_AudioNoteFeature(); 
    bHasPreviousNote = false;
    UE_LOG(LogIAR, Log, TEXT("UIARAdvancedAudioFeatureProcessor: Inicializado com sucesso."));
}

void UIARAdvancedAudioFeatureProcessor::Shutdown()
{
    Super::Shutdown(); 
    SpectrogramDataHistory.Empty();
    FilteredSpectrogramDataHistory.Empty(); // NOVO: Limpa o hist�rico filtrado
    CurrentSpectrogramPixels.Empty();
    CurrentFilteredSpectrogramPixels.Empty(); // NOVO: Limpa os pixels filtrados
    CurrentWaveformPixels.Empty();
    // Re-initialize LastDetectedNote and bHasPreviousNote from base class
    LastDetectedNote = FIAR_AudioNoteFeature(); 
    bHasPreviousNote = false;
    UE_LOG(LogIAR, Log, TEXT("UIARAdvancedAudioFeatureProcessor: Desligado."));
}

bool UIARAdvancedAudioFeatureProcessor::CalculateFFT(const TArray<float>& Samples, int32 FrameSampleRate, TArray<float>& OutSpectrum)
{
    if (Samples.Num() == 0)
    {
        OutSpectrum.SetNum(SpectrogramHeight);
        FMemory::Memset(OutSpectrum.GetData(), 0, OutSpectrum.Num() * sizeof(float));
        return true;
    }

    // --- Padding Logic: Ensure Samples have enough data for FFTWindowSize ---
    TArray<float> ProcessedSamples = Samples;
    if (ProcessedSamples.Num() < FFTWindowSize)
    {
        ProcessedSamples.SetNumZeroed(FFTWindowSize);
        // UE_LOG(LogIAR, Log, TEXT("FFT: Samples padded from %d to %d."), Samples.Num(), FFTWindowSize);
    }
    else if (ProcessedSamples.Num() > FFTWindowSize)
    {
        // If there are too many samples, truncate to FFTWindowSize
        ProcessedSamples.SetNum(FFTWindowSize);
        // UE_LOG(LogIAR, Log, TEXT("FFT: Samples truncated from %d to %d."), Samples.Num(), FFTWindowSize);
    }

    // ATEN��O: Mude #if 0 para #if 1 AQUI para ativar o c�digo OpenCV se voc� o configurou.
    // SE VOC� AINDA N�O TEM OPENCV CONFIGURADO, MANTENHA #if 0.
    // CASO CONTR�RIO, SE TIVER LINKER ERRORS COM #if 1, VOLTE PARA #if 0.
#if 1 // Mude para 1 para usar o c�digo OpenCV real, 0 para simula��o.
    // ** C�digo OpenCV Real (comentado por padr�o) **
    // A FFT opera em n�meros complexos. Usaremos cv::dft, mas uma fun��o de windowing (Hamming, Hanning)
    // seria aplicada antes para reduzir vazamento espectral.
    // Esta � uma implementa��o SIMPLIFICADA para demonstrar a integra��o.
    
    // Cria uma matriz OpenCV a partir das amostras de �udio
    cv::Mat InputMat(1, ProcessedSamples.Num(), CV_32FC1, (void*)ProcessedSamples.GetData());
    cv::Mat OutputMat;

    // Aplica a DFT. cv::DFT_COMPLEX_OUTPUT para obter a parte real e imagin�ria.
    cv::dft(InputMat, OutputMat, cv::DFT_COMPLEX_OUTPUT);

    // Calcula a magnitude do espectro. O espectro � sim�trico, precisamos apenas da primeira metade.
    OutSpectrum.SetNum(SpectrogramHeight); // FFTWindowSize / 2 + 1
    
    for (int i = 0; i < SpectrogramHeight; ++i)
    {
        // Acessa as componentes real e imagin�ria do resultado da DFT
        // A matriz OutputMat ter� colunas 2*k e 2*k+1 para a parte real e imagin�ria do k-�simo coeficiente.
        float Real = OutputMat.at<float>(0, i * 2);
        float Imag = OutputMat.at<float>(0, i * 2 + 1);
        OutSpectrum[i] = FMath::Sqrt(Real * Real + Imag * Imag);
    }
    
    // Normaliza o espectro para valores entre 0 e 1 para visualiza��o e histograma
    float MaxVal = 0.0f;
    for (float Val : OutSpectrum) { MaxVal = FMath::Max(MaxVal, Val); }
    if (MaxVal > 0)
    {
        for (float& Val : OutSpectrum) { Val /= MaxVal; }
    }
    UE_LOG(LogIAR, Verbose, TEXT("FFT (OpenCV): Processed %d samples."), ProcessedSamples.Num());
    return true;

#else
    // ** Simula��o de FFT RESPONSIVA AO �UDIO REAL (placeholder para compilar sem OpenCV) **
    // Melhoria: Gera um espectro visualmente mais rico e distribu�do, simulando a "atividade" sonora
    // em todas as frequ�ncias, modulada pela amplitude RMS.

    OutSpectrum.SetNum(SpectrogramHeight); 

    // Calcula RMS para determinar a intensidade geral
    float SumOfSquares = 0.0f;
    for (float Sample : ProcessedSamples) { SumOfSquares += Sample * Sample; }
    float RMSAmplitude = FMath::Sqrt(SumOfSquares / ProcessedSamples.Num());

    // Mapeia RMS para uma escala visual de 0 a 1.
    // Ajuste esses valores para controlar o "brilho" geral do espectrograma
    const float MinRMSForVisual = 0.0005f; // RMS m�nimo para come�ar a mostrar algo
    const float MaxRMSForVisual = 0.05f;   // RMS m�ximo para atingir brilho total
    float VisualScale = FMath::GetMappedRangeValueClamped(FVector2f(MinRMSForVisual, MaxRMSForVisual), FVector2f(0.0f, 1.0f), RMSAmplitude);
    VisualScale = FMath::Lerp(0.1f, 1.0f, VisualScale); // Garante um brilho m�nimo mesmo com pouco sinal

    // Simula a frequ�ncia alvo (mantido para compatibilidade, mas menos relevante para o visual geral)
    float TargetFreqHz = 440.0f; 
    float MaxFrequency = (float)FrameSampleRate / 2.0f; 
    float FreqPerBin = MaxFrequency / SpectrogramHeight; 
    int32 TargetBin = FMath::RoundToInt(TargetFreqHz / FreqPerBin);
    TargetBin = FMath::Clamp(TargetBin, 0, SpectrogramHeight - 1); 

    // Preenche o espectro: uma base de ru�do mais um "pico" difuso
    for (int32 i = 0; i < SpectrogramHeight; ++i)
    {
        float Value = 0.0f;
        float BinFreq = i * FreqPerBin;

        // Adiciona um componente de ru�do base que � proporcional � VisualScale
        // Isso cria um "fundo" de atividade sonora em todas as frequ�ncias
        Value += FMath::RandRange(0.0f, 0.15f) * VisualScale; // Ru�do de fundo

        // Adiciona um pico "gaussiano" mais suave para simular a fundamental, modulado pela amplitude
        float Delta = (float)i - TargetBin;
        float Sigma = SpectrogramHeight * 0.05f; // Largura do pico (5% da altura do espectrograma)
        if (Sigma == 0.0f) Sigma = 1.0f; 
        
        Value += FMath::Exp(-(Delta * Delta) / (2.0f * Sigma * Sigma)) * VisualScale * 0.7f; // Pico suave

        OutSpectrum[i] = FMath::Clamp(Value, 0.0f, 1.0f); // Garante que o valor final est� entre 0 e 1
    }
    
    // UE_LOG(LogIAR, Log, TEXT("FFT Sim (Visual Enhanced): RMS=%.4f, VisualScale=%.4f, TargetFreq=%.2fHz (Bin %d)"), RMSAmplitude, VisualScale, TargetFreqHz, TargetBin);
    return true;
#endif // Fim da simula��o ou c�digo OpenCV real
}

TArray<FIAR_AudioNoteFeature> UIARAdvancedAudioFeatureProcessor::FindTopFrequencyNotes(const TArray<float>& Spectrum, int32 SampleRate, int32 NumPeaks)
{
    TArray<FIAR_AudioNoteFeature> DetectedNotes;
    if (Spectrum.IsEmpty() || NumPeaks <= 0 || SampleRate <= 0) return DetectedNotes;

    TMap<int32, float> MidiNoteEnergy; 

    float MaxFrequency = (float)SampleRate / 2.0f; 
    float FreqPerBin = MaxFrequency / Spectrum.Num(); 

    for (int32 i = 0; i < Spectrum.Num(); ++i)
    {
        float CurrentFreqHz = i * FreqPerBin;
        // F�rmula: MIDI = 69 + 12 * log2(Hz / 440)
        int32 MidiNote = (CurrentFreqHz > 0) ? FMath::RoundToInt(69.0f + 12.0f * (FMath::Loge(CurrentFreqHz / 440.0f) / FMath::Loge(2.0f))) : 0;
        MidiNote = FMath::Clamp(MidiNote, 0, 127); 

        MidiNoteEnergy.FindOrAdd(MidiNote) += Spectrum[i];
    }

    TArray<TPair<int32, float>> SortedMidiNotes;
    for (auto& Pair : MidiNoteEnergy)
    {
        SortedMidiNotes.Add(Pair);
    }

    SortedMidiNotes.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B){
        return A.Value > B.Value;
    });

    for (int32 i = 0; i < FMath::Min(NumPeaks, SortedMidiNotes.Num()); ++i)
    {
        const TPair<int32, float>& TopNote = SortedMidiNotes[i];
        FIAR_DiatonicNoteEntry NoteEntry = UIARMIDITable::GetNoteEntryByMIDIPitch(TopNote.Key);
        
        FIAR_AudioNoteFeature DetectedNote;
        DetectedNote.MIDINoteNumber = NoteEntry.NotePitch;
        DetectedNote.NoteName = NoteEntry.NoteName;
        DetectedNote.Octave = NoteEntry.Octave;
        DetectedNote.PitchHz = NoteEntry.Frequency; 
        DetectedNote.bIsBemol = NoteEntry.bIsBemol;
        DetectedNote.bIsSharp = NoteEntry.bIsSharp;
        DetectedNote.Velocity = FMath::Clamp(TopNote.Value, 0.0f, 1.0f); // Usa a energia acumulada como "velocidade" (normalizada)
        DetectedNote.StartTime = -1.0f; 
        DetectedNote.Duration = -1.0f;  
        DetectedNote.SemitonesFromPrevious = 0.0f; // Calculado no ProcessFrame para a principal

        DetectedNotes.Add(DetectedNote);
    }

    return DetectedNotes;
}

// Este m�todo agora recebe o hist�rico (filtrado ou n�o) como par�metro
void UIARAdvancedAudioFeatureProcessor::GenerateSpectrogramPixels(TArray<FColor>& OutPixels, int32 Width, int32 Height, const TArray<TArray<float>>& InHistory)
{
    OutPixels.SetNumZeroed(Width * Height);
    
    float MaxValInHistory = 0.0f;
    if (InHistory.Num() > 0)
    {
        for (const TArray<float>& Spectrum : InHistory)
        {
            for (float Val : Spectrum)
            {
                MaxValInHistory = FMath::Max(MaxValInHistory, Val);
            }
        }
    }
    // UE_LOG(LogIAR, Log, TEXT("Spectrogram: Max Value in History (Raw): %.6f"), MaxValInHistory);
    for (int32 y = 0; y < Height; ++y) 
    {
        for (int32 x = 0; x < Width; ++x) 
        {
            float Value = 0.0f; 
            // Garante que o acesso ao hist�rico est� dentro dos limites
            if (x < InHistory.Num() && y < InHistory[x].Num())
            {
                // Acesso aos dados do espectro, normalizado para 0-1
                Value = InHistory[x][y]; 
            }

            // --- MELHORIA DE BRILHO/CONTRASTE ---
            // Raiz quadrada para amplificar valores baixos, e um piso de brilho
            Value = FMath::Lerp(0.05f, 1.0f, FMath::Sqrt(Value)); 
            Value = FMath::Clamp(Value, 0.0f, 1.0f); 
            
            FColor PixelColor;
            float Hue = (1.0f - Value) * 240.0f; // Mapeia 0.0 (alto valor) para azul (240), 1.0 (baixo valor) para vermelho/roxo (0)
            PixelColor = FLinearColor::MakeFromHSV8(Hue, 255, Value * 255).ToFColor(true); 
            OutPixels[y * Width + x] = PixelColor;
        }
    }
}

const TArray<FColor>& UIARAdvancedAudioFeatureProcessor::GetSpectrogramPixels(int32& OutWidth, int32& OutHeight) const
{
    OutWidth = MaxSpectrogramHistoryFrames;
    OutHeight = SpectrogramHeight;
    return CurrentSpectrogramPixels;
}

// Getter para o espectrograma filtrado
const TArray<FColor>& UIARAdvancedAudioFeatureProcessor::GetFilteredSpectrogramPixels(int32& OutWidth, int32& OutHeight) const
{
    OutWidth = MaxSpectrogramHistoryFrames;
    OutHeight = SpectrogramHeight;
    return CurrentFilteredSpectrogramPixels;
}

void UIARAdvancedAudioFeatureProcessor::GenerateWaveformPixels(const TArray<float>& InMonoSamples, TArray<FColor>& OutPixels, int32 Width, int32 Height)
{
    if (InMonoSamples.Num() == 0 || Width <= 0 || Height <= 0)
    {
        OutPixels.SetNumZeroed(Width * Height); 
        return;
    }

    OutPixels.SetNumZeroed(Width * Height);

    for (int32 i = 0; i < OutPixels.Num(); ++i)
    {
        OutPixels[i] = FColor::Black;
    }

    const int32 NumSamples = InMonoSamples.Num();
    const float SamplesPerColumn = (float)NumSamples / Width;

    for (int32 X = 0; X < Width; ++X)
    {
        int32 StartSampleIndex = FMath::FloorToInt(X * SamplesPerColumn);
        int32 EndSampleIndex = FMath::Min(FMath::CeilToInt((X + 1) * SamplesPerColumn), NumSamples);

        float MaxAmplitude = 0.0f;
        float MinAmplitude = 0.0f;

        if (EndSampleIndex > StartSampleIndex)
        {
            MaxAmplitude = InMonoSamples[StartSampleIndex];
            MinAmplitude = InMonoSamples[StartSampleIndex];
            for (int32 i = StartSampleIndex + 1; i < EndSampleIndex; ++i)
            {
                MaxAmplitude = FMath::Max(MaxAmplitude, InMonoSamples[i]);
                MinAmplitude = FMath::Min(MinAmplitude, InMonoSamples[i]);
            }
        }
        else if (StartSampleIndex < NumSamples)
        {
            MaxAmplitude = InMonoSamples[StartSampleIndex];
            MinAmplitude = InMonoSamples[StartSampleIndex];
        }
        
        // Mapeia a faixa de amplitude [-1, 1] para a faixa de pixels [0, Height-1]
        int32 MinPixelY = FMath::Clamp(FMath::FloorToInt(((MinAmplitude + 1.0f) / 2.0f) * (Height - 1)), 0, Height - 1);
        int32 MaxPixelY = FMath::Clamp(FMath::FloorToInt(((MaxAmplitude + 1.0f) / 2.0f) * (Height - 1)), 0, Height - 1);

        if (MinPixelY > MaxPixelY)
        {
            Swap(MinPixelY, MaxPixelY);
        }

        for (int32 Y = MinPixelY; Y <= MaxPixelY; ++Y)
        {
            int32 PixelIndex = Y * Width + X;
            if (PixelIndex >= 0 && PixelIndex < OutPixels.Num())
            {
                OutPixels[PixelIndex] = FColor::Cyan; 
            }
        }
    }
}

// A implementa��o de GetWaveformPixels que estava faltando!
const TArray<FColor>& UIARAdvancedAudioFeatureProcessor::GetWaveformPixels(int32& OutWidth, int32& OutHeight) const
{
    OutWidth = WaveformDisplayWidth;
    OutHeight = WaveformDisplayHeight;
    return CurrentWaveformPixels;
}


FIAR_AudioFeatures UIARAdvancedAudioFeatureProcessor::ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture)
{
    FIAR_AudioFeatures Features;

    if (!AudioFrame.IsValid() || !AudioFrame->RawSamplesPtr.IsValid() || AudioFrame->RawSamplesPtr->Num() == 0)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAdvancedAudioFeatureProcessor: Frame de �udio inv�lido ou vazio."));
        return Features;
    }

    // N�O MEXA AQUI: As opera��es de Noise Gate e Low/High Pass Filter s�o aplicadas antes
    // do ProcessFrame na UIARAudioComponent para que operem no `RawSamplesPtr` original.
    const TArray<float>& Samples = *(AudioFrame->RawSamplesPtr);
    const int32 NumSamples = Samples.Num();
    const int32 SampleRate = AudioFrame->SampleRate;
    const int32 NumChannels = AudioFrame->NumChannels;
    const float FrameDuration = (float)NumSamples / (float)SampleRate / (float)NumChannels;

    // --- 1. Converter para Mono e Calcular FFT ---
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
            MonoSamples[i] = Sum / NumChannels; 
        }
    }
    else
    {
        MonoSamples = Samples;
    }

    GenerateWaveformPixels(MonoSamples, CurrentWaveformPixels, WaveformDisplayWidth, WaveformDisplayHeight);

    // Espectro ORIGINAL (n�o filtrado)
    TArray<float> CurrentSpectrum;
    if (!CalculateFFT(MonoSamples, SampleRate, CurrentSpectrum)) 
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAdvancedAudioFeatureProcessor: Falha ao calcular FFT."));
        return Features;
    }

    // Espectro FILTRADO (copia do original e aplica a filtragem)
    TArray<float> FilteredSpectrum = CurrentSpectrum;
    if (bEnableContextualFrequencyFiltering && bHasPreviousNote)
    {
        ApplyContextualFrequencyFilter(FilteredSpectrum, SampleRate);
    }

    // EXPLICITAMENTE DEFINIR PITCH ESTIMATE USANDO ZCR
    // O PitchEstimate ainda � feito no MonoSamples, antes de qualquer filtragem de espectro
    Features.PitchEstimate = CalculateZeroCrossingRatePitchEstimate(MonoSamples, SampleRate);

    // --- Hist�ricos e Gera��o de Pixels para Espectrogramas ---
    // Hist�rico de espectro ORIGINAL (para o SpectrogramTexture principal)
    SpectrogramDataHistory.Add(CurrentSpectrum);
    while (SpectrogramDataHistory.Num() > MaxSpectrogramHistoryFrames)
    {
        SpectrogramDataHistory.RemoveAt(0); 
    }
    // Gera pixels para o espectrograma ORIGINAL
    GenerateSpectrogramPixels(CurrentSpectrogramPixels, MaxSpectrogramHistoryFrames, SpectrogramHeight, SpectrogramDataHistory);

    // Hist�rico de espectro FILTRADO (para o FilteredSpectrogramTexture)
    FilteredSpectrogramDataHistory.Add(FilteredSpectrum);
    while (FilteredSpectrogramDataHistory.Num() > MaxSpectrogramHistoryFrames)
    {
        FilteredSpectrogramDataHistory.RemoveAt(0);
    }
    // Gera pixels para o espectrograma FILTRADO
    GenerateSpectrogramPixels(CurrentFilteredSpectrogramPixels, MaxSpectrogramHistoryFrames, SpectrogramHeight, FilteredSpectrogramDataHistory);

    // OutSpectrogramTexture n�o � definido aqui, � no AudioComponent.

    // --- 2. An�lise de Frequ�ncia e Detec��o de Notas MIDI (Top 3) ---
    // A detec��o de notas agora usa o espectro FILTRADO, se a filtragem estiver habilitada.
    TArray<FIAR_AudioNoteFeature> TopNotes = FindTopFrequencyNotes(bEnableContextualFrequencyFiltering ? FilteredSpectrum : CurrentSpectrum, SampleRate, 3); 
    Features.DetectedNotes = TopNotes; 

    // Pega a nota principal (a mais forte) para o SemitonesFromPrevious (se houver)
    if (!Features.DetectedNotes.IsEmpty())
    {
        FIAR_AudioNoteFeature PrimaryNote = Features.DetectedNotes[0];
        
        // Atualiza LastDetectedNote para c�lculo do Contorno Mel�dico no pr�ximo frame
        if (bHasPreviousNote)
        {
            PrimaryNote.SemitonesFromPrevious = (float)PrimaryNote.MIDINoteNumber - LastDetectedNote.MIDINoteNumber;
            // Atualizar a semitonsFromPrevious da nota principal j� adicionada em Features.DetectedNotes
            if (!Features.DetectedNotes.IsEmpty()) {
                Features.DetectedNotes[0].SemitonesFromPrevious = PrimaryNote.SemitonesFromPrevious;
            }
        }
        LastDetectedNote = PrimaryNote;
        bHasPreviousNote = true;
    }
    else
    {
        bHasPreviousNote = false;
    }

    // --- 3. Calcular M�tricas de Dom�nio do Tempo (RMS, Peak, ZCR) ---
    float SumOfSquares = 0.0f;
    float MaxAmplitude = 0.0f;
    int32 ZeroCrossings = 0;

    if (MonoSamples.Num() > 0)
    {
        for (int32 i = 0; i < MonoSamples.Num(); ++i)
        {
            float Sample = MonoSamples[i];
            SumOfSquares += Sample * Sample;
            MaxAmplitude = FMath::Max(MaxAmplitude, FMath::Abs(Sample));
            if (i > 0)
            {
                if ((MonoSamples[i - 1] >= 0 && Sample < 0) || (MonoSamples[i - 1] < 0 && Sample >= 0))
                {
                    ZeroCrossings++;
                }
            }
        }
        Features.RMSAmplitude = FMath::Sqrt(SumOfSquares / MonoSamples.Num());
        Features.PeakAmplitude = MaxAmplitude;
        Features.ZeroCrossingRate = (float)ZeroCrossings / MonoSamples.Num();
    }

    // --- 4. C�lculo das Caracteristicas do Attitude-Gram ---
    // Apenas adiciona a nota principal ao hist�rico, a l�gica real de notas ativas para MIDI foi para o transcritor
    if (!Features.DetectedNotes.IsEmpty()) 
    {
        FIAR_AudioNoteFeature MainNoteForHistory = Features.DetectedNotes[0]; 
        MainNoteForHistory.StartTime = AudioFrame->Timestamp; 
        MainNoteForHistory.Duration = FrameDuration; 
        DetectedNotesHistory.Add(MainNoteForHistory);
    }
    
    while (DetectedNotesHistory.Num() > 2000) 
    {
        DetectedNotesHistory.RemoveAt(0);
    }

    if (DetectedNotesHistory.Num() > 0)
    {
        TSet<int32> UniqueOctaves;
        TSet<int32> UniqueMidiNotes;
        float TotalNoteDuration = 0.0f;
        TMap<int32, int32> MidiNoteCounts;
        int32 CurrentConsecutiveRepeats = 0;
        int32 MaxConsecutiveRepeatsLocal = 0;
        int32 PreviousMidiNoteNum = -1;

        for (const FIAR_AudioNoteFeature& Note : DetectedNotesHistory)
        {
            UniqueOctaves.Add(Note.Octave);
            UniqueMidiNotes.Add(Note.MIDINoteNumber);
            TotalNoteDuration += Note.Duration;

            MidiNoteCounts.FindOrAdd(Note.MIDINoteNumber)++;

            if (Note.MIDINoteNumber == PreviousMidiNoteNum)
            {
                CurrentConsecutiveRepeats++;
            }
            else
            {
                CurrentConsecutiveRepeats = 1;
            }
            MaxConsecutiveRepeatsLocal = FMath::Max(MaxConsecutiveRepeatsLocal, CurrentConsecutiveRepeats);
            PreviousMidiNoteNum = Note.MIDINoteNumber;
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
        if (Features.AverageBPM > 240.0f) Features.AverageBPM = 240.0f; 

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

    // UE_LOG(LogIAR, Log, TEXT("UIARAdvancedAudioFeatureProcessor: Frame processado. RMS: %.4f, Pitch: %.2f Hz, Top Notes: %d, AttScore: %.2f"),
    //        Features.RMSAmplitude, Features.PitchEstimate, Features.DetectedNotes.Num(), Features.AttitudeScore);
    
    return Features;
}

void UIARAdvancedAudioFeatureProcessor::ApplyContextualFrequencyFilter(TArray<float>& Spectrum, int32 FrameSampleRate)
{
    if (Spectrum.Num() == 0 || FrameSampleRate <= 0)
    {
        return; // Nada para filtrar
    }

    // Se n�o h� uma nota anterior detectada, n�o podemos aplicar a filtragem contextual.
    // O algoritmo de detec��o de notas (FFT, etc.) precisaria de um "primeiro hit" sem contexto.
    if (!bHasPreviousNote)
    {
        // UE_LOG(LogIAR, Verbose, TEXT("Contextual Filtering: No previous note, skipping filter application."));
        return;
    }

    // Determine a faixa de frequ�ncia alvo baseada na �ltima nota detectada.
    int32 BaseMIDINote = LastDetectedNote.MIDINoteNumber;

    // Calcula as notas MIDI inferior e superior para a faixa de filtragem.
    // Clamp para garantir que os valores estejam dentro do range MIDI (0-127).
    int32 LowerMIDINote = FMath::Clamp(BaseMIDINote - ContextualFilterSemitoneRange, 0, 127);
    int32 UpperMIDINote = FMath::Clamp(BaseMIDINote + ContextualFilterSemitoneRange, 0, 127);

    // Obt�m as frequ�ncias correspondentes a essas notas MIDI da tabela.
    float MinTargetFreq = UIARMIDITable::GetNoteEntryByMIDIPitch(LowerMIDINote).Frequency;
    float MaxTargetFreq = UIARMIDITable::GetNoteEntryByMIDIPitch(UpperMIDINote).Frequency;

    // Verifica��es de seguran�a para garantir frequ�ncias v�lidas.
    // Nota 0 tem frequ�ncia 0.0f. Se o LowerMIDINote for 0, MinTargetFreq ser� 0, o que � aceit�vel.
    if (MinTargetFreq == 0.0f && LowerMIDINote != 0) 
    {
         UE_LOG(LogIAR, Warning, TEXT("Contextual Filtering: Invalid MinTargetFreq derived. Skipping filter."));
         return;
    }
    // Similarmente para MaxTargetFreq.
    if (MaxTargetFreq == 0.0f && UpperMIDINote != 0) 
    {
         UE_LOG(LogIAR, Warning, TEXT("Contextual Filtering: Invalid MaxTargetFreq derived. Skipping filter."));
         return;
    }
    if (MinTargetFreq > MaxTargetFreq) // Garante que o min � sempre menor que o max.
    {
        Swap(MinTargetFreq, MaxTargetFreq);
    }
    
    // Calcula a resolu��o de frequ�ncia por bin do espectro.
    // O �ltimo bin do espectro (Spectrum.Num() - 1) corresponde � frequ�ncia de Nyquist (FrameSampleRate / 2).
    float MaxFrequency = (float)FrameSampleRate / 2.0f; // Frequ�ncia de Nyquist
    float FreqPerBin = 0.0f;
    if (Spectrum.Num() > 1) // Para evitar divis�o por zero se o espectro for muito pequeno.
    {
        FreqPerBin = MaxFrequency / (Spectrum.Num() - 1);
    }
    else
    {
        return; // Espectro inv�lido para processar.
    }

    UE_LOG(LogIAR, Log, TEXT("Contextual Filtering: Applying filter around %s (MIDI %d, %.2fHz). Target Freq Range: %.2fHz to %.2fHz (MIDI %d-%d). Attenuation: %.2f"),
           *LastDetectedNote.NoteName, LastDetectedNote.MIDINoteNumber, LastDetectedNote.PitchHz, MinTargetFreq, MaxTargetFreq, LowerMIDINote, UpperMIDINote, ContextualFilterAttenuationFactor);

    // Itera por todos os bins do espectro e atenua aqueles fora da faixa alvo.
    for (int32 i = 0; i < Spectrum.Num(); ++i)
    {
        float CurrentBinFreq = i * FreqPerBin;
        // Se a frequ�ncia do bin est� fora da faixa desejada, atenua sua energia.
        if (CurrentBinFreq < MinTargetFreq || CurrentBinFreq > MaxTargetFreq)
        {
            Spectrum[i] *= ContextualFilterAttenuationFactor;
        }
    }
}
