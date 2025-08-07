// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "AudioAnalysis/IARFeatureProcessor.h"
#include "../IAR.h" // Para LogIAR
#include "Math/UnrealMathUtility.h" // Para FMath

// Implementação base para a classe abstrata
UIARFeatureProcessor::UIARFeatureProcessor()
    : bHasPreviousNote(false)
{
    UE_LOG(LogIAR, Log, TEXT("UIARFeatureProcessor: Construtor chamado."));
}

UIARFeatureProcessor::~UIARFeatureProcessor()
{
    UE_LOG(LogIAR, Log, TEXT("UIARFeatureProcessor: Destrutor chamado."));
}

void UIARFeatureProcessor::Initialize()
{
    UE_LOG(LogIAR, Log, TEXT("UIARFeatureProcessor: Inicializado com sucesso."));
    LastDetectedNote = FIAR_AudioNoteFeature(); // Resetar para garantir estado limpo
    bHasPreviousNote = false;
    Z_LowPass.Empty(); // Limpa estados dos filtros
    Z_HighPass.Empty(); // Limpa estados dos filtros
}

void UIARFeatureProcessor::Shutdown()
{
    UE_LOG(LogIAR, Log, TEXT("UIARFeatureProcessor: Desligado."));
    LastDetectedNote = FIAR_AudioNoteFeature(); // Limpar estado
    bHasPreviousNote = false;
    Z_LowPass.Empty(); // Limpa estados dos filtros
    Z_HighPass.Empty(); // Limpa estados dos filtros
}

// Implementação padrão da função ProcessFrame para a classe abstrata
FIAR_AudioFeatures UIARFeatureProcessor::ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture)
{
    // Esta é a implementação da classe base abstrata.
    // As subclasses devem implementar a lógica de processamento real.
    UE_LOG(LogIAR, Warning, TEXT("UIARFeatureProcessor::ProcessFrame: Chamada da implementação base. Nenhuma feature extraída."));
    return FIAR_AudioFeatures();
}

// Implementação do CalculateZeroCrossingRatePitchEstimate (movida para a base)
float UIARFeatureProcessor::CalculateZeroCrossingRatePitchEstimate(const TArray<float>& Samples, int32 SampleRate) const
{
    if (Samples.Num() < 2 || SampleRate <= 0)
    {
        return 0.0f;
    }

    int32 ZeroCrossings = 0;
    for (int32 i = 1; i < Samples.Num(); ++i)
    {
        if ((Samples[i - 1] >= 0 && Samples[i] < 0) || (Samples[i - 1] < 0 && Samples[i] >= 0))
        {
            ZeroCrossings++;
        }
    }

    float FrameDuration = (float)Samples.Num() / (float)SampleRate;
    if (FrameDuration > 0.0f)
    {
        float ZCR_per_second = (float)ZeroCrossings / FrameDuration;
        return ZCR_per_second / 2.0f; // Cada ciclo tem duas passagens por zero
    }
    return 0.0f;
}

void UIARFeatureProcessor::ApplyNoiseGate(TArray<float>& Samples, float ThresholdRMS, float AttackTimeMs, float ReleaseTimeMs, int32 SampleRate)
{
    if (Samples.Num() == 0 || SampleRate <= 0) return;

    // Calcula o RMS do frame atual
    float SumOfSquares = 0.0f;
    for (float Sample : Samples) { SumOfSquares += Sample * Sample; }
    float CurrentRMS = FMath::Sqrt(SumOfSquares / Samples.Num());

    // Fatores de ganho para ataque e release (simplificado para um gate instantâneo ou com transição linear)
    float TargetGain = (CurrentRMS < ThresholdRMS) ? 0.0f : 1.0f; // Silenciar se abaixo do limiar

    // Aqui uma implementação mais sofisticada usaria um estado de "ganho atual" e aplicaria attack/release.
    // Por simplicidade, para o primeiro teste, aplicaremos o ganho diretamente.
    // Em uma versão real, você teria um ganho `float CurrentGain` que seria `FMath::FInterpTo` TargetGain.

    for (float& Sample : Samples)
    {
        Sample *= TargetGain;
    }
    
    // Log para depuração (pode ser Verbose para evitar poluir o log)
    UE_LOG(LogIAR, Log, TEXT("Noise Gate: RMS=%.4f, Threshold=%.4f, TargetGain=%.2f"), CurrentRMS, ThresholdRMS, TargetGain);
}

void UIARFeatureProcessor::ApplyLowPassFilter(TArray<float>& Samples, float CutoffFrequencyHz, int32 SampleRate, int32 NumChannels)
{
    if (Samples.Num() == 0 || SampleRate <= 0 || CutoffFrequencyHz <= 0 || NumChannels <= 0) return;

    // Inicializa o estado do filtro (Z-1) se ainda não o fez ou se o número de canais mudou
    if (Z_LowPass.Num() != NumChannels)
    {
        Z_LowPass.Init(0.0f, NumChannels);
    }

    // Calcula o coeficiente do filtro de primeira ordem (RC filter)
    // alpha = 2 * PI * cutoff_freq / sample_rate
    // a = alpha / (alpha + 1)  (para filtro passa-baixa)
    float Alpha = FMath::Tan(PI * CutoffFrequencyHz / SampleRate); // Coeficiente para filtro bilinear transform (approx)
    float A = Alpha / (1.0f + Alpha);

    for (int32 i = 0; i < Samples.Num(); ++i)
    {
        int32 ChannelIdx = i % NumChannels;
        // y[n] = a * x[n] + (1 - a) * y[n-1]
        Samples[i] = A * Samples[i] + (1.0f - A) * Z_LowPass[ChannelIdx];
        Z_LowPass[ChannelIdx] = Samples[i]; // Atualiza o estado para a próxima amostra
    }
    UE_LOG(LogIAR, Log, TEXT("Low Pass Filter: Cutoff=%.2fHz, Samples=%d, Channels=%d, Alpha=%.4f"), CutoffFrequencyHz, Samples.Num(), NumChannels, Alpha);
}

void UIARFeatureProcessor::ApplyHighPassFilter(TArray<float>& Samples, float CutoffFrequencyHz, int32 SampleRate, int32 NumChannels)
{
    if (Samples.Num() == 0 || SampleRate <= 0 || CutoffFrequencyHz <= 0 || NumChannels <= 0) return;

    // Inicializa o estado do filtro (Z-1) se ainda não o fez ou se o número de canais mudou
    if (Z_HighPass.Num() != NumChannels)
    {
        Z_HighPass.Init(0.0f, NumChannels);
    }

    // Calcula o coeficiente do filtro de primeira ordem (RC filter)
    // alpha = 2 * PI * cutoff_freq / sample_rate
    // a = 1 / (alpha + 1) (para filtro passa-alta, e y[n] = a * (x[n] - x[n-1]) + (1-a) * y[n-1])
    float Alpha = FMath::Tan(PI * CutoffFrequencyHz / SampleRate); // Coeficiente para filtro bilinear transform (approx)
    float A = 1.0f / (1.0f + Alpha);

    for (int32 i = 0; i < Samples.Num(); ++i)
    {
        int32 ChannelIdx = i % NumChannels;
        float InputSample = Samples[i]; // x[n]
        float PrevInputSample = (i >= NumChannels) ? Samples[i - NumChannels] : InputSample; // x[n-1]
        
        Samples[i] = A * (InputSample - PrevInputSample) + (1.0f - A) * Z_HighPass[ChannelIdx];
        Z_HighPass[ChannelIdx] = Samples[i]; // Atualiza o estado para a próxima amostra
    }
    UE_LOG(LogIAR, Log, TEXT("High Pass Filter: Cutoff=%.2fHz, Samples=%d, Channels=%d, Alpha=%.4f"), CutoffFrequencyHz, Samples.Num(), NumChannels, Alpha);
}
