#pragma once

#include "fk/dsp/dynamics/Compressor.h"
#include "fk/plugins/InternalPlugin.h"

namespace fk::plugins
{

/// Kompressor-Plugin um dsp::dynamics::Compressor. Parameter werden pro Block in den DSP-Kern übertragen.
class CompressorProcessor final : public InternalPlugin
{
public:
    CompressorProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /// Aktuelle Absenkung in dB (≤ 0) für Anzeigen; vom Message-Thread lesbar.
    [[nodiscard]] float getGainReductionDb() const noexcept;

    /// Spitzenpegel des letzten Blocks am Eingang in dB (für den Punkt auf der Kennlinie).
    [[nodiscard]] float getInputLevelDb() const noexcept;

    /// Statische Kennlinie mit den aktuellen Parametern: Eingang dB → Ausgang dB (mit Makeup).
    [[nodiscard]] float transferDb (float inputDb) const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& thresholdDb;
    std::atomic<float>& ratio;
    std::atomic<float>& attackMs;
    std::atomic<float>& releaseMs;
    std::atomic<float>& kneeDb;
    std::atomic<float>& makeupDb;
    std::atomic<float>& externalSidechain;

    dsp::dynamics::Compressor compressor;
    std::atomic<float> gainReductionDb = 0.0f;
    std::atomic<float> inputLevelDb = -100.0f;
};

} // namespace fk::plugins
