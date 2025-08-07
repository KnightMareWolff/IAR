// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioSimulatedSource.h"
#include "../IAR.h"
#include "Kismet/GameplayStatics.h" 
#include "Engine/World.h"

UIARAudioSimulatedSource::UIARAudioSimulatedSource()
    : UIARAudioSource() 
    , CurrentTimeAccumulator(0.0f)
    , SineWaveFrequencyHz(440.0f) 
    , SamplesPerFrame(0)
    , FrameDurationSeconds(0.0f)
    , AmplitudeModulationTime(0.0f) 
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Construtor chamado."));
}

UIARAudioSimulatedSource::~UIARAudioSimulatedSource()
{
    Shutdown(); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Destrutor chamado."));
}

void UIARAudioSimulatedSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    Super::Initialize(StreamSettings, InFramePool); 

    if (CurrentStreamSettings.SampleRate <= 0 || CurrentStreamSettings.NumChannels <= 0)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: SampleRate ou NumChannels inválidos. SampleRate= %d , Channels= %d ."),
            CurrentStreamSettings.SampleRate, CurrentStreamSettings.NumChannels);
        return;
    }

    SamplesPerFrame = 4096; 
    FrameDurationSeconds = (float)SamplesPerFrame / CurrentStreamSettings.SampleRate;

    if (!FramePool)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: FramePool é nulo após inicialização. "));
        return;
    }

    UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Inicializado com %d amostras por frame (%.4f segundos/frame)."), SamplesPerFrame, FrameDurationSeconds);
}

void UIARAudioSimulatedSource::StartCapture()
{
    if (!bIsCapturing) 
    {
        if (UWorld* World = GetWorld())
        {
            if (FramePool && FramePool->IsValidLowLevelFast()) 
            {
                World->GetTimerManager().SetTimer(
                    AudioGenerationTimerHandle,
                    this,
                    &UIARAudioSimulatedSource::FillSimulatedFrame,
                    FrameDurationSeconds, 
                    true 
                );
                Super::StartCapture(); 
                UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Captura de áudio simulada iniciada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: Não foi possível iniciar a captura. FramePool é inválido ou não inicializado."));
            }
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: Não foi possível obter o World para iniciar o timer."));
        }
    }
}

void UIARAudioSimulatedSource::StopCapture()
{
    if (bIsCapturing) 
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(AudioGenerationTimerHandle);
            UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Timer de geração de áudio limpo."));
        }
        else
        {
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioSimulatedSource: World é nulo ao tentar parar a captura. O timer será interrompido naturalmente."));
        }
        Super::StopCapture(); 
        UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Captura de áudio simulada parada."));
    }
}

void UIARAudioSimulatedSource::Shutdown()
{
    StopCapture(); 
    Super::Shutdown(); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioSimulatedSource: Desligado e recursos liberados."));
}

void UIARAudioSimulatedSource::FillSimulatedFrame() 
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioSimulatedSource: FillSimulatedFrame chamada com World inválido. Interrompendo a geração de frames."));
        if (AudioGenerationTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(AudioGenerationTimerHandle);
        }
        Super::StopCapture(); 
        return; 
    }

    if (!FramePool || !FramePool->IsValidLowLevelFast()) 
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: Falha ao adquirir frame. FramePool é inválido ou nulo."));
        return;
    }

    TSharedPtr<FIAR_AudioFrameData> AudioFrame = FramePool->AcquireFrame();
    if (!AudioFrame.IsValid())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioSimulatedSource: Falha ao adquirir frame do pool. Não é possível gerar amostras."));
        return;
    }

    TArray<float>& RawSamples = *(AudioFrame->RawSamplesPtr);
    
    const int32 TotalSamples = SamplesPerFrame * CurrentStreamSettings.NumChannels;
    if (RawSamples.Num() != TotalSamples)
    {
        RawSamples.SetNumZeroed(TotalSamples);
    }

    float CurrentAmplitude = 0.0f;
    CurrentAmplitude = (FMath::Sin(AmplitudeModulationTime * PI * 0.4f) * 0.5f + 0.5f) * 0.5f; 
    CurrentAmplitude = FMath::Max(0.0f, CurrentAmplitude - 0.1f); 
    CurrentAmplitude *= 2.0f; 
    CurrentAmplitude = FMath::Min(CurrentAmplitude, 1.0f);


    for (int32 i = 0; i < SamplesPerFrame; ++i)
    {
        float SampleValue = FMath::Sin(2.0f * PI * SineWaveFrequencyHz * CurrentTimeAccumulator) * CurrentAmplitude;
        RawSamples[i * CurrentStreamSettings.NumChannels] = SampleValue; 

        for (int32 Channel = 1; Channel < CurrentStreamSettings.NumChannels; ++Channel)
        {
            RawSamples[(i * CurrentStreamSettings.NumChannels) + Channel] = RawSamples[i * CurrentStreamSettings.NumChannels];
        }

        CurrentTimeAccumulator += 1.0f / CurrentStreamSettings.SampleRate;
    }
    AmplitudeModulationTime += FrameDurationSeconds; 

    AudioFrame->SampleRate = CurrentStreamSettings.SampleRate;
    AudioFrame->NumChannels = CurrentStreamSettings.NumChannels;
    AudioFrame->Timestamp = UGameplayStatics::GetTimeSeconds(this); 

    OnAudioFrameAcquired.Broadcast(AudioFrame); // BROADCAST DO DELEGATE DA CLASSE BASE
}
