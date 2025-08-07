// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "IARMIDITable.generated.h"

/**
 * @brief Representa uma entrada na tabela diat�nica de notas MIDI.
 * Cont�m informa��es est�ticas para cada uma das 128 notas MIDI.
 */
USTRUCT(BlueprintType)
struct IAR_API FIAR_DiatonicNoteEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    FString NoteName = TEXT(""); // Ex: "C", "Db", "G#"

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    bool bIsBemol = false; // Indica se a nota � um bemol (ex: Db)

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    bool bIsSharp = false; // Indica se a nota � um sustenido (ex: C#)

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    int32 NotePitch = 0; // O n�mero do pitch MIDI (0-127)

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    int32 Octave = 0; // A oitava da nota

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    float Frequency = 0.0f; // A frequ�ncia em Hz

    UPROPERTY(BlueprintReadOnly, Category = "IAR|MIDI Table")
    int32 MidiShortMessage = 0; // Valor do Midi Short Message (�til para MIDI completo)
};

/**
 * @brief Tabela est�tica de informa��es de notas MIDI.
 * Fornece m�todos para lookup de informa��es de notas com base no pitch MIDI.
 */
UCLASS()
class IAR_API UIARMIDITable : public UObject
{
    GENERATED_BODY()

public:
    /**
     * @brief Inicializa a tabela MIDI. Deve ser chamado uma �nica vez na inicializa��o do m�dulo.
     */
    static void InitializeMIDITable();

    /**
     * @brief Retorna uma entrada da tabela de notas diat�nicas para um dado pitch MIDI.
     * @param MIDIPitch O n�mero do pitch MIDI (0-127).
     * @return A estrutura FIAR_DiatonicNoteEntry correspondente, ou uma vazia se inv�lido.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|MIDI Table")
    static FIAR_DiatonicNoteEntry GetNoteEntryByMIDIPitch(int32 MIDIPitch);

private:
    static TArray<FIAR_DiatonicNoteEntry> MIDITable;
    static bool bIsMIDITableInitialized;
};
