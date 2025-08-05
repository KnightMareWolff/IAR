// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"

/**
 * @brief Utilit�rio para convers�o de canais de �udio (mono para est�reo, est�reo para mono).
 */
struct IAR_API FIARChannelConverter
{
public:
    /**
     * @brief Converte amostras de �udio entre diferentes contagens de canais.
     * @param InSamples O TArray<float> contendo as amostras de �udio de entrada (intercaladas para multi-canal).
     * @param InNumChannels O n�mero de canais nas amostras de entrada.
     * @param OutSamples O TArray<float> onde as amostras de �udio convertidas ser�o armazenadas. Ser� redimensionado.
     * @param OutNumChannels O n�mero de canais desejado para a sa�da.
     * @return True se a convers�o for bem-sucedida, false caso contr�rio (convers�o n�o suportada, dados inv�lidos).
     */
    static bool Convert(const TArray<float>& InSamples, int32 InNumChannels, TArray<float>& OutSamples, int32 OutNumChannels);
};
