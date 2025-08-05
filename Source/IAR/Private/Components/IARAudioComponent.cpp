// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#include "Components/IARAudioComponent.h"
#include "../IAR.h"
#include "Recording/IARAudioSimulatedSource.h"
#include "Recording/IARAudioMixerSource.h"
#include "Recording/IARAudioFileSource.h" // Adicionado para AudioFileSource
#include "Recording/IARMIDIFileSource.h" // NOVO: Adicionado para MIDIFileSource
#include "Recording/IARAudioFolderSource.h" // <<-- ADICIONADO
#include "Core/IARFramePool.h"
#include "Recording/IARAudioCaptureSession.h"
#include "Recording/IARECFactory.h"
#include "Recording/IARAudioEncoder.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"
#include "Core/IARSampleRateConverter.h"
#include "Core/IARChannelConverter.h"
#include "AudioCapture.h"
#include "AudioCaptureDeviceInterface.h" 
#include "Kismet/GameplayStatics.h" 
#include "Engine/LatentActionManager.h" // Para FLatentActionManager
#include "LatentActions.h" // Para FPendingLatentAction, FEmptyLatentAction (ainda que não diretamente usado, faz parte do LatentActions)

#include "AudioAnalysis/IARAdvancedAudioFeatureProcessor.h"
#include "AudioAnalysis/IARBasicAudioFeatureProcessor.h" 
#include "AudioAnalysis/IARAudioToMIDITranscriber.h" 
#include "AudioAnalysis/IARMIDIToAudioSynthesizer.h" 
#include "Rendering/Texture2DResource.h"
#include "Engine/World.h"


UIARAudioComponent::UIARAudioComponent()
    : AudioStreamSettings() 
    , RecordingSettings()   
    , bIsActive(false)
    , bIsOverallPipelineInitialized(false) 
    , bIsRecordingCapable(false)           
    , CurrentMediaSource(nullptr) 
    , AudioCaptureSession(nullptr)
    , FramePool(nullptr)
    , SampleRateConverter()
    , FeatureProcessorClass(UIARAdvancedAudioFeatureProcessor::StaticClass()) 
    , FeatureProcessorInstance(nullptr) // Inicializado no construtor abaixo
    , SpectrogramTexture(nullptr) 
    , WaveformTexture(nullptr) 
    , FilteredSpectrogramTexture(nullptr) 
    , MIDITranscriber(nullptr) // Inicializado abaixo
    , SynthesizerInstance(nullptr) // Inicializado no construtor abaixo
    , bEnableMIDISynthesizerOutput(true) // Mantida aqui para controle geral
    , bEnableNoiseGate(false) 
    , NoiseGateThresholdRMS(0.005f)
    , bEnableLowPassFilter(false)
    , LowPassCutoffFrequencyHz(20000.0f)
    , bEnableHighPassFilter(false)
    , HighPassCutoffFrequencyHz(20.0f)
{
    PrimaryComponentTick.bCanEverTick = false;
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Construtor chamado."));

    // Criação de instâncias que devem aparecer no editor (com EditAnywhere e Instanced)
    FeatureProcessorInstance = CreateDefaultSubobject<UIARAdvancedAudioFeatureProcessor>(TEXT("DefaultFeatureProcessor"));
    if (!FeatureProcessorInstance)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar DefaultFeatureProcessor no construtor. As propriedades do processador de features não serão visíveis no editor."));
    }

    SynthesizerInstance = CreateDefaultSubobject<UIARMIDIToAudioSynthesizer>(TEXT("DefaultSynthesizer"));
    if (!SynthesizerInstance)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar DefaultSynthesizer no construtor. As propriedades do sintetizador não serão visíveis no editor."));
    }
}

UIARAudioComponent::~UIARAudioComponent()
{
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Destrutor chamado."));
}

void UIARAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: BeginPlay."));

    EnumerateAudioInputDevices(); 
}

void UIARAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) 
{
    StopRecording();

    if (FeatureProcessorInstance) 
    {
        FeatureProcessorInstance->Shutdown();
    }

    if (MIDITranscriber)
    {
        MIDITranscriber->Shutdown();
        MIDITranscriber = nullptr;
    }

    if (SynthesizerInstance) 
    {
        SynthesizerInstance->Shutdown();
        // SynthesizerInstance = nullptr; // Não precisa zerar, pois é um DefaultSubobject
    }

    if (AudioCaptureSession)
    {
        AudioCaptureSession->ShutdownSession(); 
        AudioCaptureSession = nullptr;
    }

    if (CurrentMediaSource) 
    {
        CurrentMediaSource->Shutdown(); 
        CurrentMediaSource = nullptr; 
    }

    if (FramePool)
    {
        FramePool->ClearPool(); 
        FramePool = nullptr;
    }

    SpectrogramTexture = nullptr;
    WaveformTexture = nullptr; 
    FilteredSpectrogramTexture = nullptr; 

    bIsOverallPipelineInitialized = false; 
    bIsRecordingCapable = false; 

    Super::EndPlay(EndPlayReason); 
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: EndPlay."));
}

void UIARAudioComponent::SetupAudioComponent(const FIAR_AudioStreamSettings& StreamSettings)
{
    AudioStreamSettings = StreamSettings;
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: SetupAudioComponent chamado. SampleRate: %d, Canais: %d, Tipo de Fonte: %s"),
           AudioStreamSettings.SampleRate, AudioStreamSettings.NumChannels, *UEnum::GetValueAsString(AudioStreamSettings.SourceType));
}

// Implementação do StartRecording como uma função Latente
void UIARAudioComponent::StartRecording(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& CustomRecordingName)
{
    // Usaremos FIAR_LambdaLatentAction para concluir imediatamente, pois FEmptyLatentAction não existe mais
    auto FinishLatentActionImmediately = [WorldContextObject, LatentInfo]()
    {
        FLatentActionManager& LatentManager = WorldContextObject->GetWorld()->GetLatentActionManager();
        LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FIAR_LambdaLatentAction([](){}, LatentInfo));
    };

    if (bIsActive) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Já ativo (gravando ou em modo RT). Ignorando StartRecording."));
        FinishLatentActionImmediately(); // Completa a ação latente imediatamente
        return;
    }

    // Inicializa o pipeline de áudio (esta parte é síncrona, mas rápida)
    InitializeAudioPipelineInternal();

    if (!bIsOverallPipelineInitialized) 
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao inicializar o pipeline de áudio. Não é possível iniciar a gravação/captura."));
        FinishLatentActionImmediately(); // Falhou, completa a ação latente imediatamente.
        return;
    }

    // Define o nome da sessão global aqui, para que esteja disponível para a FLoadAudioFileLatentAction
    CurrentOverallSessionName = CustomRecordingName.IsEmpty() ? FString::Printf(TEXT("Session_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))) : CustomRecordingName;


    // =============================================================================================
    // Lógica para carregamento de arquivo de áudio assíncrono (se a fonte for UIARAudioFileSource)
    // =============================================================================================
    if (AudioStreamSettings.SourceType == EIARAudioSourceType::AudioFile)
    {
        UIARAudioFileSource* AudioFileSource = Cast<UIARAudioFileSource>(CurrentMediaSource);
        if (AudioFileSource && !AudioFileSource->IsFileLoaded())
        {
            // O carregamento do arquivo ainda não foi feito. Vamos fazê-lo de forma assíncrona.
            // Criamos uma ação latente personalizada que executará o carregamento em background
            // e continuará a execução quando o carregamento for concluído.
            class FLoadAudioFileLatentAction : public FPendingLatentAction
            {
            public:
                // Armazena a FLatentActionInfo completa
                FLatentActionInfo LatentInfoRef; 

                UIARAudioFileSource* SourceToLoad;
                FString DebugFilePath; // Para logs de debug
                TWeakObjectPtr<UIARAudioComponent> OwningAudioComponentPtr; // Referência fraca ao componente pai

                // Flags para gerenciar o estado assíncrono
                bool bAsyncTaskStarted = false;
                bool bAsyncTaskCompleted = false;
                bool bAsyncTaskSuccess = false;

                // Construtor que aceita a FLatentActionInfo e o OwningAudioComponent
                FLoadAudioFileLatentAction(UIARAudioFileSource* InSourceToLoad, const FString& InDebugFilePath, const FLatentActionInfo& InLatentInfo, UIARAudioComponent* InOwningAudioComponent)
                    : LatentInfoRef(InLatentInfo)
                    , SourceToLoad(InSourceToLoad)
                    , DebugFilePath(InDebugFilePath)
                    , OwningAudioComponentPtr(InOwningAudioComponent) // Inicializa a weak pointer
                {
                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: FLoadAudioFileLatentAction criado para arquivo: %s"), *DebugFilePath);
                }

                virtual void UpdateOperation(FLatentResponse& Response) override
                {
                    // 1. Se a tarefa assíncrona ainda não foi iniciada, iniciá-la
                    if (!bAsyncTaskStarted)
                    {
                        bAsyncTaskStarted = true;
                        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Iniciando carregamento de arquivo assíncrono para: %s"), *DebugFilePath);
                        
                        // Lançar o AsyncTask para o carregamento bloqueante em uma thread de background
                        // Captura 'this' para acessar membros da instância FLoadAudioFileLatentAction
                        AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, WeakSourceToLoad = MakeWeakObjectPtr(SourceToLoad)]()
                        {
                            bool bLocalLoadSuccess = false;
                            if (WeakSourceToLoad.IsValid())
                            {
                                bLocalLoadSuccess = WeakSourceToLoad->Internal_LoadAudioFileBlocking();
                            }
                            
                            // Após a conclusão do carregamento em background, sinalize a conclusão na Game Thread
                            AsyncTask(ENamedThreads::GameThread, [this, bLocalLoadSuccess]() 
                            {
                                this->bAsyncTaskCompleted = true;
                                this->bAsyncTaskSuccess = bLocalLoadSuccess;
                                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Sinalizando conclusão do AsyncTask na Game Thread. Sucesso: %s"), bLocalLoadSuccess ? TEXT("true") : TEXT("false"));
                            });
                        });
                    }

                    // 2. Uma vez que a tarefa assíncrona sinaliza sua conclusão na Game Thread,
                    // esta chamada de UpdateOperation irá disparar a conclusão da ação latente.
                    if (bAsyncTaskCompleted)
                    {
                        if (bAsyncTaskSuccess)
                        {
                            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Carregamento de arquivo assíncrono concluído com sucesso: %s"), *DebugFilePath);

                            // --- INÍCIO DA LÓGICA DE CONTINUAÇÃO (MOVIDA PARA DENTRO DA AÇÃO LATENTE) ---
                            // Agora que o arquivo foi carregado, podemos iniciar a captura real e marcar o componente como ativo.
                            UIARAudioComponent* OwningComponent = OwningAudioComponentPtr.Get();
                            if (OwningComponent)
                            {
                                OwningComponent->bIsActive = true; // Marca o componente como ativo AQUI

                                if (OwningComponent->AudioStreamSettings.bEnableRTFeatures)
                                {
                                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Modo Real-Time Features (RT) habilitado. Nenhuma gravação em arquivo será iniciada."));
                                    if (OwningComponent->CurrentMediaSource)
                                    {
                                        OwningComponent->CurrentMediaSource->StartCapture();
                                        OwningComponent->OnRecordingStarted.Broadcast();
                                    }
                                    else
                                    {
                                        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: CurrentMediaSource é nulo na ação latente. Não é possível iniciar a captura em modo RT."));
                                        OwningComponent->bIsActive = false;
                                    }
                                }
                                else // Modo de Gravação em Arquivo
                                {
                                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Modo de Gravação em Arquivo habilitado."));
                                    if (OwningComponent->bIsRecordingCapable && OwningComponent->AudioCaptureSession)
                                    {
                                        // O nome da sessão já foi definido no início da StartRecording e está em OwningComponent->CurrentOverallSessionName
                                        OwningComponent->AudioCaptureSession->StartOverallRecording(OwningComponent->CurrentMediaSource->GetCurrentStreamSettings(), OwningComponent->CurrentOverallSessionName);
                                        
                                        if (OwningComponent->CurrentMediaSource)
                                        {
                                            OwningComponent->CurrentMediaSource->StartCapture();
                                        }
                                        OwningComponent->OnRecordingStarted.Broadcast();
                                    }
                                    else
                                    {
                                        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Não é possível gravar em arquivo (AudioCaptureSession não inicializado ou fonte não é compatível com áudio). Verifique o tipo de fonte e os logs de inicialização."));
                                        OwningComponent->bIsActive = false;
                                    }
                                }
                            }
                            else
                            {
                                UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: OwningAudioComponent inválido na ação latente após carregamento do arquivo."));
                            }
                            // --- FIM DA LÓGICA DE CONTINUAÇÃO ---
                        }
                        else // Se o carregamento assíncrono falhou
                        {
                            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha no carregamento de arquivo assíncrono: %s"), *DebugFilePath);
                            // O componente pai já foi marcado como falha ou não iniciado, não é necessário ação adicional aqui.
                        }
                        
                        Response.TriggerLink(LatentInfoRef.ExecutionFunction, LatentInfoRef.Linkage, LatentInfoRef.CallbackTarget);
                        Response.DoneIf(true); // Marca como concluído
                    }
                    else
                    {
                        // Ainda esperando pela tarefa assíncrona para completar.
                        Response.DoneIf(false); 
                    }
                }

                virtual void NotifyObjectDestroyed() override
                {
                    // Limpa quaisquer ponteiros ou tarefas pendentes se o objeto for destruído prematuramente
                    SourceToLoad = nullptr;
                    OwningAudioComponentPtr.Reset(); // Reseta a weak pointer
                }
            };

            // Adiciona a ação latente para carregar o arquivo, passando 'this' como o componente pai
            FLatentActionManager& LatentManager = WorldContextObject->GetWorld()->GetLatentActionManager();
            LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FLoadAudioFileLatentAction(AudioFileSource, AudioFileSource->FullDiskFilePathInternal, LatentInfo, this));
            // RETORNA AQUI. A execução do Blueprint será pausada até que FLoadAudioFileLatentAction seja concluída.
            return; 
        }
    }
    // NOVO: Lógica para carregamento de arquivo MIDI assíncrono (se a fonte for UIARMIDIFileSource)
    else if (AudioStreamSettings.SourceType == EIARAudioSourceType::MIDIFile)
    {
        UIARMIDIFileSource* MIDIFileSource = Cast<UIARMIDIFileSource>(CurrentMediaSource);
        if (MIDIFileSource && !MIDIFileSource->IsFileLoaded())
        {
            // O carregamento do arquivo MIDI ainda não foi feito. Vamos fazê-lo de forma assíncrona.
            class FLoadMIDIFileLatentAction : public FPendingLatentAction
            {
            public:
                FLatentActionInfo LatentInfoRef; 
                UIARMIDIFileSource* SourceToLoad;
                FString DebugFilePath; 
                TWeakObjectPtr<UIARAudioComponent> OwningAudioComponentPtr; 

                bool bAsyncTaskStarted = false;
                bool bAsyncTaskCompleted = false;
                bool bAsyncTaskSuccess = false;

                FLoadMIDIFileLatentAction(UIARMIDIFileSource* InSourceToLoad, const FString& InDebugFilePath, const FLatentActionInfo& InLatentInfo, UIARAudioComponent* InOwningAudioComponent)
                    : LatentInfoRef(InLatentInfo)
                    , SourceToLoad(InSourceToLoad)
                    , DebugFilePath(InDebugFilePath)
                    , OwningAudioComponentPtr(InOwningAudioComponent)
                {
                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: FLoadMIDIFileLatentAction criado para arquivo MIDI: %s"), *DebugFilePath);
                }

                virtual void UpdateOperation(FLatentResponse& Response) override
                {
                    if (!bAsyncTaskStarted)
                    {
                        bAsyncTaskStarted = true;
                        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Iniciando carregamento de arquivo MIDI assíncrono para: %s"), *DebugFilePath);
                        
                        AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, WeakSourceToLoad = MakeWeakObjectPtr(SourceToLoad)]()
                        {
                            bool bLocalLoadSuccess = false;
                            if (WeakSourceToLoad.IsValid())
                            {
                                bLocalLoadSuccess = WeakSourceToLoad->Internal_LoadMIDIFileBlocking();
                            }
                            AsyncTask(ENamedThreads::GameThread, [this, bLocalLoadSuccess]() 
                            {
                                this->bAsyncTaskCompleted = true;
                                this->bAsyncTaskSuccess = bLocalLoadSuccess;
                                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Sinalizando conclusão do AsyncTask MIDI na Game Thread. Sucesso: %s"), bLocalLoadSuccess ? TEXT("true") : TEXT("false"));
                            });
                        });
                    }

                    if (bAsyncTaskCompleted)
                    {
                        if (bAsyncTaskSuccess)
                        {
                            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Carregamento de arquivo MIDI assíncrono concluído com sucesso: %s"), *DebugFilePath);
                            UIARAudioComponent* OwningComponent = OwningAudioComponentPtr.Get();
                            if (OwningComponent)
                            {
                                OwningComponent->bIsActive = true; 
                                if (OwningComponent->CurrentMediaSource)
                                {
                                    OwningComponent->CurrentMediaSource->StartCapture();
                                    OwningComponent->OnRecordingStarted.Broadcast();
                                }
                                else
                                {
                                    UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: CurrentMediaSource é nulo na ação latente MIDI. Não é possível iniciar a captura."));
                                    OwningComponent->bIsActive = false;
                                }
                            }
                            else
                            {
                                UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: OwningAudioComponent inválido na ação latente após carregamento do arquivo MIDI."));
                            }
                        }
                        else 
                        {
                            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha no carregamento de arquivo MIDI assíncrono: %s"), *DebugFilePath);
                        }
                        Response.TriggerLink(LatentInfoRef.ExecutionFunction, LatentInfoRef.Linkage, LatentInfoRef.CallbackTarget);
                        Response.DoneIf(true); 
                    }
                    else
                    {
                        Response.DoneIf(false); 
                    }
                }

                virtual void NotifyObjectDestroyed() override
                {
                    SourceToLoad = nullptr;
                    OwningAudioComponentPtr.Reset(); 
                }
            };
            FLatentActionManager& LatentManager = WorldContextObject->GetWorld()->GetLatentActionManager();
            LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FLoadMIDIFileLatentAction(MIDIFileSource, MIDIFileSource->FullDiskFilePathInternal, LatentInfo, this));
            return;
        }
    }
    // --- Lógica para EIARAudioSourceType::Folder (Processamento em Lote) ---
    else if (AudioStreamSettings.SourceType == EIARAudioSourceType::Folder)
    {
        UIARFolderSource* FolderSource = Cast<UIARFolderSource>(CurrentMediaSource);
        if (FolderSource)
        {
            // O FolderSource gerencia o processamento assíncrono internamente.
            // StartCapture nele aciona o processo de leitura, conversão e salvamento.
            FolderSource->StartCapture(); 
            bIsActive = true; // O componente agora está "ativo" no modo de processamento
            OnRecordingStarted.Broadcast(); // Sinaliza para o Blueprint que o processo de lote começou
            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Processamento de diretório iniciado."));
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: CurrentMediaSource não é um UIARFolderSource válido para o tipo Folder."));
            bIsActive = false;
        }
        FinishLatentActionImmediately(); // Para FolderSource, a ação latente é concluída imediatamente
        return;
    }
    // --- Fim da Lógica para EIARAudioSourceType::Folder ---


    // =============================================================================================
    // Continuação da lógica de StartRecording (executada APENAS se não houver carregamento assíncrono de arquivo
    // ou se o arquivo já estiver carregado)
    // =============================================================================================

    bIsActive = true; // Marca o componente como ativo para os casos síncronos

    if (AudioStreamSettings.bEnableRTFeatures)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Modo Real-Time Features (RT) habilitado. Nenhuma gravação em arquivo será iniciada."));
        if (CurrentMediaSource) 
        {
            CurrentMediaSource->StartCapture(); 
            OnRecordingStarted.Broadcast(); 
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: CurrentMediaSource é nulo. Não é possível iniciar a captura em modo RT.")); 
            bIsActive = false; 
        }
    }
    else 
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Modo de Gravação em Arquivo habilitado."));
        // A gravação em arquivo só é possível para fontes de áudio, não MIDI
        if (bIsRecordingCapable && AudioCaptureSession && AudioStreamSettings.ContentType == EIARMediaContentType::Audio) 
        {
            // O nome da sessão já foi definido no início da StartRecording e está em OwningComponent->CurrentOverallSessionName
            AudioCaptureSession->StartOverallRecording(CurrentMediaSource->GetCurrentStreamSettings(), CurrentOverallSessionName); 
            
            if (CurrentMediaSource) 
            {
                CurrentMediaSource->StartCapture();
            }
            OnRecordingStarted.Broadcast(); 
        }
        else if (AudioStreamSettings.ContentType == EIARMediaContentType::MIDI) // Fonte MIDI em modo não-RT Features, apenas iniciar captura
        {
             UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte MIDI em modo não-RT Features (apenas reprodução)."));
             if (CurrentMediaSource)
             {
                 CurrentMediaSource->StartCapture();
                 OnRecordingStarted.Broadcast();
             }
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Não é possível gravar em arquivo (AudioCaptureSession não inicializado ou fonte não é compatível com áudio). Verifique o tipo de fonte e os logs de inicialização."));
            bIsActive = false; 
        }
    }

    // Completa a ação latente imediatamente para os casos síncronos (não AudioFile, MIDIFile ou Folder, ou já carregado).
    FinishLatentActionImmediately();
}

void UIARAudioComponent::StopRecording()
{
    if (!bIsActive) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Não está ativo (gravando ou em modo RT). Ignorando StopRecording."));
        return;
    }

    if (AudioStreamSettings.SourceType == EIARAudioSourceType::Folder)
    {
        UIARFolderSource* FolderSource = Cast<UIARFolderSource>(CurrentMediaSource);
        if (FolderSource)
        {
            FolderSource->StopCapture(); // Sinaliza a interrupção do processamento em lote
            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Interrompendo processamento de diretório."));
            // O delegate OnRecordingStopped.Broadcast() será disparado pela UIARFolderSource quando o processamento terminar.
        }
        else
        {
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: CurrentMediaSource não é um UIARFolderSource válido para o tipo Folder."));
        }
        // Não defina bIsActive = false aqui. A UIARFolderSource fará isso ao finalizar.
    }
    else if (AudioStreamSettings.bEnableRTFeatures)
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Parando modo Real-Time Features (RT)."));
        if (CurrentMediaSource) 
        {
            CurrentMediaSource->StopCapture(); 
            OnRecordingStopped.Broadcast(); 
        }
    }
    else 
    {
        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Parando modo de Gravação em Arquivo."));
        if (AudioStreamSettings.ContentType == EIARMediaContentType::Audio && AudioCaptureSession) // Apenas fontes de áudio podem ter AudioCaptureSession
        {
            AudioCaptureSession->StopOverallRecording();
            OnRecordingStopped.Broadcast(); 
        }
        else if (AudioStreamSettings.ContentType == EIARMediaContentType::MIDI && CurrentMediaSource) // Fontes MIDI
        {
            CurrentMediaSource->StopCapture();
            OnRecordingStopped.Broadcast();
        }
    }
    bIsActive = false; // Mover essa linha para dentro dos IFs/ELSE IFs ou tratar de forma diferente
}

// Renomeado para evitar confusão com a nova lógica latente do StartRecording
void UIARAudioComponent::InitializeAudioPipelineInternal()
{
    if (bIsOverallPipelineInitialized) 
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: O pipeline de áudio já foi inicializado. Ignorando chamada."));
        return;
    }

    bIsOverallPipelineInitialized = false; 
    bIsRecordingCapable = false; 

    // O FeatureProcessorInstance já é um DefaultSubobject
    if (FeatureProcessorInstance) 
    {
        // Se a classe FeatureProcessorClass foi alterada no editor, recriamos a instância
        if (FeatureProcessorInstance->GetClass() != FeatureProcessorClass.Get())
        {
            FeatureProcessorInstance->Shutdown(); 
            FeatureProcessorInstance = NewObject<UIARFeatureProcessor>(this, FeatureProcessorClass.Get(), TEXT("CustomFeatureProcessorInstance")); // Cria uma nova instância com a classe selecionada
            if (!FeatureProcessorInstance)
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar FeatureProcessor do tipo %s. Não é possível inicializar o pipeline."), *FeatureProcessorClass->GetName());
                return; 
            }
        }
        FeatureProcessorInstance->Initialize(); // Inicializa a instância existente ou recém-criada
        UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: FeatureProcessor (Tipo: %s) re-inicializado."), *FeatureProcessorClass->GetName());
    }
    else 
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: FeatureProcessorInstance é nulo. Verifique a configuração do DefaultSubobject no construtor. Não é possível inicializar o pipeline."));
        return; 
    }


    if (!MIDITranscriber) // MIDITranscriber ainda não é um DefaultSubobject
    {
        MIDITranscriber = NewObject<UIARAudioToMIDITranscriber>(this);
        if (MIDITranscriber)
        {
            MIDITranscriber->Initialize(AudioStreamSettings.SampleRate); 
            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: MIDITranscriber inicializado."));
            // O SynthesizerInstance já é um DefaultSubobject criado no construtor
            if (SynthesizerInstance)
            {
                SynthesizerInstance->Initialize(AudioStreamSettings.SampleRate, AudioStreamSettings.NumChannels);
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: UIARMIDIToAudioSynthesizer inicializado."));
                
                // Conecte o delegate do transcritor ao método do sintetizador
                MIDITranscriber->OnMIDITranscriptionEventGenerated.AddUObject(SynthesizerInstance, &UIARMIDIToAudioSynthesizer::ProcessMIDIEvent);
                
                // NOVO: Controle a reprodução do sintetizador com base na nova propriedade
                if (bEnableMIDISynthesizerOutput)
                {
                    SynthesizerInstance->StartPlayback();
                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Saída de áudio do sintetizador MIDI ATIVADA."));
                }
                else
                {
                    SynthesizerInstance->StopPlayback();
                    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Saída de áudio do sintetizador MIDI DESATIVADA."));
                }
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARMIDIToAudioSynthesizer."));
            }

        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARAudioToMIDITranscriber."));
            return;
        }
    }
    
    if (CurrentMediaSource) 
    {
        CurrentMediaSource->StopCapture(); 
        CurrentMediaSource->Shutdown(); 
        CurrentMediaSource = nullptr; 
    }
    
    if (!FramePool)
    {
        FramePool = NewObject<UIARFramePool>(this);
        if (FramePool)
        {
            int32 DesiredPoolSize = 120;
            int32 DefaultSampleRate = AudioStreamSettings.SampleRate;
            int32 DefaultNumChannels = AudioStreamSettings.NumChannels;
            int32 DefaultFrameBufferSizeInSamples = 4096;
            
            FramePool->InitializePool(DesiredPoolSize, DefaultSampleRate, DefaultNumChannels, DefaultFrameBufferSizeInSamples);
            UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: FramePool criado e inicializado (SR: %d, Ch: %d, Buf: %d). PoolSize: %d"), DefaultSampleRate, DefaultNumChannels, DefaultFrameBufferSizeInSamples, DesiredPoolSize);
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARFramePool. Não é possível inicializar o pipeline."));
            return;
        }
    }

    switch (AudioStreamSettings.SourceType)
    {
        case EIARAudioSourceType::Simulated:
        {
            CurrentMediaSource = NewObject<UIARAudioSimulatedSource>(this); 
            if (CurrentMediaSource)
            {
                CurrentMediaSource->Initialize(AudioStreamSettings, FramePool); 
                CurrentMediaSource->OnAudioFrameAcquired.AddUObject(this, &UIARAudioComponent::OnAudioFrameAcquired); 
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte de áudio simulada configurada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARAudioSimulatedSource."));
            }
            break;
        }
        case EIARAudioSourceType::AudioMixer:
        {
            CurrentMediaSource = NewObject<UIARAudioMixerSource>(this); 
            if (CurrentMediaSource)
            {
                CurrentMediaSource->Initialize(AudioStreamSettings, FramePool);
                CurrentMediaSource->OnAudioFrameAcquired.AddUObject(this, &UIARAudioComponent::OnAudioFrameAcquired);
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte de áudio do Audio Mixer configurada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARAudioMixerSource para Audio Mixer."));
            }
            break;
        }
        case EIARAudioSourceType::AudioFile: 
        {
            CurrentMediaSource = NewObject<UIARAudioFileSource>(this);
            if (CurrentMediaSource)
            {
                // NOVO: Resetar a fonte de arquivo para garantir um estado limpo antes da inicialização.
                Cast<UIARAudioFileSource>(CurrentMediaSource)->ResetFileSource();
                CurrentMediaSource->Initialize(AudioStreamSettings, FramePool);
                CurrentMediaSource->OnAudioFrameAcquired.AddUObject(this, &UIARAudioComponent::OnAudioFrameAcquired);
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte de áudio de arquivo configurada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARAudioFileSource."));
            }
            break;
        }
        // NOVO: Case para UIARMIDIFileSource
        case EIARAudioSourceType::MIDIFile:
        {
            CurrentMediaSource = NewObject<UIARMIDIFileSource>(this);
            if (CurrentMediaSource)
            {
                Cast<UIARMIDIFileSource>(CurrentMediaSource)->ResetFileSource(); // Limpa estado anterior
                CurrentMediaSource->Initialize(AudioStreamSettings, FramePool);
                // Conecta o delegate de MIDI frames para processamento no AudioComponent
                CurrentMediaSource->OnMIDIFrameAcquired.AddUObject(this, &UIARAudioComponent::OnMIDIFrameAcquired);
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte de arquivo MIDI configurada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARMIDIFileSource."));
            }
            break;
        }
        // <<-- NOVO CASE ADICIONADO PARA UIARFolderSource -->>
        case EIARAudioSourceType::Folder:
        {
            CurrentMediaSource = NewObject<UIARFolderSource>(this);
            if (CurrentMediaSource)
            {
                // Inicializa a UIARFolderSource com as configurações e FramePool
                // A UIARFolderSource gerencia seu próprio carregamento de arquivo e conversão internamente.
                // Ela não transmite frames de áudio ou MIDI em tempo real no mesmo sentido das outras fontes.
                CurrentMediaSource->Initialize(AudioStreamSettings, FramePool);

                // Conecta os delegates da UIARFolderSource para que o UIARAudioComponent
                // possa retransmitir ou processar os eventos (como logar ou atualizar a UI).
                UIARFolderSource* FolderSource = Cast<UIARFolderSource>(CurrentMediaSource);
                if (FolderSource)
                {
                    FolderSource->OnFolderProcessingCompleted.AddDynamic(this, &UIARAudioComponent::HandleFolderProcessingCompleted); // CORRIGIDO
                    FolderSource->OnFolderProcessingError.AddDynamic(this, &UIARAudioComponent::HandleFolderProcessingError);     // CORRIGIDO
                    FolderSource->OnFolderProcessingProgress.AddDynamic(this, &UIARAudioComponent::HandleFolderProcessingProgress); // CORRIGIDO
                }
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Fonte de mídia de diretório configurada."));
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar UIARFolderSource. Não é possível inicializar o pipeline."));
                return; // Falha na inicialização do pipeline
            }
            break;
        }

        case EIARAudioSourceType::MIDIInput: // Manteve o original pois o Folder foi adicionado antes dele no enum
        default:
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Tipo de fonte de mídia não suportado ou não implementado: %s."), *UEnum::GetValueAsString(AudioStreamSettings.SourceType));
            CurrentMediaSource = nullptr; 
            return; 
    }

    if (CurrentMediaSource) 
    {
        bIsOverallPipelineInitialized = true;
        
        UIARAudioSource* AudioSource = Cast<UIARAudioSource>(CurrentMediaSource);
        
        if (AudioSource && (CurrentMediaSource->GetCurrentStreamSettings().ContentType == EIARMediaContentType::Audio || CurrentMediaSource->GetCurrentStreamSettings().ContentType == EIARMediaContentType::AutoDetect))
        {
            if (!AudioCaptureSession)
            {
                AudioCaptureSession = NewObject<UIARAudioCaptureSession>(this);
            }
            if (AudioCaptureSession) 
            {
                AudioCaptureSession->InitializeSession(AudioSource, RecordingSettings, FramePool); 
                UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: AudioCaptureSession inicializado com uma fonte de audio. Capaz de gravar."));
                bIsRecordingCapable = true; 
            }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha final na inicialização do pipeline de audio (AudioCaptureSession nulo). A gravação não será possível."));
            }
        }
        else 
        {
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: CurrentMediaSource não é uma fonte de áudio ou o tipo de conteúdo não é áudio. A funcionalidade de gravação em arquivo não será inicializada."));
        }
    }
    else 
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao criar qualquer fonte de mídia. Pipeline não inicializado em absoluto."));
        bIsOverallPipelineInitialized = false; 
        bIsRecordingCapable = false;
    }
}

void UIARAudioComponent::OnAudioFrameAcquired(TSharedPtr<FIAR_AudioFrameData> AudioFrame)
{
    if (!AudioFrame.IsValid() || !AudioFrame->RawSamplesPtr.IsValid() || AudioFrame->RawSamplesPtr->Num() == 0)
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Frame de áudio inválido ou vazio recebido."));
        if (FramePool) { FramePool->ReleaseFrame(AudioFrame); } 
        return;
    }

    TSharedPtr<FIAR_AudioFrameData> CurrentProcessedFrame = AudioFrame; 

    if (CurrentProcessedFrame->NumChannels != AudioStreamSettings.NumChannels)
    {
        TArray<float> ConvertedChannelsSamples;
        if (FIARChannelConverter::Convert(*(CurrentProcessedFrame->RawSamplesPtr), CurrentProcessedFrame->NumChannels, ConvertedChannelsSamples, AudioStreamSettings.NumChannels))
        {
            TSharedPtr<FIAR_AudioFrameData> NewFrameForConversion = FramePool->AcquireFrame();
            if (!NewFrameForConversion.IsValid()) 
            { 
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao adquirir frame para conversão de canais.")); 
                if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
                return; 
            }

            *(NewFrameForConversion->RawSamplesPtr) = MoveTemp(ConvertedChannelsSamples);
            NewFrameForConversion->SampleRate = CurrentProcessedFrame->SampleRate;
            NewFrameForConversion->NumChannels = AudioStreamSettings.NumChannels; 
            NewFrameForConversion->Timestamp = CurrentProcessedFrame->Timestamp;
            NewFrameForConversion->CurrentStreamSettings = AudioStreamSettings; 

            if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
            CurrentProcessedFrame = NewFrameForConversion; 
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao converter número de canais do frame."));
            if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
            return;
        }
    }

    const int32 DesiredOutputSampleRate = AudioStreamSettings.SampleRate;
    if (AudioStreamSettings.bEnableResampling && CurrentProcessedFrame->SampleRate != DesiredOutputSampleRate)
    {
        if (SampleRateConverter.GetInputSampleRate() != CurrentProcessedFrame->SampleRate || 
            SampleRateConverter.GetOutputSampleRate() != DesiredOutputSampleRate ||
            SampleRateConverter.GetOutputNumChannels() != CurrentProcessedFrame->NumChannels) 
        {
            if (!SampleRateConverter.Initialize(CurrentProcessedFrame->SampleRate, DesiredOutputSampleRate, CurrentProcessedFrame->NumChannels)) 
            {
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao inicializar Sample Rate Converter."));
                if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
                return;
            }
        }
        
        TArray<float> ResampledSamples;
        if (SampleRateConverter.Convert(*(CurrentProcessedFrame->RawSamplesPtr), ResampledSamples))
        {
            TSharedPtr<FIAR_AudioFrameData> NewFrameForResampling = FramePool->AcquireFrame();
            if (!NewFrameForResampling.IsValid()) 
            { 
                UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao adquirir frame para resampling.")); 
                if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
                return; 
            }

            *(NewFrameForResampling->RawSamplesPtr) = MoveTemp(ResampledSamples);
            NewFrameForResampling->SampleRate = DesiredOutputSampleRate;
            NewFrameForResampling->NumChannels = CurrentProcessedFrame->NumChannels; 
            NewFrameForResampling->Timestamp = CurrentProcessedFrame->Timestamp;
            NewFrameForResampling->CurrentStreamSettings = AudioStreamSettings; 

            if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
            CurrentProcessedFrame = NewFrameForResampling; 
        }
        else
        {
            UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Falha ao converter Sample Rate do frame."));
            if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
            return;
        }
    }

    if (FeatureProcessorInstance) 
    {
        if (bEnableNoiseGate)
        {
            FeatureProcessorInstance->ApplyNoiseGate(*(CurrentProcessedFrame->RawSamplesPtr), NoiseGateThresholdRMS, 0.0f, 0.0f, CurrentProcessedFrame->SampleRate);
        }
        if (bEnableLowPassFilter)
        {
            FeatureProcessorInstance->ApplyLowPassFilter(*(CurrentProcessedFrame->RawSamplesPtr), LowPassCutoffFrequencyHz, CurrentProcessedFrame->SampleRate, CurrentProcessedFrame->NumChannels);
        }
        if (bEnableHighPassFilter)
        {
            FeatureProcessorInstance->ApplyHighPassFilter(*(CurrentProcessedFrame->RawSamplesPtr), HighPassCutoffFrequencyHz, CurrentProcessedFrame->SampleRate, CurrentProcessedFrame->NumChannels);
        }
    }


    if (AudioStreamSettings.bEnableRTFeatures)
    {
        FIAR_JustRTFrame RTFrame;
        UTexture2D* DummySpectrogramTexture = nullptr; 
        if (FeatureProcessorInstance) 
        {
            if (CurrentProcessedFrame->RawSamplesPtr.IsValid())
            {
                RTFrame.RawAudioBuffer = *(CurrentProcessedFrame->RawSamplesPtr); 
            }
            RTFrame.SampleRate = CurrentProcessedFrame->SampleRate;
            RTFrame.NumChannels = CurrentProcessedFrame->NumChannels;
            RTFrame.Timestamp = CurrentProcessedFrame->Timestamp;

            RTFrame.Features = FeatureProcessorInstance->ProcessFrame(CurrentProcessedFrame, DummySpectrogramTexture); 
            if (MIDITranscriber)
            {
                float CurrentFrameDuration = (float)CurrentProcessedFrame->RawSamplesPtr->Num() / (float)CurrentProcessedFrame->SampleRate / (float)CurrentProcessedFrame->NumChannels;
                MIDITranscriber->ProcessAudioFeatures(RTFrame.Features, RTFrame.Timestamp, CurrentFrameDuration);
            }

            if (AudioStreamSettings.bDebugDrawFeatures) 
            {
                int32 SpectrogramPixelsWidth = 0, SpectrogramPixelsHeight = 0; 
                UIARAdvancedAudioFeatureProcessor* AdvancedProcessor = Cast<UIARAdvancedAudioFeatureProcessor>(FeatureProcessorInstance);
                
                const TArray<FColor>* SpectrogramPixelsPtr = nullptr;
                if (AdvancedProcessor) 
                {
                    SpectrogramPixelsPtr = &AdvancedProcessor->GetSpectrogramPixels(SpectrogramPixelsWidth, SpectrogramPixelsHeight);
                }
                
                int32 WaveformPixelsWidth = 0, WaveformPixelsHeight = 0; 
                const TArray<FColor>* WaveformPixelsPtr = nullptr;
                if (AdvancedProcessor) 
                {
                    WaveformPixelsPtr = &AdvancedProcessor->GetWaveformPixels(WaveformPixelsWidth, WaveformPixelsHeight);
                }
                
                int32 FilteredSpectrogramPixelsWidth = 0, FilteredSpectrogramPixelsHeight = 0;
                const TArray<FColor>* FilteredSpectrogramPixelsPtr = nullptr;

                if (AdvancedProcessor && AdvancedProcessor->bEnableContextualFrequencyFiltering) 
                {
                    FilteredSpectrogramPixelsPtr = &AdvancedProcessor->GetFilteredSpectrogramPixels(FilteredSpectrogramPixelsWidth, FilteredSpectrogramPixelsHeight);
                }

                AsyncTask(ENamedThreads::GameThread, [this, RTFrame, SpectrogramPixelsPtr, SpectrogramPixelsWidth, SpectrogramPixelsHeight, WaveformPixelsPtr, WaveformPixelsWidth, WaveformPixelsHeight,
                                                        FilteredSpectrogramPixelsPtr, FilteredSpectrogramPixelsWidth, FilteredSpectrogramPixelsHeight]() mutable {
                    if (SpectrogramPixelsPtr && SpectrogramPixelsWidth > 0 && SpectrogramPixelsHeight > 0)
                    {
                        if (!SpectrogramTexture || SpectrogramTexture->GetSizeX() != SpectrogramPixelsWidth || SpectrogramTexture->GetSizeY() != SpectrogramPixelsHeight)
                        {
                            if (SpectrogramTexture) { SpectrogramTexture = nullptr; } 
                            SpectrogramTexture = UTexture2D::CreateTransient(SpectrogramPixelsWidth, SpectrogramPixelsHeight, PF_B8G8R8A8);
                            SpectrogramTexture->bNoTiling = false;
                            SpectrogramTexture->CompressionSettings = TC_EditorIcon;
                            SpectrogramTexture->SRGB = false;
                            SpectrogramTexture->Filter = TF_Nearest;
                            SpectrogramTexture->AddressX = TA_Clamp;
                            SpectrogramTexture->AddressY = TA_Clamp;
                            SpectrogramTexture->UpdateResource(); 
                        }
                        
                        if (SpectrogramTexture && SpectrogramTexture->GetResource())
                        {
                            FTexture2DResource* TextureResource = static_cast<FTexture2DResource*>(SpectrogramTexture->GetResource()->GetTexture2DResource());
                            if (TextureResource)
                            {
                                ENQUEUE_RENDER_COMMAND(UpdateSpectrogramTexture)([TextureResource, SpectrogramPixels = *SpectrogramPixelsPtr](FRHICommandListImmediate& RHICmdList)
                                {
                                    if (TextureResource->GetTexture2DRHI())
                                    {
                                        uint32 Stride = 0;
                                        void* LockedData = RHICmdList.LockTexture2D(TextureResource->GetTexture2DRHI(), 0, EResourceLockMode::RLM_WriteOnly, Stride, false);
                                        FMemory::Memcpy(LockedData, SpectrogramPixels.GetData(), SpectrogramPixels.Num() * sizeof(FColor));
                                        RHICmdList.UnlockTexture2D(TextureResource->GetTexture2DRHI(), 0, true);
                                    }
                                });
                                RTFrame.SpectrogramTexture = SpectrogramTexture; 
                            }
                            else
                            {
                                UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Recurso de textura RHI para espectrograma (original) não disponível para update de pixels."));
                            }
                        }
                        else
                        {
                            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Textura do espectrograma (original) não é válida ou recurso não existe na Game Thread."));
                        }
                    }
                    else 
                    {
                        RTFrame.SpectrogramTexture = nullptr;
                    }
                    
                    if (WaveformPixelsPtr && WaveformPixelsWidth > 0 && WaveformPixelsHeight > 0)
                    {
                        if (!WaveformTexture || WaveformTexture->GetSizeX() != WaveformPixelsWidth || WaveformTexture->GetSizeY() != WaveformPixelsHeight)
                        {
                            if (WaveformTexture) { WaveformTexture = nullptr; }
                            WaveformTexture = UTexture2D::CreateTransient(WaveformPixelsWidth, WaveformPixelsHeight, PF_B8G8R8A8);
                            WaveformTexture->bNoTiling = false;
                            WaveformTexture->CompressionSettings = TC_EditorIcon;
                            WaveformTexture->SRGB = false;
                            WaveformTexture->Filter = TF_Nearest;
                            WaveformTexture->AddressX = TA_Clamp;
                            WaveformTexture->AddressY = TA_Clamp;
                            WaveformTexture->UpdateResource();
                        }

                        if (WaveformTexture && WaveformTexture->GetResource())
                        {
                            FTexture2DResource* TextureResource = static_cast<FTexture2DResource*>(WaveformTexture->GetResource()->GetTexture2DResource());
                            if (TextureResource)
                            {
                                ENQUEUE_RENDER_COMMAND(UpdateWaveformTexture)([TextureResource, WaveformPixels = *WaveformPixelsPtr](FRHICommandListImmediate& RHICmdList)
                                {
                                    if (TextureResource->GetTexture2DRHI())
                                    {
                                        uint32 Stride = 0;
                                        void* LockedData = RHICmdList.LockTexture2D(TextureResource->GetTexture2DRHI(), 0, EResourceLockMode::RLM_WriteOnly, Stride, false);
                                        FMemory::Memcpy(LockedData, WaveformPixels.GetData(), WaveformPixels.Num() * sizeof(FColor));
                                        RHICmdList.UnlockTexture2D(TextureResource->GetTexture2DRHI(), 0, true);
                                    }
                                });
                                RTFrame.WaveformTexture = WaveformTexture; 
                            }
                            else
                            {
                                UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Recurso de textura RHI para waveform não disponível para update de pixels."));
                            }
                        }
                        else
                        {
                            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Textura da waveform não é válida ou recurso não existe na Game Thread."));
                        }
                    }
                    else 
                    {
                        RTFrame.WaveformTexture = nullptr;
                    }

                    if (FilteredSpectrogramPixelsPtr && FilteredSpectrogramPixelsWidth > 0 && FilteredSpectrogramPixelsHeight > 0)
                    {
                        if (!FilteredSpectrogramTexture || FilteredSpectrogramTexture->GetSizeX() != FilteredSpectrogramPixelsWidth || FilteredSpectrogramTexture->GetSizeY() != FilteredSpectrogramPixelsHeight)
                        {
                            if (FilteredSpectrogramTexture) { FilteredSpectrogramTexture = nullptr; }
                            FilteredSpectrogramTexture = UTexture2D::CreateTransient(FilteredSpectrogramPixelsWidth, FilteredSpectrogramPixelsHeight, PF_B8G8R8A8);
                            FilteredSpectrogramTexture->bNoTiling = false;
                            FilteredSpectrogramTexture->CompressionSettings = TC_EditorIcon;
                            FilteredSpectrogramTexture->SRGB = false;
                            FilteredSpectrogramTexture->Filter = TF_Nearest;
                            FilteredSpectrogramTexture->AddressX = TA_Clamp;
                            FilteredSpectrogramTexture->AddressY = TA_Clamp;
                            FilteredSpectrogramTexture->UpdateResource();
                        }

                        if (FilteredSpectrogramTexture && FilteredSpectrogramTexture->GetResource())
                        {
                            FTexture2DResource* TextureResource = static_cast<FTexture2DResource*>(FilteredSpectrogramTexture->GetResource()->GetTexture2DResource());
                            if (TextureResource)
                            {
                                ENQUEUE_RENDER_COMMAND(UpdateFilteredSpectrogramTexture)([TextureResource, FilteredSpectrogramPixels = *FilteredSpectrogramPixelsPtr](FRHICommandListImmediate& RHICmdList)
                                {
                                    if (TextureResource->GetTexture2DRHI())
                                    {
                                        uint32 Stride = 0;
                                        void* LockedData = RHICmdList.LockTexture2D(TextureResource->GetTexture2DRHI(), 0, EResourceLockMode::RLM_WriteOnly, Stride, false);
                                        FMemory::Memcpy(LockedData, FilteredSpectrogramPixels.GetData(), FilteredSpectrogramPixels.Num() * sizeof(FColor));
                                        RHICmdList.UnlockTexture2D(TextureResource->GetTexture2DRHI(), 0, true);
                                    }
                                });
                                RTFrame.FilteredSpectrogramTexture = FilteredSpectrogramTexture;
                            }
                            else
                            {
                                UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Recurso de textura RHI para espectrograma filtrado não disponível para update de pixels."));
                            }
                        }
                        else
                        {
                            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Textura do espectrograma filtrado não é válida ou recurso não existe na Game Thread."));
                        }
                    }
                    else 
                    {
                        RTFrame.FilteredSpectrogramTexture = nullptr; 
                    }
                    
                    OnRealTimeAudioFrameReady.Broadcast(RTFrame);
                });
            }
        }
        else
        {
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: FeatureProcessorInstance é nulo, não é possível gerar features em tempo real."));
        }
        // O frame foi consumido pelo processamento de features RT (dados copiados, etc.),
        // então ele é liberado de volta para o pool aqui.
        if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
    }
    else // Modo de Gravação em Arquivo (AudioStreamSettings.bEnableRTFeatures == false)
    {
        // Verifica se a sessão de captura de áudio está ativa e pronta para receber frames.
        // Apenas fontes de áudio devem ir para o AudioCaptureSession
        if (AudioStreamSettings.ContentType == EIARMediaContentType::Audio && AudioCaptureSession && AudioCaptureSession->IsAnyRecordingActive())
        {
            // Passa o frame para a sessão de captura de áudio para codificação.
            // A responsabilidade de liberar CurrentProcessedFrame de volta para o pool
            // AGORA passa para AudioCaptureSession (que, por sua vez, passará para o Encoder).
            AudioCaptureSession->OnAudioFrameReceived(CurrentProcessedFrame); 
        }
        else
        {
            // Se o modo de gravação está ativo, mas a sessão de captura não está pronta,
            // ou se a gravação foi parada, o frame deve ser liberado aqui.
            UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Frame processado, mas sessão de gravação inativa. Liberando frame."));
            if (FramePool) { FramePool->ReleaseFrame(CurrentProcessedFrame); } 
        }
    }
}

void UIARAudioComponent::OnMIDIFrameAcquired(TSharedPtr<FIAR_MIDIFrame> MIDIFrame)
{
    if (!MIDIFrame.IsValid() || MIDIFrame->Events.Num() == 0)
    {
        UE_LOG(LogIAR, Warning, TEXT("UIARAudioComponent: Frame MIDI inválido ou vazio recebido."));
        // MIDI Frames não vêm do FramePool, então não precisam ser liberados para ele.
        return;
    }

    if (SynthesizerInstance)
    {
        for (const FIAR_MIDIEvent& MIDIEvent : MIDIFrame->Events)
        {
            SynthesizerInstance->ProcessMIDIEvent(MIDIEvent);
        }
    }
    else
    {
        UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: SynthesizerInstance é nulo. Eventos MIDI não puderam ser processados."));
    }
    // MIDI Frames não vêm do FramePool, então não precisam ser explicitamente liberados para ele.
    // A memória será gerenciada pelo TSharedPtr.
}


// ** DEFINIÇÕES DE FUNÇÕES QUE ESTAVAM FALTANDO **

void UIARAudioComponent::ConvertAudioFile(UObject* WorldContextObject, FString InSourceAudioPath, FString OutConvertedAudioPath, const FIAR_AudioConversionSettings& ConversionSettings, bool bOverwrite)
{
    if (!WorldContextObject) { UE_LOG(LogIAR, Error, TEXT("ConvertAudioFile: WorldContextObject é nulo.")); return; }
    IPlatformFile& PlatformFile = (FPlatformFileManager::Get()).GetPlatformFile(); 
    if (PlatformFile.FileExists(*OutConvertedAudioPath))
    {
        if (bOverwrite)
        {
            if (!PlatformFile.DeleteFile(*OutConvertedAudioPath)) { UE_LOG(LogIAR, Error, TEXT("ConvertAudioFile: Falha ao sobrescrever arquivo existente: %s"), *OutConvertedAudioPath); return; }
        }
        else { UE_LOG(LogIAR, Warning, TEXT("ConvertAudioFile: Arquivo de saída já existe e sobrescrita desabilitada: %s"), *OutConvertedAudioPath); return; }
    }
    FString FFmpegExecutablePath = UIARAudioEncoder::GetFFmpegExecutablePathInternal();
    if (FFmpegExecutablePath.IsEmpty() || !PlatformFile.FileExists(*FFmpegExecutablePath)) { UE_LOG(LogIAR, Error, TEXT("ConvertAudioFile: FFmpeg executável não encontrado em: %s. Não é possível converter."), *FFmpegExecutablePath); return; }
    FString FFmpegArguments = UIARECFactory::BuildAudioConversionCommand(InSourceAudioPath, OutConvertedAudioPath, ConversionSettings);
    AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [FFmpegExecutablePath, FFmpegArguments, OutConvertedAudioPath]()
    {
        bool bSuccess = UIARAudioEncoder::LaunchBlockingFFmpegProcess(FFmpegExecutablePath, FFmpegArguments);
        AsyncTask(ENamedThreads::GameThread, [bSuccess, OutConvertedAudioPath]()
        {
            if (bSuccess) { UE_LOG(LogIAR, Log, TEXT("ConvertAudioFile: Conversão concluída com sucesso: %s"), *OutConvertedAudioPath); }
            else
            {
                UE_LOG(LogIAR, Error, TEXT("ConvertAudioFile: Falha na conversão para: %s"), *OutConvertedAudioPath);
                if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutConvertedAudioPath)) { FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*OutConvertedAudioPath); }
            }
        });
    });
}

void UIARAudioComponent::EnumerateAudioInputDevices()
{
    AvailableAudioInputDevices_BPFriendly.Empty();
    TArray<FCaptureDeviceInfo> RawDevicesInfo;
    
    FAudioCapture pCap;
    pCap.GetCaptureDevicesAvailable(RawDevicesInfo);


    UE_LOG(LogIAR, Log, TEXT("--- Dispositivos de Áudio de Entrada Detectados (UIARAudioComponent) ---"));
    if (RawDevicesInfo.Num() == 0) { UE_LOG(LogIAR, Warning, TEXT("Nenhum dispositivo de áudio de entrada detectado.")); }
    else
    {
        for (const FCaptureDeviceInfo& DeviceInfo : RawDevicesInfo)
        {
            UE_LOG(LogIAR, Log, TEXT("  Dispositivo: Microfone (Logi C270 HD WebCam) (ID: {0.0.1.00000000}.{5a6b495d-7b69-4423-8560-5b0fc380f9d8})"));
            UE_LOG(LogIAR, Log, TEXT("  Canais: 1, Sample Rate: 48000 "));
            UE_LOG(LogIAR, Log, TEXT("  Dispositivo: Microfone (USBAudio2.0) (ID: {0.0.1.00000000}.{b9e1fc86-ad78-4860-a6f2-fd70fab96908})"));
            UE_LOG(LogIAR, Log, TEXT("  Canais: 2, Sample Rate: 48000 "));
            FIAR_AudioDeviceInfo BPFriendlyInfo; 
            BPFriendlyInfo.DeviceName = DeviceInfo.DeviceName;
            BPFriendlyInfo.DeviceId = DeviceInfo.DeviceId;
            BPFriendlyInfo.NumInputChannels = DeviceInfo.InputChannels;
            BPFriendlyInfo.SampleRate = DeviceInfo.PreferredSampleRate; 
            AvailableAudioInputDevices_BPFriendly.Add(BPFriendlyInfo);
        }
    }
    UE_LOG(LogIAR, Log, TEXT("--------------------------------------------------"));
}

bool UIARAudioComponent::GetAudioInputDeviceById(const FString& InDeviceId, FIAR_AudioDeviceInfo& OutDeviceInfo)
{
    for (const FIAR_AudioDeviceInfo& DeviceInfo : AvailableAudioInputDevices_BPFriendly)
    {
        if (DeviceInfo.DeviceId == InDeviceId) { OutDeviceInfo = DeviceInfo; return true; }
    }
    return false;
}

TArray<FIAR_AudioDeviceInfo> UIARAudioComponent::GetAvailableAudioInputDevicesList()
{
    return AvailableAudioInputDevices_BPFriendly;
}

void UIARAudioComponent::HandleFolderProcessingCompleted(const FString& OutputFolderPath)
{
    // Apenas retransmite o evento para o delegate público do UIARAudioComponent
    // Você pode adicionar lógica específica aqui se precisar de algum processamento adicional
    // quando o processamento da pasta for concluído.
    OnFolderProcessingCompleted.Broadcast(OutputFolderPath);
    UE_LOG(LogIAR, Log, TEXT("UIARAudioComponent: Processamento de diretório concluído para: %s"), *OutputFolderPath);
}

void UIARAudioComponent::HandleFolderProcessingError(const FString& ErrorMessage)
{
    // Apenas retransmite o evento para o delegate público do UIARAudioComponent
    // Você pode adicionar lógica específica aqui para lidar com erros durante o processamento da pasta.
    OnFolderProcessingError.Broadcast(ErrorMessage);
    UE_LOG(LogIAR, Error, TEXT("UIARAudioComponent: Erro no processamento de diretório: %s"), *ErrorMessage);
}

void UIARAudioComponent::HandleFolderProcessingProgress(const FString& CurrentFileName, float ProgressRatio)
{
    // Apenas retransmite o evento para o delegate público do UIARAudioComponent
    // Você pode adicionar lógica aqui para atualizar a UI, por exemplo.
    OnFolderProcessingProgress.Broadcast(CurrentFileName, ProgressRatio);
    // UE_LOG(LogIAR, Verbose, TEXT("UIARAudioComponent: Progresso de processamento: %s (%.2f%%)"), *CurrentFileName, ProgressRatio * 100.0f);
}
