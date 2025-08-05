// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Define a categoria de log para o plugin IAR
DECLARE_LOG_CATEGORY_EXTERN(LogIAR, Log, All);

/**
 * Módulo principal para o plugin Insane Audio Recorder (IAR).
 * Gerencia a inicialização e o desligamento do plugin.
 */
class IAR_API IARModule : public IModuleInterface
{
public:
    /** Implementação da interface IModuleInterface */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
