// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of Williäm Wolff and protected by copyright law.
// Prohibited copy or distribution without expressed authorization of the Author.

using System.IO;
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class IAR : ModuleRules
{
    public IAR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                // Adicione caminhos de include públicos necessários aqui (por exemplo, para headers globais)
                Path.Combine(ModuleDirectory, "Public"),
                Path.Combine(ModuleDirectory, "Public", "Core"),
                Path.Combine(ModuleDirectory, "Public", "Components"),
                Path.Combine(ModuleDirectory, "Public", "Recording"),
                Path.Combine(ModuleDirectory, "Public", "UI"),
                Path.Combine(ModuleDirectory, "Public", "AudioAnalysis"),
                Path.Combine(ModuleDirectory, "Public", "MIDI")
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                // Adicione outros caminhos de include privados necessários aqui
                Path.Combine(ModuleDirectory, "Private"),
                Path.Combine(ModuleDirectory, "Private", "Core"),
                Path.Combine(ModuleDirectory, "Private", "Components"),
                Path.Combine(ModuleDirectory, "Private", "Recording"),
                Path.Combine(ModuleDirectory, "Private", "UI"),
                Path.Combine(ModuleDirectory, "Private", "AudioAnalysis"),
                Path.Combine(ModuleDirectory, "Private", "MIDI"),
                // VAI AQUI A CORREÇÃO FINAL: Removido 'Target.' de EngineDirectory
                Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Public") // <-- CORRIGIDO AQUI!
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "AudioCapture", // Para captura de entrada de áudio do sistema
                "AudioCaptureCore", // Para captura de entrada de áudio do sistema
                "UMG",          // Para widgets de UI
                "Slate",        // Para elementos de UI de baixo nível
                "SlateCore",    // Para elementos de UI de baixo nível
                "RHI",          // Para acesso a recursos de renderização (e.g., texturas de debug)
                "RenderCore"    // Para acesso a recursos de renderização
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                // Adicione dependências privadas que você linka estaticamente aqui
                "ImageWrapper" // Se for usar para carregar/salvar imagens ou visualização de debug
            }
        );


        //MIDIDevice Definitions - Only Win64 and Mac are defined in the Plugin
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.Linux))
        {
            PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(EngineDirectory, "Plugins", "Runtime", "MIDIDevice", "Source", "MIDIDevice", "Public")
            });

            PublicDependencyModuleNames.Add("MIDIDevice");

            //AddEngineThirdPartyPrivateStaticDependencies(Target, "portmidi");
        }

        //OpenCV Definitions
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.Linux))
        {
            // **CORREÇÃO**: Adição explícita do caminho público do OpenCVHelper
            // Isso ajuda o compilador a encontrar 'OpenCVHelper.h', 'PreOpenCVHeaders.h', 'PostOpenCVHeaders.h'
            // quando o plugin OpenCV está habilitado.
            PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(EngineDirectory, "Plugins", "Runtime", "OpenCV", "Source", "OpenCVHelper", "Public")
            });

            //OpenCV Only for the Target Platforms.
            //To make possible it works on Android a really big platform change will be needed on the Code Infrastructure, mainly on the Pipes!
            PublicDependencyModuleNames.Add("OpenCV");
            PublicDependencyModuleNames.Add("OpenCVHelper");
        }

        // Condicional para módulos RHI específicos de plataforma
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D12RHI");
            //PrivateDependencyModuleNames.Add("HeadMountedDisplay");
            // Se você tiver código que se beneficia diretamente do D3D12, adicione aqui.
            // Ex: PrivateDependencyModuleNames.Add("D3D12Core");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Para Android, você pode precisar de VulkanRHI ou OpenGLDRHI
            // se o seu plugin interage diretamente com a renderização de baixo nível.
            // Se não, você pode não precisar adicionar RHIs específicas aqui.
            // PrivateDependencyModuleNames.Add("VulkanRHI");
            // PrivateDependencyModuleNames.Add("OpenGLDRHI");
            PCHUsage = PCHUsageMode.NoPCHs;
        }
        // Repita para outras plataformas como Linux, Mac, etc., com seus RHIs correspondentes
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // OpenGL ou Metal RHI para Mac, dependendo da versão do UE e necessidades.
            // PrivateDependencyModuleNames.Add("MetalRHI");
        }
        // Configurações para Thread-Safety e Multiplataforma
        // Desativar Unity Builds para facilitar a depuração de arquivos individuais no início
        //bUseUnity = false;

        // Habilita as características de Thread-Safety e Lock-Free
        // As estruturas do Unreal (FThreadSafeBool, TQueue, FEvent) já são thread-safe.
        // Para std::atomic, garantir includes diretos nas classes (.h).

        // Bibliotecas de Terceiros (ex: FFmpeg, RtMidi, DSP libs)
        // Esta seção será expandida conforme as dependências externas forem integradas.
        // As implementações das funções Load* abaixo são apenas exemplos e precisam ser adaptadas
        // para sua distribuição específica das bibliotecas.

        // Exemplo de como carregar FFmpeg (se for linkar estaticamente ou precisar dos headers)
        // LoadFFmpeg(Target);
        // Exemplo de como carregar RtMidi (para MIDI I/O)
        // LoadRtMidi(Target);
        // Exemplo de como carregar uma biblioteca DSP (e.g., para FFT ou processamento de áudio)
        // LoadDSPFramework(Target);
    }

    /*
    // Funções de exemplo para carregar bibliotecas de terceiros (descomente e adapte conforme necessário)
    private bool LoadFFmpeg(ReadOnlyTargetRules Target)
    {
        // Exemplo de estrutura de diretório para FFmpeg dentro do seu plugin:
        // [PluginRoot]/ThirdParty/FFmpeg/
        //                       /include/ (headers)
        //                       /lib/Win64/ (arquivos .lib)
        //                       /bin/Win64/ (arquivos .dll ou executáveis)

        string FFmpegPath = Path.Combine(PluginDirectory, "ThirdParty", "FFmpeg");
        if (!Directory.Exists(FFmpegPath))
        {
            Log.TraceInformation("FFmpeg not found at: " + FFmpegPath);
            return false;
        }

        string LibPath = "";
        string BinPath = "";
        string IncludePath = Path.Combine(FFmpegPath, "include");

        PublicIncludePaths.Add(IncludePath);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibPath = Path.Combine(FFmpegPath, "lib", "Win64");
            BinPath = Path.Combine(FFmpegPath, "bin", "Win64"); // Para DLLs ou executáveis
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "avcodec.lib")); 
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "avformat.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "avutil.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "swresample.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "swscale.lib"));
            
            // Para DLLs, adicione-as às RuntimeDependencies e PublicDelayLoadDLLs
            PublicDelayLoadDLLs.Add("avcodec.dll");
            RuntimeDependencies.Add(Path.Combine(BinPath, "avcodec.dll"));
            // Repita para avformat, avutil, etc.
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            LibPath = Path.Combine(FFmpegPath, "lib", "Linux");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavcodec.so"));
            // ...
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibPath = Path.Combine(FFmpegPath, "lib", "Mac");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavcodec.dylib"));
            // ...
        }
        return true;
    }

    private bool LoadRtMidi(ReadOnlyTargetRules Target)
    {
        string RtMidiPath = Path.Combine(PluginDirectory, "ThirdParty", "RtMidi");
        if (!Directory.Exists(RtMidiPath))
        {
            Log.TraceInformation("RtMidi not found at: " + RtMidiPath);
            return false;
        }

        string LibPath = "";
        string IncludePath = Path.Combine(RtMidiPath, "include");

        PublicIncludePaths.Add(IncludePath);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibPath = Path.Combine(RtMidiPath, "lib", "Win64");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "rtmidi.lib")); 
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            LibPath = Path.Combine(RtMidiPath, "lib", "Linux");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "librtmidi.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibPath = Path.Combine(RtMidiPath, "lib", "Mac");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "librtmidi.dylib"));
        }
        return true;
    }
    */
}