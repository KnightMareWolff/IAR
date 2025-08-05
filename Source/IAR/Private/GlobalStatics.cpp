// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "GlobalStatics.h"
#include "../IAR.h"

FIAR_SystemErrorDetails UIARGlobalStatics::GetLastSystemErrorDetails()
{
    FIAR_SystemErrorDetails ErrorDetails;

    // 1. Captura o último erro gerado pelo sistema operacional na thread atual
    // FPlatformMisc::GetLastError() retorna uint32, será convertido para int32
    ErrorDetails.ErrorCode = FPlatformMisc::GetLastError(); 

    // 2. Obtém a mensagem de erro do sistema para esse código
    // Define um buffer (ou alocado dinamicamente se a mensagem puder ser muito grande)
    // para FPlatformMisc::GetSystemErrorMessage escrever a mensagem.
    // UE_ARRAY_COUNT é uma macro útil para obter o tamanho de arrays em TCHARs.
    TCHAR ErrorBuffer[1024]; // Um buffer de 1KB deve ser suficiente para a maioria das mensagens de erro

    // Chama GetSystemErrorMessage para preencher o buffer
    FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, UE_ARRAY_COUNT(ErrorBuffer), ErrorDetails.ErrorCode);

    // Converte o conteúdo do buffer para FString
    ErrorDetails.ErrorDescription = ErrorBuffer;

    // Remove quebras de linha/espaços em branco indesejados do final da string
    ErrorDetails.ErrorDescription.TrimEndInline();

    // 3. Lida com casos onde a mensagem de erro pode estar vazia ou não reconhecida
    if (ErrorDetails.ErrorDescription.IsEmpty())
    {
        ErrorDetails.ErrorDescription = FString::Printf(TEXT("Falha ao recuperar mensagem de erro do sistema para o código %d. Este código de erro pode não ser reconhecido pelo sistema ou é genérico."), ErrorDetails.ErrorCode);
    }

    return ErrorDetails;
}

// NOVO: Implementação da função para obter o caminho raiz de gravações
FString UIARGlobalStatics::GetIARRecordingRootPath()
{
    FString RootPath = FPaths::ProjectSavedDir() / TEXT("Recording");
    FPaths::NormalizeDirectoryName(RootPath); // Normaliza o caminho para garantir barras corretas, etc.
    return RootPath;
}
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Private\GlobalStatics.cpp ---
