// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/IAR_Types.h" // Para FIAR_AudioFrameData, FIAR_MIDIFrame (USTRUCTs)
#include "Core/IARFramePool.h" // Para UIARFramePool


#include "IARMediaSource.generated.h"


// --- DELEGATES INTERNOS (C++ Only - usam TSharedPtr, para uso interno e eficiente) ---
// Estas s�o as declara��es de tipo dos delegates.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioFrameAcquired, TSharedPtr<FIAR_AudioFrameData>);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMIDIFrameAcquired, TSharedPtr<FIAR_MIDIFrame>);

/**
 * @brief Classe base abstrata para todas as fontes de m�dia do plugin IAR (�udio ou MIDI).
 * Define a interface comum para inicializa��o, captura e parada de m�dia.
 * Pode emitir frames de �udio (FIAR_AudioFrameData) ou frames MIDI (FIAR_MIDIFrame).
 */
UCLASS(Abstract, BlueprintType)
class IAR_API UIARMediaSource : public UObject
{
    GENERATED_BODY()

public:
    UIARMediaSource();
    virtual ~UIARMediaSource();

    /**
     * @brief Inicializa a fonte de m�dia.
     * @param StreamSettings As configura��es do stream de m�dia desejadas para esta fonte.
     * @param InFramePool O FramePool a ser utilizado para aquisi��o de frames de �udio (pode ser nulo para MIDI).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Media Source")
    virtual void Initialize(FIAR_AudioStreamSettings& StreamSettings, UIARFramePool* InFramePool);

    /**
     * @brief Inicia a captura de m�dia da fonte.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Media Source")
    virtual void StartCapture();

    /**
     * @brief Para a captura de m�dia da fonte.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Media Source")
    virtual void StopCapture();

    /**
     * @brief Desliga e limpa os recursos da fonte de m�dia.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|Media Source")
    virtual void Shutdown();

    // Retorna o estado atual da captura
    UFUNCTION(BlueprintCallable, Category = "IAR|Media Source")
    bool IsCapturing() const { return bIsCapturing; }

    /**
     * @brief Retorna as configura��es atuais do stream de m�dia.
     * @return Uma refer�ncia constante para as configura��es do stream.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|Media Source")
    FIAR_AudioStreamSettings& GetCurrentStreamSettings(){ return CurrentStreamSettings; }


    // --- DELEGATES INTERNOS (C++ Only - usam TSharedPtr) ---
    // Estes delegates n�o s�o BlueprintAssignable, pois usam TSharedPtr.
    // Eles servem como a interface de comunica��o de frames em C++.
    FOnAudioFrameAcquired OnAudioFrameAcquired; 
    FOnMIDIFrameAcquired OnMIDIFrameAcquired;   


protected:
    FIAR_AudioStreamSettings CurrentStreamSettings; // Armazena as configura��es atuais do stream
    bool bIsCapturing = false; // Flag para controlar o estado da captura
    
    UPROPERTY()
    UIARFramePool* FramePool; // Refer�ncia ao pool de frames (opcional, para fontes MIDI n�o precisa)
};
