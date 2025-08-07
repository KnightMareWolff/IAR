// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h" // For FPendingLatentAction, FLatentResponse
#include "LatentActions.h"

/**
 * @brief Custom FPendingLatentAction that executes a given lambda function.
 * Mimics FFunctionGraphLatentAction, which may not be publicly available in some UE versions.
 */
class IAR_API FIAR_LambdaLatentAction : public FPendingLatentAction
{
public:
    // Using a TFunction to store the lambda to execute
    using FActionLambda = TFunction<void()>;

    /**
     * @brief Construtor para FIAR_LambdaLatentAction.
     * @param InLambda A fun��o lambda a ser executada quando a a��o latente for atualizada.
     * @param InLatentInfo A FLatentActionInfo original que cont�m ExecutionFunction, Linkage e CallbackTarget.
     */
    FIAR_LambdaLatentAction(const FActionLambda& InLambda, const FLatentActionInfo& InLatentInfo);

    /**
     * @brief O m�todo de atualiza��o principal para a a��o latente.
     * Executa a fun��o lambda e ent�o sinaliza que a a��o est� completa.
     * @param Response A resposta da a��o latente para o gerenciador.
     */
    virtual void UpdateOperation(FLatentResponse& Response) override;

    /**
     * @brief Chamado quando o objeto Blueprint propriet�rio � destru�do antes da a��o completar.
     * Garante que a lambda n�o tente acessar um objeto inv�lido.
     */
    virtual void NotifyObjectDestroyed() override;

private:
    FActionLambda ActionLambda; // A fun��o lambda a ser executada
    FLatentActionInfo LatentInfo; // Armazena a info latente para disparar a conclus�o
};
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Public\Core\IARLambdaLatentAction.h ---
