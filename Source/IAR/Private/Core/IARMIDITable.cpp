// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "Core/IARMIDITable.h"
#include "../IAR.h" // Para LogIAR

TArray<FIAR_DiatonicNoteEntry> UIARMIDITable::MIDITable;
bool UIARMIDITable::bIsMIDITableInitialized = false;

void UIARMIDITable::InitializeMIDITable()
{
    if (bIsMIDITableInitialized)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARMIDITable: Tabela MIDI j� inicializada."));
        return;
    }

    MIDITable.SetNum(128); // 0-127 MIDI notes

    // Popula a tabela com os dados fornecidos no MIDINotesTable.txt
    // Esta � a transcri��o direta do seu arquivo C++ para preencher a TArray
    // Nota: 'tNote' foi substitu�da por 'MIDITable[i]' para preenchimento direto
    for (int i = 0; i < 128; i++)
    {
        FIAR_DiatonicNoteEntry tNote; // Usamos uma tempor�ria para facilitar a atribui��o
        switch (i)
        {
        case  0 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 8.1757f; tNote.MidiShortMessage = 0x00400090;   }break;
        case  1 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 8.6619f; tNote.MidiShortMessage = 0x00400190;   }break;
        case  2 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 9.1770f; tNote.MidiShortMessage = 0x00400290;   }break;
        case  3 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 9.7227f; tNote.MidiShortMessage = 0x00400390;   }break;
        case  4 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 10.300f; tNote.MidiShortMessage = 0x00400490;   }break;
        case  5 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 10.913f; tNote.MidiShortMessage = 0x00400590;   }break;
        case  6 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 11.562f; tNote.MidiShortMessage = 0x00400690;   }break;
        case  7 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 12.249f; tNote.MidiShortMessage = 0x00400790;   }break;
        case  8 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 12.978f; tNote.MidiShortMessage = 0x00400890;   }break;
        case  9 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 13.750f; tNote.MidiShortMessage = 0x00400990;   }break;
        case 10 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 14.567f; tNote.MidiShortMessage = 0x00400A90;   }break;
        case 11 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave = -1; tNote.Frequency = 15.433f; tNote.MidiShortMessage = 0x00400B90;   }break;
        case 12 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 16.351f; tNote.MidiShortMessage = 0x00400C90;   }break;
        case 13 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 17.323f; tNote.MidiShortMessage = 0x00400D90;   }break;
        case 14 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 18.354f; tNote.MidiShortMessage = 0x00400E90;   }break;
        case 15 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 19.445f; tNote.MidiShortMessage = 0x00400F90;   }break;
        case 16 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 20.601f; tNote.MidiShortMessage = 0x00401090;   }break;
        case 17 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 21.826f; tNote.MidiShortMessage = 0x00401190;   }break;
        case 18 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 23.124f; tNote.MidiShortMessage = 0x00401290;   }break;
        case 19 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 24.499f; tNote.MidiShortMessage = 0x00401390;   }break;
        case 20 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 25.956f; tNote.MidiShortMessage = 0x00401490;   }break;
        case 21 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 27.500f; tNote.MidiShortMessage = 0x00401590;   }break;
        case 22 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 29.135f; tNote.MidiShortMessage = 0x00401690;   }break;
        case 23 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  0; tNote.Frequency = 30.868f; tNote.MidiShortMessage = 0x00401790;   }break;
        case 24 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 32.703f; tNote.MidiShortMessage = 0x00401890;   }break;
        case 25 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 34.648f; tNote.MidiShortMessage = 0x00401990;   }break;
        case 26 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 36.708f; tNote.MidiShortMessage = 0x00401A90;   }break;
        case 27 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 38.891f; tNote.MidiShortMessage = 0x00401B90;   }break;
        case 28 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 41.203f; tNote.MidiShortMessage = 0x00401C90;   }break;
        case 29 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 43.654f; tNote.MidiShortMessage = 0x00401D90;   }break;
        case 30 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 46.249f; tNote.MidiShortMessage = 0x00401E90;   }break;
        case 31 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 48.999f; tNote.MidiShortMessage = 0x00401F90;   }break;
        case 32 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 51.913f; tNote.MidiShortMessage = 0x00402090;   }break;
        case 33 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 55.000f; tNote.MidiShortMessage = 0x00402190;   }break;
        case 34 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 58.270f; tNote.MidiShortMessage = 0x00402290;   }break;
        case 35 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  1; tNote.Frequency = 61.735f; tNote.MidiShortMessage = 0x00402390;   }break;
        case 36 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 65.406f; tNote.MidiShortMessage = 0x00402490;   }break;
        case 37 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 69.296f; tNote.MidiShortMessage = 0x00402590;   }break;
        case 38 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 73.416f; tNote.MidiShortMessage = 0x00402690;   }break;
        case 39 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 77.782f; tNote.MidiShortMessage = 0x00402790;   }break;
        case 40 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 82.407f; tNote.MidiShortMessage = 0x00402890;   }break;
        case 41 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 87.307f; tNote.MidiShortMessage = 0x00402990;   }break;
        case 42 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 92.499f; tNote.MidiShortMessage = 0x00402A90;   }break;
        case 43 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 97.999f; tNote.MidiShortMessage = 0x00402B90;   }break;
        case 44 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 103.83f; tNote.MidiShortMessage = 0x00402C90;   }break;
        case 45 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 110.00f; tNote.MidiShortMessage = 0x00402D90;   }break;
        case 46 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 116.54f; tNote.MidiShortMessage = 0x00402E90;   }break;
        case 47 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  2; tNote.Frequency = 123.47f; tNote.MidiShortMessage = 0x00402F90;   }break;
        case 48 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 130.81f; tNote.MidiShortMessage = 0x00403090;   }break;
        case 49 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 138.59f; tNote.MidiShortMessage = 0x00403190;   }break;
        case 50 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 146.83f; tNote.MidiShortMessage = 0x00403290;   }break;
        case 51 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 155.56f; tNote.MidiShortMessage = 0x00403390;   }break;
        case 52 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 164.81f; tNote.MidiShortMessage = 0x00403490;   }break;
        case 53 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 174.61f; tNote.MidiShortMessage = 0x00403590;   }break;
        case 54 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 185.00f; tNote.MidiShortMessage = 0x00403690;   }break;
        case 55 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 196.00f; tNote.MidiShortMessage = 0x00403790;   }break;
        case 56 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 207.65f; tNote.MidiShortMessage = 0x00403890;   }break;
        case 57 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 220.00f; tNote.MidiShortMessage = 0x00403990;   }break;
        case 58 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 233.08f; tNote.MidiShortMessage = 0x00403A90;   }break;
        case 59 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  3; tNote.Frequency = 246.94f; tNote.MidiShortMessage = 0x00403B90;   }break;
        case 60 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 261.63f; tNote.MidiShortMessage = 0x00403C90;   }break;
        case 61 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 277.18f; tNote.MidiShortMessage = 0x00403D90;   }break;
        case 62 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 293.67f; tNote.MidiShortMessage = 0x00403E90;   }break;
        case 63 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 311.13f; tNote.MidiShortMessage = 0x00403F90;   }break;
        case 64 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 329.63f; tNote.MidiShortMessage = 0x00404090;   }break;
        case 65 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 349.23f; tNote.MidiShortMessage = 0x00404190;   }break;
        case 66 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 369.99f; tNote.MidiShortMessage = 0x00404290;   }break;
        case 67 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 392.00f; tNote.MidiShortMessage = 0x00404390;   }break;
        case 68 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 415.30f; tNote.MidiShortMessage = 0x00404490;   }break;
        case 69 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 440.00f; tNote.MidiShortMessage = 0x00404590;   }break;
        case 70 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 466.16f; tNote.MidiShortMessage = 0x00404690;   }break;
        case 71 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  4; tNote.Frequency = 493.88f; tNote.MidiShortMessage = 0x00404790;   }break;
        case 72 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 523.25f; tNote.MidiShortMessage = 0x00404890;   }break;
        case 73 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 554.37f; tNote.MidiShortMessage = 0x00404990;   }break;
        case 74 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 587.33f; tNote.MidiShortMessage = 0x00404A90;   }break;
        case 75 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 622.25f; tNote.MidiShortMessage = 0x00404B90;   }break;
        case 76 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 659.26f; tNote.MidiShortMessage = 0x00404C90;   }break;
        case 77 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 698.46f; tNote.MidiShortMessage = 0x00404D90;   }break;
        case 78 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 739.99f; tNote.MidiShortMessage = 0x00404E90;   }break;
        case 79 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 783.99f; tNote.MidiShortMessage = 0x00404F90;   }break;
        case 80 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 830.61f; tNote.MidiShortMessage = 0x00405090;   }break;
        case 81 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 880.00f; tNote.MidiShortMessage = 0x00405190;   }break;
        case 82 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 932.33f; tNote.MidiShortMessage = 0x00405290;   }break;
        case 83 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  5; tNote.Frequency = 987.77f; tNote.MidiShortMessage = 0x00405390;   }break;
        case 84 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1046.5f; tNote.MidiShortMessage = 0x00405490;   }break;
        case 85 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1108.7f; tNote.MidiShortMessage = 0x00405590;   }break;
        case 86 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1174.7f; tNote.MidiShortMessage = 0x00405690;   }break;
        case 87 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1244.5f; tNote.MidiShortMessage = 0x00405790;   }break;
        case 88 : {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1318.5f; tNote.MidiShortMessage = 0x00405890;   }break;
        case 89 : {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1396.9f; tNote.MidiShortMessage = 0x00405990;   }break;
        case 90 : {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1480.0f; tNote.MidiShortMessage = 0x00405A90;   }break;
        case 91 : {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1568.0f; tNote.MidiShortMessage = 0x00405B90;   }break;
        case 92 : {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1661.2f; tNote.MidiShortMessage = 0x00405C90;   }break;
        case 93 : {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1760.0f; tNote.MidiShortMessage = 0x00405D90;   }break;
        case 94 : {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1864.7f; tNote.MidiShortMessage = 0x00405E90;   }break;
        case 95 : {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  6; tNote.Frequency = 1975.5f; tNote.MidiShortMessage = 0x00405F90;   }break;
        case 96 : {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2093.0f; tNote.MidiShortMessage = 0x00406090;   }break;
        case 97 : {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2217.5f; tNote.MidiShortMessage = 0x00406190;   }break;
        case 98 : {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2349.3f; tNote.MidiShortMessage = 0x00406290;   }break;
        case 99 : {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2489.0f; tNote.MidiShortMessage = 0x00406390;   }break;
        case 100: {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2637.0f; tNote.MidiShortMessage = 0x00406490;   }break;
        case 101: {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2793.0f; tNote.MidiShortMessage = 0x00406590;   }break;
        case 102: {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 2960.0f; tNote.MidiShortMessage = 0x00406690;   }break;
        case 103: {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 3136.0f; tNote.MidiShortMessage = 0x00406790;   }break;
        case 104: {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 3322.4f; tNote.MidiShortMessage = 0x00406890;   }break;
        case 105: {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 3520.0f; tNote.MidiShortMessage = 0x00406990;   }break;
        case 106: {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 3729.3f; tNote.MidiShortMessage = 0x00406A90;   }break;
        case 107: {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  7; tNote.Frequency = 3951.1f; tNote.MidiShortMessage = 0x00406B90;   }break;
        case 108: {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 4186.0f; tNote.MidiShortMessage = 0x00406C90;   }break;
        case 109: {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 4434.9f; tNote.MidiShortMessage = 0x00406D90;   }break;
        case 110: {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 4698.6f; tNote.MidiShortMessage = 0x00406E90;   }break;
        case 111: {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 4978.0f; tNote.MidiShortMessage = 0x00406F90;   }break;
        case 112: {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 5274.0f; tNote.MidiShortMessage = 0x00407090;   }break;
        case 113: {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 5587.6f; tNote.MidiShortMessage = 0x00407190;   }break;
        case 114: {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 5919.9f; tNote.MidiShortMessage = 0x00407290;   }break;
        case 115: {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 6271.9f; tNote.MidiShortMessage = 0x00407390;   }break;
        case 116: {tNote.NoteName = "Ab"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 6644.8f; tNote.MidiShortMessage = 0x00407490;   }break;
        case 117: {tNote.NoteName = "A" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 7040.0f; tNote.MidiShortMessage = 0x00407590;   }break;
        case 118: {tNote.NoteName = "Bb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 7458.6f; tNote.MidiShortMessage = 0x00407690;   }break;
        case 119: {tNote.NoteName = "B" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  8; tNote.Frequency = 7902.1f; tNote.MidiShortMessage = 0x00407790;   }break;
        case 120: {tNote.NoteName = "C" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 8372.0f; tNote.MidiShortMessage = 0x00407890;   }break;
        case 121: {tNote.NoteName = "Db"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 8869.8f; tNote.MidiShortMessage = 0x00407990;   }break;
        case 122: {tNote.NoteName = "D" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 9397.2f; tNote.MidiShortMessage = 0x00407A90;   }break;
        case 123: {tNote.NoteName = "Eb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 9956.0f; tNote.MidiShortMessage = 0x00407B90;   }break;
        case 124: {tNote.NoteName = "E" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 10548.0f; tNote.MidiShortMessage = 0x00407C90;   }break;
        case 125: {tNote.NoteName = "F" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 11175.3f; tNote.MidiShortMessage = 0x00407D90;   }break;
        case 126: {tNote.NoteName = "Gb"; tNote.bIsBemol = true ; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 11839.8f; tNote.MidiShortMessage = 0x00407E90;   }break;
        case 127: {tNote.NoteName = "G" ; tNote.bIsBemol = false; tNote.bIsSharp = false; tNote.NotePitch = i; tNote.Octave =  9; tNote.Frequency = 12543.8f; tNote.MidiShortMessage = 0x00407F90;   }break;
        }
        MIDITable[i] = tNote;
    }
    bIsMIDITableInitialized = true;
    UE_LOG(LogIAR, Log, TEXT("UIARMIDITable: Tabela MIDI inicializada com 128 entradas."));
}

FIAR_DiatonicNoteEntry UIARMIDITable::GetNoteEntryByMIDIPitch(int32 MIDIPitch)
{
    if (!bIsMIDITableInitialized)
    {
        // Garante que a tabela est� inicializada mesmo se o StartupModule n�o for chamado diretamente (cen�rios de PIE)
        InitializeMIDITable(); 
    }

    if (MIDIPitch >= 0 && MIDIPitch < MIDITable.Num())
    {
        return MIDITable[MIDIPitch];
    }
    
    UE_LOG(LogIAR, Warning, TEXT("UIARMIDITable: MIDIPitch inv�lido (%d) solicitado. Retornando entrada vazia."), MIDIPitch);
    return FIAR_DiatonicNoteEntry(); // Retorna uma estrutura vazia se o pitch for inv�lido
}
