// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// -------------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"
#include "AudioAnalysis/IARFeatureProcessor.h"
#include "../GlobalStatics.h"
#include "IARAdvancedAudioFeatureProcessor.generated.h"


/**
 * @brief Processador de features de �udio avan�ado utilizando FFT e OpenCV (opcional).
 * Gera espectrogramas e calcula histogramas de frequ�ncia para detec��o de notas.
 * Agora, gera dados de pixel (TArray<FColor>) para o espectrograma, que ser�o usados na Game Thread.
 */
UCLASS()
class IAR_API UIARAdvancedAudioFeatureProcessor : public UIARFeatureProcessor
{
    GENERATED_BODY()

public:
    UIARAdvancedAudioFeatureProcessor();
    virtual ~UIARAdvancedAudioFeatureProcessor();

    virtual void Initialize() override;
    virtual void Shutdown() override;

    /**
     * @brief Implementa��o concreta de ProcessFrame para extrair features e Attitude-Gram.
     * Utiliza FFT para an�lise de frequ�ncia e OpenCV (se ativado) para histograma.
     * @param OutSpectrogramTexture Ponteiro para a textura do espectrograma principal para ser atualizada.
     * @return As caracter�sticas de �udio extra�das.
     */
    virtual FIAR_AudioFeatures ProcessFrame(const TSharedPtr<FIAR_AudioFrameData>& AudioFrame, UTexture2D*& OutSpectrogramTexture) override;

    // --- UPROPERTYs para ordem de inicializa��o (corrige C5038) ---
    // Estas s�o as primeiras propriedades inicializadas no construtor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|Waveform")
    int32 WaveformDisplayWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|Waveform")
    int32 WaveformDisplayHeight;

private: // Vari�veis de membro n�o UPROPERTY, mas que s�o inicializadas cedo no construtor
    int32 FFTWindowSize; 
    int32 SpectrogramHeight; 
    int32 MaxSpectrogramHistoryFrames; 

public: // As propriedades p�blicas restantes, que s�o inicializadas depois no construtor
    // Propriedades para Filtragem Contextual de Frequ�ncia
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|Contextual Filtering",
              meta = (Tooltip = "Habilita a filtragem de frequ�ncias fora da faixa esperada, baseada na nota anteriormente detectada."))
    bool bEnableContextualFrequencyFiltering = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|Contextual Filtering",
              meta = (ClampMin = "1", ClampMax = "60", Tooltip = "Define o tamanho da janela de filtragem em semitons, centrado na �ltima nota detectada (ex: 12 semitons = 1 oitava)."))
    int32 ContextualFilterSemitoneRange = 12; // Ex: +/- 1 oitava

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IAR|AudioAnalysis|Contextual Filtering",
              meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Fator pelo qual a energia das frequ�ncias fora da janela ser� atenuada (0.0 = silenciado, 1.0 = sem atenua��o)."))
    float ContextualFilterAttenuationFactor = 0.05f; // Atenua��o de 95%

    // --- Getters para pixels ---
    /**
     * @brief Retorna os dados de pixel do espectrograma principal (n�o filtrado) mais recente.
     * @param OutWidth Largura dos dados de pixel.
     * @param OutHeight Altura dos dados de pixel.
     * @return Uma refer�ncia constante � TArray de FColors contendo os pixels do espectrograma.
     */
    const TArray<FColor>& GetSpectrogramPixels(int32& OutWidth, int32& OutHeight) const;

    /**
     * @brief Retorna os dados de pixel da waveform mais recente.
     * @param OutWidth Largura dos dados de pixel.
     * @param OutHeight Altura dos dados de pixel.
     * @return Uma refer�ncia constante � TArray de FColors contendo os pixels da waveform.
     */
    const TArray<FColor>& GetWaveformPixels(int32& OutWidth, int32& OutHeight) const;

    /**
     * @brief Retorna os dados de pixel do espectrograma FILTRADO mais recente.
     * @param OutWidth Largura dos dados de pixel.
     * @param OutHeight Altura dos dados de pixel.
     * @return Uma refer�ncia constante � TArray de FColors contendo os pixels do espectrograma filtrado.
     */
    const TArray<FColor>& GetFilteredSpectrogramPixels(int32& OutWidth, int32& OutHeight) const;

private:
    // Hist�rico de espectros para construir um espectrograma visual (UTexture2D)
    TArray<TArray<float>> SpectrogramDataHistory; // Armazena espectros de frames recentes (N�O FILTRADO)
    TArray<TArray<float>> FilteredSpectrogramDataHistory; // NOVO: Armazena espectros de frames recentes (FILTRADO)
    
    // Buffer de pixels gerado na Audio Thread, pronto para ser copiado para a UTexture2D na Game Thread
    TArray<FColor> CurrentSpectrogramPixels; // N�O FILTRADO
    TArray<FColor> CurrentFilteredSpectrogramPixels; // NOVO: FILTRADO
    
    // NOVO: Buffer de pixels para a waveform
    TArray<FColor> CurrentWaveformPixels;

    // Estado para Attitude-Gram (acumuladores para c�lculo ao longo do tempo)
    TArray<FIAR_AudioNoteFeature> DetectedNotesHistory; // Hist�rico de notas para Attitude-Gram
    FIAR_AudioNoteFeature CurrentBuildingNote; // Nota sendo "constru�da" ao longo dos frames
    float NoteOnsetTimestamp = 0.0f; // Timestamp de in�cio da nota atual
    bool bIsNoteActive = false; // Flag para indicar se uma nota est� "ativa"

    /**
     * @brief Converte um buffer de �udio (mono) em um espectro de frequ�ncia usando FFT.
     * Pode usar OpenCV se ativado, ou uma simula��o para compila��o.
     * @param Samples Amostras de �udio mono.
     * @param FrameSampleRate A Sample Rate do frame atual.
     * @param OutSpectrum O espectro de magnitude resultante da FFT (normalizado entre 0 e 1).
     * @return True se a FFT foi calculada com sucesso.
     */
    bool CalculateFFT(const TArray<float>& Samples, int32 FrameSampleRate, TArray<float>& OutSpectrum);

    /**
     * @brief Identifica os K maiores picos de frequ�ncia no espectro e os mapeia para notas MIDI.
     * @param Spectrum O espectro de magnitude da FFT.
     * @param SampleRate Taxa de amostragem original.
     * @param NumPeaks O n�mero de picos a serem encontrados.
     * @return Um array de FIAR_AudioNoteFeature representando as notas detectadas.
     */
    TArray<FIAR_AudioNoteFeature> FindTopFrequencyNotes(const TArray<float>& Spectrum, int32 SampleRate, int32 NumPeaks = 3);
    
    /**
     * @brief Gera os pixels para o espectrograma a partir do hist�rico.
     * @param OutPixels A TArray<FColor> para armazenar os pixels gerados.
     * @param Width Largura desejada da imagem.
     * @param Height Altura desejada da imagem.
     * @param InHistory O hist�rico de espectros a ser usado (filtrado ou n�o filtrado).
     */
    void GenerateSpectrogramPixels(TArray<FColor>& OutPixels, int32 Width, int32 Height, const TArray<TArray<float>>& InHistory);

    /**
     * @brief Gera os pixels para a waveform a partir das amostras de �udio.
     * @param InMonoSamples As amostras de �udio monof�nicas.
     * @param OutPixels A TArray<FColor> para armazenar os pixels gerados.
     * @param Width Largura desejada da imagem.
     * @param Height Altura desejada da imagem.
     */
    void GenerateWaveformPixels(const TArray<float>& InMonoSamples, TArray<FColor>& OutPixels, int32 Width, int32 Height);

    // Fun��o para aplicar a filtragem contextual de frequ�ncia
    void ApplyContextualFrequencyFilter(TArray<float>& Spectrum, int32 FrameSampleRate);
};
