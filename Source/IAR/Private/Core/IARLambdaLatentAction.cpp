// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#include "IARLambdaLatentAction.h"
// Nenhuma inclus�o adicional necess�ria para esta implementa��o simples.

FIAR_LambdaLatentAction::FIAR_LambdaLatentAction(const FActionLambda& InLambda, const FLatentActionInfo& InLatentInfo)
    : ActionLambda(InLambda)
    , LatentInfo(InLatentInfo) // Inicializa o struct completo
{
    // Construtor, inicializa os membros.
}

void FIAR_LambdaLatentAction::UpdateOperation(FLatentResponse& Response)
{
    // Executa a fun��o lambda se ela for v�lida
    if (ActionLambda)
    {
        ActionLambda();
    }

    // Sinaliza o pino de execu��o do Blueprint original para continuar
    // **CORRIGIDO para UE 5.6: Usando LatentInfo.Linkage e a assinatura correta de TriggerLink**
    Response.TriggerLink(LatentInfo.ExecutionFunction, LatentInfo.Linkage, LatentInfo.CallbackTarget);

    // Marca esta a��o como completa para que seja removida pelo LatentActionManager
    Response.DoneIf(true);
}

void FIAR_LambdaLatentAction::NotifyObjectDestroyed()
{
    // Se o objeto propriet�rio for destru�do, limpa a lambda para evitar refer�ncias penduradas
    ActionLambda = nullptr;
}
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Private\Core\IARLambdaLatentAction.cpp ---
