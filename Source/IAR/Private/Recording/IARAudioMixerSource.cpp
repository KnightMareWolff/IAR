// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Recording/IARAudioMixerSource.h"
#include "../IAR.h"
#include "AudioCapture.h"       
#include "AudioCaptureDeviceInterface.h" 
#include "AudioCaptureCore.h"   

UIARAudioMixerSource::UIARAudioMixerSource()
    : UIARAudioSource() 
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Construtor chamado."));
}

UIARAudioMixerSource::~UIARAudioMixerSource()
{
    Shutdown(); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Destrutor chamado."));
}

void UIARAudioMixerSource::Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool)
{
    Super::Initialize(StreamSettings, InFramePool); 

    if (!FramePool || !IsValid(FramePool))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: FramePool inválido. Não é possível inicializar a fonte."));
        return;
    }

    if (!AudioCapture.IsValid())
    {
        AudioCapture = MakeUnique<FAudioCapture>();
    }

    if (AudioCapture.IsValid())
    {
        FAudioCaptureDeviceParams Params;
        Params.DeviceIndex = StreamSettings.InputDeviceIndex;
        Params.NumInputChannels = StreamSettings.NumChannels; 
        Params.SampleRate = StreamSettings.SampleRate; 

        FOnAudioCaptureFunction OnAudioCaptureCallback = 
            [this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
        {
            this->OnAudioCapture(InAudio, NumFrames, NumChannels, SampleRate, StreamTime, bOverFlow);
        };

        uint32 DesiredFramesPerBuffer = 1024; 
        if (AudioCapture->OpenAudioCaptureStream(Params, OnAudioCaptureCallback, DesiredFramesPerBuffer))
        {
            UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Dispositivo de audio aberto com sucesso. Solicitado SR: %d, Cap. SR: %d, Solicitado Ch: %d, Cap. Ch: %d."),
                StreamSettings.SampleRate, Params.SampleRate, StreamSettings.NumChannels, Params.NumInputChannels);
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha ao abrir dispositivo de audio com as configurações fornecidas (Index: %d, SR: %d, Ch: %d). Tentando dispositivo padrão."),
                Params.DeviceIndex, Params.SampleRate, Params.NumInputChannels);
            
            FAudioCaptureDeviceParams DefaultParams;
            DefaultParams.NumInputChannels = StreamSettings.NumChannels; 
            DefaultParams.SampleRate = StreamSettings.SampleRate; 

            if (AudioCapture->OpenAudioCaptureStream(DefaultParams, OnAudioCaptureCallback, DesiredFramesPerBuffer)) 
            {
                UE_LOG(LogIAR, Warning, TEXT("UIARAudioMixerSource: Dispositivo de audio padrão aberto com sucesso. Solicitado SR: %d, Cap. SR: %d, Solicitado Ch: %d, Cap. Ch: %d."),
                    StreamSettings.SampleRate, DefaultParams.SampleRate, StreamSettings.NumChannels, DefaultParams.NumInputChannels);
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha grave ao abrir QUALQUER dispositivo de audio. Verifique as permissões ou dispositivos de entrada."));
                AudioCapture = nullptr; 
                return;
            }
        }
    }
    else
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha ao criar instância de FAudioCapture."));
    }
}

void UIARAudioMixerSource::StartCapture()
{
    if (bIsCapturing) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioMixerSource: Captura já está ativa."));
        return;
    }

    if (AudioCapture.IsValid() && AudioCapture->StartStream())
    {
        Super::StartCapture(); 
        FCaptureDeviceInfo pInfo;
        AudioCapture->GetCaptureDeviceInfo(pInfo);
        UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Captura de audio do Audio Mixer iniciada (SR Cap: %d, Ch Cap: %d)."), AudioCapture->GetSampleRate(), pInfo.InputChannels);
    }
    else
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha ao iniciar a captura do Audio Mixer."));
    }
}

void UIARAudioMixerSource::StopCapture()
{
    if (!bIsCapturing) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioMixerSource: Captura já está inativa."));
        return;
    }

    if (AudioCapture.IsValid() && AudioCapture->StopStream())
    {
        Super::StopCapture(); 
        UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Captura de audio do Audio Mixer parada."));
    }
    else
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha ao parar a captura do Audio Mixer."));
    }
}

void UIARAudioMixerSource::Shutdown()
{
    Super::Shutdown(); 
    if (AudioCapture.IsValid())
    {
        AudioCapture->CloseStream();
        AudioCapture = nullptr; 
        UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Stream de captura de audio do Audio Mixer fechado."));
    }
    UE_LOG(LogIAR, Log, TEXT("UIARAudioMixerSource: Desligado e recursos liberados."));
}

void UIARAudioMixerSource::OnAudioCapture(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
{
    if (!FramePool || !IsValid(FramePool))
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: FramePool inválido durante OnAudioCapture. Não é possível processar amostras."));
        StopCapture(); 
        return;
    }

    TSharedPtr<FIAR_AudioFrameData> AudioFrame = FramePool->AcquireFrame();
    if (!AudioFrame.IsValid())
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioMixerSource: Falha ao adquirir frame do pool em OnAudioCapture. Não é possível processar amostras."));
        return;
    }

    const float* AudioDataFloat = static_cast<const float*>(InAudio);
    
    AudioFrame->RawSamplesPtr->SetNumUninitialized(NumFrames * NumChannels);
    FMemory::Memcpy(AudioFrame->RawSamplesPtr->GetData(), AudioDataFloat, NumFrames * NumChannels * sizeof(float));

    AudioFrame->SampleRate = SampleRate; 
    AudioFrame->NumChannels = NumChannels; 
    AudioFrame->Timestamp = (float)StreamTime; 

    if (bOverFlow)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioMixerSource: Audio capture overflow detected. Some audio data may have been lost."));
    }

    OnAudioFrameAcquired.Broadcast(AudioFrame); 
}
