#pragma once

#include "fk/dsp/filters/Biquad.h"
#include "fk/plugins/InternalPlugin.h"
#include "fk/plugins/analysis/SpectrumAnalyser.h"

#include <array>
#include <vector>

namespace fk::plugins
{

/// Parametrischer 4-Band-EQ plus Hochpass: Low Shelf, zwei Glocken, High Shelf.
/// Koeffizienten werden nur neu berechnet, wenn sich ein Parameter geändert hat.
class EqProcessor final : public InternalPlugin
{
public:
    EqProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /// Ein Band für die Oberfläche: Parameter-IDs (gainId/qId leer beim Hochpass) und Kurzname.
    struct BandDescription
    {
        juce::String frequencyId;
        juce::String gainId;
        juce::String qId;
        juce::String shortLabel;
    };

    /// Hochpass zuerst, dann die vier Bänder; Reihenfolge wie im Signalweg.
    [[nodiscard]] static std::vector<BandDescription> describeBands();

    /// Frequenzgang aller aktiven Filter in dB aus den aktuellen Parametern (Message-Thread,
    /// rechnet die Koeffizienten je Aufruf neu; für die Kurve, nicht für den Audio-Pfad).
    [[nodiscard]] float getMagnitudeDb (float frequencyHz) const;

    [[nodiscard]] analysis::SpectrumAnalyser& getSpectrumAnalyser() noexcept;

private:
    static constexpr int numBands = 4;
    static constexpr int numChannels = 2;

    /// Ein Band: Typ fest, Frequenz/Gain/Q aus Parametern, ein Filter pro Kanal.
    struct Band
    {
        dsp::filters::FilterType type = dsp::filters::FilterType::peak;
        std::atomic<float>* frequencyHz = nullptr;
        std::atomic<float>* gainDb = nullptr;
        std::atomic<float>* q = nullptr;
        std::array<dsp::filters::Biquad<float>, numChannels> filters;
        float lastFrequency = -1.0f;
        float lastGain = 0.0f;
        float lastQ = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateCoefficientsIfNeeded();

    std::atomic<double> currentSampleRate { 44100.0 };
    analysis::SpectrumAnalyser spectrumAnalyser;
    std::atomic<float>& highPassEnabled;
    std::atomic<float>& highPassFrequencyHz;
    float lastHighPassFrequency = -1.0f;
    std::array<dsp::filters::Biquad<float>, numChannels> highPassFilters;
    std::array<Band, numBands> bands;
};

} // namespace fk::plugins
