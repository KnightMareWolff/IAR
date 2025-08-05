// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"

/**
 * @brief Implementa um Sample Rate Converter (SRC) b�sico usando interpola��o linear.
 * Converte amostras de �udio de uma taxa de amostragem de entrada para uma taxa de amostragem de sa�da.
 * N�o � thread-safe por si s�, deve ser usado por uma �nica thread ou com sincroniza��o externa.
 */
struct IAR_API FIARSampleRateConverter
{
public:
    FIARSampleRateConverter();

    /**
     * @brief Inicializa o conversor com as taxas de amostragem de entrada e sa�da.
     * @param InInputSampleRate A taxa de amostragem das amostras de entrada.
     * @param InOutputSampleRate A taxa de amostragem desejada para as amostras de sa�da.
     * @param InNumChannels O n�mero de canais do �udio (ex: 1 para mono, 2 para est�reo).
     * @return True se a inicializa��o for bem-sucedida, false caso contr�rio (taxas inv�lidas).
     */
    bool Initialize(int32 InInputSampleRate, int32 InOutputSampleRate, int32 InNumChannels);

    /**
     * @brief Converte um buffer de amostras de �udio de entrada para a taxa de amostragem de sa�da.
     * @param InSamples O TArray<float> contendo as amostras de �udio de entrada.
     * @param OutSamples O TArray<float> onde as amostras de �udio resampleadas ser�o armazenadas.
     *                   Ser� redimensionado conforme necess�rio.
     * @return True se a convers�o for bem-sucedida, false caso contr�rio.
     */
    bool Convert(const TArray<float>& InSamples, TArray<float>& OutSamples);

    /**
     * @brief Retorna a taxa de amostragem de entrada configurada.
     */
    int32 GetInputSampleRate() const { return InputSampleRate; }

    /**
     * @brief Retorna a taxa de amostragem de sa�da configurada.
     */
    int32 GetOutputSampleRate() const { return OutputSampleRate; }

    /**
     * @brief Retorna numero canais de sa�da configurada.
     */
    int32 GetOutputNumChannels() const { return NumChannels; }

private:

    int32 InputSampleRate;
    int32 OutputSampleRate;
    int32 NumChannels;
    float SampleRateRatio; // InputSampleRate / OutputSampleRate

    // Mant�m o estado da posi��o fracion�ria para interpola��o cont�nua entre chamadas.
    // Isso � importante para evitar "cliques" no �udio resampleado.
    TArray<float> Phase; // Um valor de fase por canal para interpola��o
};
