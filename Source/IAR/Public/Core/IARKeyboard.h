// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/IAR_Types.h" 
#include "Core/IARMIDITable.h" 

#include "MIDIDeviceController.h"
#include "MIDIDeviceManager.h" 

#include "IARLambdaLatentAction.h" 
#include "TimerManager.h"
// Incluir a biblioteca MidiFile (necess�rio para ler o arquivo .mid)
#include "MIDI/MidiFile.h" 

#include "IARKeyboard.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMIDIEventGenerated, const FIAR_MIDIEvent&, MIDIEvent);

// NOVO DELEGATE: Para notificar quando a reprodu��o do arquivo MIDI termina
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMidiFilePlaybackCompleted, const FString&, FilePath);


/**
 * @brief Simula um teclado musical e interage com dispositivos MIDI externos via plugin "MIDI Device Support".
 * Permite enviar eventos MIDI Note On/Off para o dispositivo de sa�da selecionado.
 * Agora tamb�m permite receber eventos MIDI de um dispositivo de entrada.
 */
UCLASS(BlueprintType, Blueprintable)
class IAR_API UIARKeyboard : public UObject
{
    GENERATED_BODY()

public:
    UIARKeyboard();
    virtual ~UIARKeyboard();

    /**
     * @brief Inicializa o teclado MIDI, tentando se conectar a dispositivos de entrada e sa�da espec�ficos.
     * @param InInputDeviceID O ID do dispositivo MIDI de entrada ao qual se conectar. Use -1 para nenhum dispositivo de entrada.
     * @param InOutputDeviceID O ID do dispositivo MIDI de sa�da ao qual se conectar. Use -1 para o primeiro dispositivo dispon�vel.
     * @param InDefaultVelocity Velocidade padr�o para as notas MIDI (0-127).
     * @param InMIDIChannel O canal MIDI a ser usado (0-15).
     * @return True se a conex�o com o dispositivo MIDI de sa�da foi bem-sucedida, false caso contr�rio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard")
    bool InitializeKeyboard(int32 InInputDeviceID = -1, int32 InOutputDeviceID = -1, uint8 InDefaultVelocity = 90, uint8 InMIDIChannel = 0);

    /**
     * @brief Lista todos os dispositivos MIDI de entrada e sa�da dispon�veis.
     * @param OutInputDeviceNames Array para preencher com nomes/IDs de dispositivos de entrada.
     * @param OutOutputDeviceNames Array para preencher com nomes/IDs de dispositivos de sa�da.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|MIDI|Keyboard")
    static void GetAvailableMIDIDeviceNames(TArray<FString>& OutInputDeviceNames, TArray<FString>& OutOutputDeviceNames);

    /**
     * @brief Pressiona uma nota MIDI, enviando um Note On para o dispositivo de sa�da.
     * @param MIDINoteNumber O n�mero da nota MIDI (0-127).
     * @param InVelocity A velocidade (for�a) com que a nota � pressionada (0-127). Se 0, usa DefaultVelocity.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard")
    void PressNote(int32 MIDINoteNumber, uint8 InVelocity = 0);

    /**
     * @brief Solta uma nota MIDI, enviando um Note Off para o dispositivo de sa�da.
     * @param MIDINoteNumber O n�mero da nota MIDI (0-127).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard")
    void ReleaseNote(int32 MIDINoteNumber);

    /**
     * @brief Pressiona e solta uma nota MIDI por um per�odo, enviando mensagens Note On/Off para o dispositivo de sa�da.
     * �til para simular toques r�pidos ou para testar.
     * @param WorldContextObject Contexto do mundo para a��es latentes.
     * @param LatentInfo Informa��es para o sistema de a��es latentes do Blueprint.
     * @param MIDINoteNumber O n�mero da nota MIDI (0-127).
     * @param DurationSeconds Dura��o em segundos que a nota permanecer� pressionada.
     * @param InVelocity A velocidade (for�a) com que a nota � pressionada (0-127). Se 0, usa DefaultVelocity.
     * @param InOutputDeviceID O ID do dispositivo MIDI de sa�da a ser usado para esta nota. Use -1 para o primeiro dispon�vel.
     * @param InMIDIChannel O canal MIDI a ser usado (0-15).
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
    static void PlayNoteOnce(UObject* WorldContextObject, FLatentActionInfo LatentInfo, int32 MIDINoteNumber, float DurationSeconds = 0.5f, uint8 InVelocity = 90, int32 InOutputDeviceID = -1, uint8 InMIDIChannel = 0);

    /**
     * @brief Carrega um arquivo MIDI e o reproduz atrav�s do dispositivo MIDI de sa�da configurado.
     * @param FilePath Caminho completo para o arquivo MIDI (.mid) no disco.
     * @return true se a reprodu��o foi iniciada com sucesso, false caso contr�rio.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard")
    bool PlayMidiFile(const FString& FilePath);

    /**
     * @brief Para a reprodu��o do arquivo MIDI atualmente em andamento.
     */
    UFUNCTION(BlueprintCallable, Category = "IAR|MIDI|Keyboard")
    void StopMidiFilePlayback();


    // Delegate a ser disparado quando um evento MIDI � gerado/enviado (para MIDI In e Out)
    UPROPERTY(BlueprintAssignable, Category = "IAR|MIDI|Keyboard")
    FOnMIDIEventGenerated OnMIDIEventGenerated;

    // NOVO DELEGATE: Notifica quando a reprodu��o de um arquivo MIDI termina
    UPROPERTY(BlueprintAssignable, Category = "IAR|MIDI|Keyboard")
    FOnMidiFilePlaybackCompleted OnMidiFilePlaybackCompleted;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|MIDI|Keyboard|Data")
    FIAR_DiatonicNoteEntry GetNoteByMIDIPitch(int32 MIDINoteNumber) const { return UIARMIDITable::GetNoteEntryByMIDIPitch(MIDINoteNumber); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "IAR|MIDI|Keyboard|Data")
    FString GetNoteNameByMIDIPitch(int32 MIDINoteNumber) const { return UIARMIDITable::GetNoteEntryByMIDIPitch(MIDINoteNumber).NoteName; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|MIDI|Keyboard")
    uint8 DefaultVelocity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IAR|MIDI|Keyboard")
    uint8 MIDIChannel;

    // NOVO: Taxa de tick para o playback de arquivo MIDI
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|MIDI|Keyboard|Playback")
    float PlaybackTickRate = 0.005f; // Intervalo padr�o de 5ms

private:
    UPROPERTY()
    UMIDIDeviceInputController* InputMIDIDeviceController; 
    UPROPERTY()
    UMIDIDeviceOutputController* OutputMIDIDeviceController; 

    TSet<int32> ActiveNotes; // Notas atualmente "pressionadas" (para o MIDI Out em tempo real)

    // NOVOS MEMBROS PARA REPRODU��O DE ARQUIVO MIDI
    smf::MidiFile LoadedMidiFile; // Objeto da biblioteca smf para ler e armazenar o MIDI
    TArray<FIAR_MIDIEvent> PlaybackEvents; // Lista plana de eventos MIDI a serem reproduzidos
    int32 CurrentPlaybackEventIndex; // �ndice do pr�ximo evento a ser despachado
    float CurrentPlaybackTimeSeconds; // Tempo atual da reprodu��o do arquivo (em segundos)
    FTimerHandle MidiPlaybackTimerHandle; // Timer para despachar eventos MIDI
    FString CurrentMidiFilePath; // Path do arquivo MIDI atualmente em reprodu��o
    bool bIsMidiPlaying; // Flag para indicar se a reprodu��o est� ativa

    // Fun��es internas para lidar com eventos MIDI recebidos (MIDI In)
    UFUNCTION()
    void HandleReceivedNoteOn(UMIDIDeviceInputController* MIDIDeviceController, int32 Timestamp, int32 Channel, int32 Note, int32 Velocity);
    UFUNCTION()
    void HandleReceivedNoteOff(UMIDIDeviceInputController* MIDIDeviceController, int32 Timestamp, int32 Channel, int32 Note, int32 Velocity);

    // Fun��o interna para despachar os eventos MIDI do arquivo
    void ProcessMidiPlayback();

    // Fun��o interna para broadcast de eventos MIDI (para MIDI In e Out)
    void BroadcastMIDIEvent(uint8 Status, uint8 Data1, uint8 Data2);
};
