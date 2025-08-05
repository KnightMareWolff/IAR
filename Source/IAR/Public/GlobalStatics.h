// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HAL/PlatformMisc.h" // Para FPlatformMisc::GetLastError() e GetSystemErrorMessage()
#include "Misc/Paths.h" // ADICIONADO: Para FPaths::ProjectSavedDir

#if WITH_OPENCV
#include "OpenCVHelper.h"
#include "PreOpenCVHeaders.h"

#undef check // the check macro causes problems with opencv headers
#pragma warning(disable: 4668) // 'symbol' not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4828) // The character set in the source file does not support the character used in the literal
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp> // Para cv::VideoCapture
#include <opencv2/imgproc.hpp> // Para cv::cvtColor

#include "PostOpenCVHeaders.h"
#endif

#include "GlobalStatics.generated.h"

// Define uma USTRUCT para encapsular detalhes do erro, tornando-o acessível no Blueprint
USTRUCT(BlueprintType)
struct IAR_API FIAR_SystemErrorDetails
{
    GENERATED_BODY()

    // O código de erro numérico (usando int32 para compatibilidade com Blueprint)
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Error Details")
    int32 ErrorCode = 0; 

    // A descrição textual do erro
    UPROPERTY(BlueprintReadOnly, Category = "IAR|Error Details")
    FString ErrorDescription;
};


/**
 * @brief Biblioteca de Funções Blueprint para utilitários de sistema multiplataforma.
 * Fornece acesso a informações de nível de sistema e tratamento de erros.
 * Multiplataforma: Camada de abstração sobre APIs específicas do SO.
 */
UCLASS()
class IAR_API UIARGlobalStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /**
    * @brief Recupera a descrição do último erro do sistema operacional.
    * Esta função é multiplataforma, utilizando as abstrações da Unreal Engine.
    * @param ErrorCode Opcional. O código de erro a ser traduzido. Se 0, usa o último erro da thread atual.
    * @return Uma USTRUCT IAR_SystemErrorDetails contendo o código de erro e sua descrição.
    */
    UFUNCTION(BlueprintCallable, Category = "IAR System|Error Handling",
              meta = (DisplayName = "Get Last System Error Details",
              ToolTip = "Retrieves the last system error code and its description, multi-platform aware.",
              Keywords = "error, system, last, code, description, platform, iar"))
    static FIAR_SystemErrorDetails GetLastSystemErrorDetails();

    /**
     * @brief Retorna o caminho raiz padrão para arquivos de gravação e mídia.
     * Corresponde a [ProjectSavedDirectory]/Recording/
     * @return O caminho absoluto para o diretório raiz de gravações.
     */
    UFUNCTION(BlueprintPure, Category = "IAR System|Paths",
              meta = (DisplayName = "Get IAR Recording Root Path",
                      Tooltip = "Returns the default root path for IAR recording and media files: [ProjectSavedDirectory]/Recording/"))
    static FString GetIARRecordingRootPath();

};
// --- FIM DO ARQUIVO: D:\william\UnrealProjects\IACSWorld\Plugins\IAR\Source\IAR\Public\GlobalStatics.h ---
