// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Core/IARSampleRateConverter.h"
#include "../IAR.h" // Para LogIAR

FIARSampleRateConverter::FIARSampleRateConverter()
    : InputSampleRate(0)
    , OutputSampleRate(0)
    , NumChannels(0)
    , SampleRateRatio(0.0f)
{
}

bool FIARSampleRateConverter::Initialize(int32 InInputSampleRate, int32 InOutputSampleRate, int32 InNumChannels)
{
    if (InInputSampleRate <= 0 || InOutputSampleRate <= 0 || InNumChannels <= 0)
    {
        UE_LOG(LogIAR, Error, TEXT("FIARSampleRateConverter: Taxas de amostragem ou n�mero de canais inv�lidos para inicializa��o."));
        return false;
    }

    InputSampleRate = InInputSampleRate;
    OutputSampleRate = InOutputSampleRate;
    NumChannels = InNumChannels;
    SampleRateRatio = (float)InputSampleRate / (float)OutputSampleRate;

    Phase.Init(0.0f, NumChannels); // Inicializa a fase para cada canal

    UE_LOG(LogIAR, Log, TEXT("FIARSampleRateConverter: Inicializado. Input SR: %d, Output SR: %d, Channels: %d, Ratio: %.4f"),
        InputSampleRate, OutputSampleRate, NumChannels, SampleRateRatio);

    return true;
}

bool FIARSampleRateConverter::Convert(const TArray<float>& InSamples, TArray<float>& OutSamples)
{
    if (InputSampleRate == 0 || OutputSampleRate == 0 || NumChannels == 0)
    {
        UE_LOG(LogIAR, Error, TEXT("FIARSampleRateConverter: Conversor n�o inicializado. Chame Initialize primeiro."));
        return false;
    }

    if (InSamples.Num() == 0)
    {
        OutSamples.SetNum(0);
        return true; // Nada para converter
    }

    // Se as taxas s�o as mesmas, apenas copia os dados
    if (InputSampleRate == OutputSampleRate)
    {
        OutSamples = InSamples;
        return true;
    }

    // Calcular o n�mero de amostras de sa�da
    // Estimativa inicial: (NumSamplesIn * OutputSampleRate) / InputSampleRate
    // Ajuste com base na fase acumulada para garantir precis�o
    const int32 NumInputFrames = InSamples.Num() / NumChannels;
    const int32 EstimatedOutputFrames = FMath::CeilToInt((float)NumInputFrames / SampleRateRatio) + 1; // +1 para garantir espa�o
    OutSamples.SetNumUninitialized(EstimatedOutputFrames * NumChannels);

    int32 CurrentOutputFrame = 0;

    for (int32 Channel = 0; Channel < NumChannels; ++Channel)
    {
        int32 InputSampleOffset = Channel; // Offset para o canal atual na amostra de entrada
        CurrentOutputFrame = 0; // Reinicia para cada canal para preencher OutSamples

        while (true)
        {
            // Posi��o flutuante no buffer de entrada
            float InputPos = Phase[Channel];

            // �ndice da amostra de entrada atual e pr�xima
            int32 Index0 = FMath::FloorToInt(InputPos);
            int32 Index1 = Index0 + 1;

            // Se o Index1 est� fora dos limites do buffer de entrada, n�o h� mais dados para interpolar.
            // Precisamos sair do loop ou preencher com zeros se o frame for incompleto.
            // Para interpola��o cont�nua, guardamos a fase para a pr�xima chamada.
            if ((Index1 * NumChannels + Channel) >= InSamples.Num())
            {
                // Se estamos no final do buffer de entrada, ajustamos a fase para o pr�ximo frame
                // A fase acumulada � a parte fracion�ria da posi��o de leitura.
                // Reajustamos para ser relativo ao in�cio do pr�ximo "bloco" de input.
                Phase[Channel] = InputPos - NumInputFrames; // Guarda a sobra para a pr�xima chamada
                break; 
            }

            // Fator de interpola��o (parte fracion�ria da posi��o)
            float Frac = InputPos - Index0;

            // Valores das amostras de entrada para interpola��o
            float Sample0 = InSamples[Index0 * NumChannels + Channel];
            float Sample1 = InSamples[Index1 * NumChannels + Channel];

            // Interpola��o linear
            float OutputSample = Sample0 + (Sample1 - Sample0) * Frac;

            // Adiciona a amostra resampleada ao buffer de sa�da
            OutSamples[CurrentOutputFrame * NumChannels + Channel] = OutputSample;

            // Move para a pr�xima posi��o de sa�da no buffer de entrada
            Phase[Channel] += SampleRateRatio; // Incrementa a fase para o pr�ximo sample de sa�da

            CurrentOutputFrame++;
            if (CurrentOutputFrame * NumChannels + Channel >= OutSamples.Num())
            {
                // Parar se o buffer de sa�da estimado estiver cheio, ser� redimensionado na pr�xima itera��o
                break;
            }
        }
    }
    // Redimensiona OutSamples para o tamanho real que foi preenchido.
    // � importante fazer isso para n�o enviar dados "lixo" para o encoder.
    OutSamples.SetNum(CurrentOutputFrame * NumChannels, EAllowShrinking::No); 
    return true;
}
