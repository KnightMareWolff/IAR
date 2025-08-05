// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------

#include "IAR.h"
#include "Core/IARMIDITable.h" // Inclui a tabela MIDI para inicialização

// Define a categoria de log para ser usada nas implementações
DEFINE_LOG_CATEGORY(LogIAR);

#define LOCTEXT_NAMESPACE "IARModule"

void IARModule::StartupModule()
{
    // Este código será executado após o carregamento do seu módulo na memória.
    // O momento exato é especificado no arquivo .uplugin por módulo.
    UE_LOG(LogIAR, Log, TEXT("Módulo IAR Iniciado"));
    
    // Inicializa a tabela MIDI uma única vez
    UIARMIDITable::InitializeMIDITable();
    // NOTA: Para uso real do FFmpeg ou outras libs externas, garanta que seus
    // binários/DLLs estejam acessíveis em tempo de execução (ex: copiados para a pasta Binaries).
}

void IARModule::ShutdownModule()
{
    // Esta função pode ser chamada durante o desligamento para limpar o módulo.
    // Para módulos que suportam recarregamento dinâmico, chamamos esta função antes de descarregar o módulo.
    UE_LOG(LogIAR, Log, TEXT("Módulo IAR Desligado"));
}

#undef LOCTEXT_NAMESPACE
    
// Implementa o módulo IARModule
IMPLEMENT_MODULE(IARModule, IAR)

