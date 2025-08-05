// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Core/IARChannelConverter.h"
#include "../IAR.h" // Para logging

bool FIARChannelConverter::Convert(const TArray<float>& InSamples, int32 InNumChannels, TArray<float>& OutSamples, int32 OutNumChannels)
{
    if (InNumChannels <= 0 || OutNumChannels <= 0)
    {
        UE_LOG(LogIAR, Error, TEXT("FIARChannelConverter: Contagem de canais inv�lida. Entrada: %d, Sa�da: %d"), InNumChannels, OutNumChannels);
        return false;
    }

    if (InSamples.Num() == 0)
    {
        OutSamples.SetNum(0);
        return true; // Nada para converter
    }

    if (InNumChannels == OutNumChannels)
    {
        OutSamples = InSamples;
        return true; // Nenhuma convers�o necess�ria
    }

    int32 InNumFrames = InSamples.Num() / InNumChannels;
    OutSamples.SetNumUninitialized(InNumFrames * OutNumChannels);

    if (InNumChannels == 1 && OutNumChannels == 2) // Mono para Est�reo
    {
        UE_LOG(LogIAR, Log, TEXT("FIARChannelConverter: Convertendo de Mono para Est�reo."));
        for (int32 i = 0; i < InNumFrames; ++i)
        {
            OutSamples[i * 2] = InSamples[i];     // Canal Esquerdo
            OutSamples[i * 2 + 1] = InSamples[i]; // Canal Direito (duplica o mono)
        }
        return true;
    }
    else if (InNumChannels == 2 && OutNumChannels == 1) // Est�reo para Mono
    {
        UE_LOG(LogIAR, Log, TEXT("FIARChannelConverter: Convertendo de Est�reo para Mono."));
        for (int32 i = 0; i < InNumFrames; ++i)
        {
            OutSamples[i] = (InSamples[i * 2] + InSamples[i * 2 + 1]) / 2.0f; // M�dia dos canais
        }
        return true;
    }
    else
    {
        UE_LOG(LogIAR, Warning, TEXT("FIARChannelConverter: Convers�o de canais n�o suportada. Entrada: %d, Sa�da: %d"), InNumChannels, OutNumChannels);
        OutSamples.SetNum(0);
        return false;
    }
}
