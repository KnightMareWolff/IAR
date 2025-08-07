// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "AudioAnalysis/IARMIDIToAudioSynthesizer.h"
#include "../IAR.h"
#include "Kismet/GameplayStatics.h" // Para GetTimeSeconds
#include "Engine/World.h"

UIARMIDIToAudioSynthesizer::UIARMIDIToAudioSynthesizer()
    : SynthesizedSoundWave(nullptr)
    , AudioPlayerComponent(nullptr) // <--- INICIALIZE AQUI!
{
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Construtor chamado."));
}

UIARMIDIToAudioSynthesizer::~UIARMIDIToAudioSynthesizer()
{
    Shutdown(); // Garante a limpeza
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Destrutor chamado."));
}

void UIARMIDIToAudioSynthesizer::Initialize(int32 InSampleRate, int32 InNumChannels)
{
    SampleRate = InSampleRate;
    NumChannels = InNumChannels;

    // Inicializa o pool de vozes
    VoicePool.SetNum(MaxPolyphony);
    for(int32 i = 0; i < MaxPolyphony; ++i)
    {
        VoicePool[i] = FSynthesizedVoice(); // Garante que estejam limpas
    }
    ActiveVoices.Empty(); // Limpa o mapa de ponteiros
    CurrentSynthesizerTime = 0.0f;

    // Cria e configura o USoundWaveProcedural
    if (!SynthesizedSoundWave)
    {
        SynthesizedSoundWave = NewObject<USoundWaveProcedural>(this);
        SynthesizedSoundWave->SetSampleRate(SampleRate);
        SynthesizedSoundWave->NumChannels = NumChannels;
        SynthesizedSoundWave->SoundGroup = ESoundGroup::SOUNDGROUP_Default;
        SynthesizedSoundWave->bProcedural = true; // Marca como procedural
        SynthesizedSoundWave->bLooping = false;
        
        // Protege o SynthesizedSoundWave do Garbage Collector
        SynthesizedSoundWave->AddToRoot(); 
    }

    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Inicializado com SampleRate: %d, Canais: %d, Polifonia: %d."), SampleRate, NumChannels, MaxPolyphony);
}

void UIARMIDIToAudioSynthesizer::Shutdown()
{
    StopPlayback(); // Para o timer e a reprodução
    
    // Limpa as vozes e o SoundWave
    ActiveVoices.Empty(); // Limpa apenas os ponteiros do mapa
    VoicePool.Empty();    // Destrói os objetos reais FSynthesizedVoice
    
    if (SynthesizedSoundWave)
    {
        // Remove a proteção do Garbage Collector antes de nulificar
        SynthesizedSoundWave->RemoveFromRoot(); 
        SynthesizedSoundWave = nullptr;
    }

    // <--- NOVO: Limpa o AudioPlayerComponent
    if (AudioPlayerComponent)
    {
        AudioPlayerComponent->Stop(); // Garante que o componente de áudio pare de tocar
        AudioPlayerComponent->DestroyComponent(); // Destrói o componente (se não foi destruído com o ator)
        AudioPlayerComponent = nullptr;
    }
    // NOVO FIM --->

    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Desligado."));
}

void UIARMIDIToAudioSynthesizer::ProcessMIDIEvent(const FIAR_MIDIEvent& MIDIEvent)
{
    float CurrentTime = 0.0f;
    // Tenta obter o tempo do mundo, mas se GetWorld() for nulo (durante o shutdown), usa 0.0f
    if (UWorld* World = GetWorld())
    {
        CurrentTime = World->GetTimeSeconds();
    }


    // Note On (Status 0x90, Data2 > 0)
    if (MIDIEvent.Status == 0x90 && MIDIEvent.Data2 > 0)
    {
        FSynthesizedVoice* FoundVoice = nullptr;
        
        // 1. Verifica se esta nota MIDI já possui uma voz ativa (para re-trigger ou pitch bend em sintetizadores mais avançados)
        if (ActiveVoices.Contains(MIDIEvent.Data1))
        {
            FoundVoice = ActiveVoices[MIDIEvent.Data1]; // Obtém o ponteiro para a voz existente
            UE_LOG(LogIAR, Log, TEXT("Synthesizer: Re-triggering existing voice for MIDI %d"), MIDIEvent.Data1);
        }
        else // 2. Se a nota não está ativa, tenta encontrar uma voz INATIVA no pool
        {
            for (FSynthesizedVoice& PoolVoice : VoicePool)
            {
                if (PoolVoice.EnvelopeState == FSynthesizedVoice::Off)
                {
                    FoundVoice = &PoolVoice; // Obtém um ponteiro para a voz disponível no pool
                    break;
                }
            }
        }

        if (FoundVoice)
        {
            // Armazena o ponteiro para a voz no mapa de vozes ativas
            ActiveVoices.Add(MIDIEvent.Data1, FoundVoice);
            FoundVoice->NoteOn(MIDIEvent.Data1, MIDIEvent.Data2 / 127.0f, SampleRate, CurrentTime);
            UE_LOG(LogIAR, Log, TEXT("Synthesizer: Note ON MIDI %d (Freq %.2f Hz) Vel %.2f"), MIDIEvent.Data1, FoundVoice->FrequencyHz, FoundVoice->Velocity);
        }
        else
        {
            UE_LOG(LogIAR, Warning, TEXT("Synthesizer: Max polyphony reached. Dropping MIDI Note On %d."), MIDIEvent.Data1);
        }
    }
    // Note Off (Status 0x80 ou Status 0x90 com Data2 == 0)
    else if (MIDIEvent.Status == 0x80 || (MIDIEvent.Status == 0x90 && MIDIEvent.Data2 == 0))
    {
        // Encontra o ponteiro para a voz no mapa ActiveVoices
        if (FSynthesizedVoice** VoicePtr = ActiveVoices.Find(MIDIEvent.Data1))
        {
            (*VoicePtr)->NoteOff(CurrentTime); // Chama NoteOff no objeto real via ponteiro
            UE_LOG(LogIAR, Log, TEXT("Synthesizer: Note OFF MIDI %d"), MIDIEvent.Data1);
            // A voz será removida de ActiveVoices na próxima chamada de GenerateAudioBuffer
            // quando seu envelope chegar a 'Off'.
        }
    }
}

void UIARMIDIToAudioSynthesizer::StartPlayback()
{
    if (!SynthesizedSoundWave || !GetWorld()) 
    {
        UE_LOG(LogIAR, Error, TEXT("Synthesizer: SoundWave inválido ou World não disponível. Não é possível iniciar a reprodução."));
        return;
    }

    // <--- NOVO: Cria e inicia o AudioPlayerComponent
    if (!AudioPlayerComponent)
    {
        AudioPlayerComponent = NewObject<UAudioComponent>(this);
        if (AudioPlayerComponent)
        {
            AudioPlayerComponent->SetSound(SynthesizedSoundWave);
            AudioPlayerComponent->bAutoActivate = false; // Não ativar automaticamente
            AudioPlayerComponent->RegisterComponent(); // Registra o componente
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("Synthesizer: Falha ao criar AudioPlayerComponent."));
            return;
        }
    }

    AudioPlayerComponent->Play(); // Inicia a reprodução do AudioPlayerComponent
    // NOVO FIM --->

    // Remove a chamada de UGameplayStatics::PlaySound2D, pois agora estamos usando AudioPlayerComponent
    // SynthesizedSoundWave->bLooping = false; 
    // UGameplayStatics::PlaySound2D(GetWorld(), SynthesizedSoundWave);

    // Inicia o timer para gerar buffers de áudio periodicamente
    GetWorld()->GetTimerManager().SetTimer(
        AudioGenerationTimerHandle,
        this,
        &UIARMIDIToAudioSynthesizer::GenerateAudioBuffer,
        AudioBufferInterval,
        true // Repetir
    );
    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Reprodução iniciada."));
}

void UIARMIDIToAudioSynthesizer::StopPlayback()
{
    
    if (!this->IsValidLowLevel())return;
    
    UWorld* World = nullptr;
    
    World = GetWorld();
    // NOVO: Verifica se o GetWorld() é válido antes de tentar limpar o timer
    if (World->IsValidLowLevel())
    {
        if (World->GetTimerManager().IsTimerActive(AudioGenerationTimerHandle))
        {
            World->GetTimerManager().ClearTimer(AudioGenerationTimerHandle);
        }
    }
    else
    {
        // Adiciona um aviso se GetWorld() for nulo, indicando que estamos em um estágio avançado de desligamento.
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDIToAudioSynthesizer: GetWorld() retornou null durante StopPlayback. Timer pode não ter sido limpo explicitamente."));
    }

    // <--- NOVO: Para o AudioPlayerComponent
    if (AudioPlayerComponent && AudioPlayerComponent->IsPlaying())
    {
        AudioPlayerComponent->Stop();
    }
    // NOVO FIM --->

    UE_LOG(LogIAR, Log, TEXT("UIARMIDIToAudioSynthesizer: Reprodução parada."));
}

void UIARMIDIToAudioSynthesizer::GenerateAudioBuffer()
{
    if (!SynthesizedSoundWave) return;

    const int32 NumSamplesPerBuffer = FMath::RoundToInt(SampleRate * AudioBufferInterval);
    const int32 TotalSamples = NumSamplesPerBuffer * NumChannels;
    
    TArray<float> AudioBuffer;
    AudioBuffer.SetNumZeroed(TotalSamples);

    TArray<int32> NotesToDeactivate; // Lista de notas (MIDI Note Number) a serem removidas do mapa ActiveVoices

    // Itera para gerar amostras
    for (int32 i = 0; i < NumSamplesPerBuffer; ++i)
    {
        float MixedSample = 0.0f;
        int32 CurrentActiveVoices = 0;

        // Itera sobre as vozes ativas usando um iterador para permitir modificação segura (remoção)
        for (auto It = ActiveVoices.CreateIterator(); It; ++It)
        {
            FSynthesizedVoice* Voice = It.Value(); // Obtém o ponteiro para a voz real
            if (Voice->EnvelopeState != FSynthesizedVoice::Off)
            {
                MixedSample += Voice->GenerateSample(); 
                CurrentActiveVoices++;
            }
            else
            {
                NotesToDeactivate.Add(It.Key()); // Marca para remover vozes que terminaram
            }
        }

        if (CurrentActiveVoices > 0)
        {
            MixedSample /= FMath::Min(CurrentActiveVoices, MaxPolyphony); 
            MixedSample = FMath::Clamp(MixedSample, -1.0f, 1.0f); 
        }
        else
        {
            MixedSample = 0.0f;
        }

        // Preenche o buffer de áudio (canais intercalados para estéreo)
        for (int32 Channel = 0; Channel < NumChannels; ++Channel)
        {
            AudioBuffer[i * NumChannels + Channel] = MixedSample;
        }

        CurrentSynthesizerTime += 1.0f / SampleRate;
    }

    // Remove as vozes que terminaram da lista de vozes ativas (após a iteração sobre o mapa)
    for (int32 NoteToDeactivate : NotesToDeactivate)
    {
        ActiveVoices.Remove(NoteToDeactivate);
    }

    if (AudioBuffer.Num() > 0)
    {
        // Converte amostras float (entre -1.0 e 1.0) para PCM de 16 bits (entre -32768 e 32767)
        TArray<int16> PCMBuffer;
        PCMBuffer.SetNumUninitialized(AudioBuffer.Num());
        for (int32 i = 0; i < AudioBuffer.Num(); ++i)
        {
            PCMBuffer[i] = FMath::Clamp(AudioBuffer[i], -1.0f, 1.0f) * 32767.0f;
        }

        // Enfileira os dados PCM de 16 bits
        SynthesizedSoundWave->QueueAudio(reinterpret_cast<const uint8*>(PCMBuffer.GetData()), PCMBuffer.Num() * sizeof(int16)); 
    }
    
    OnSynthesizedAudioFrameReady.Broadcast(AudioBuffer);
}
