#pragma once

#include "fk/dsp/dynamics/NoiseGate.h"
#include "fk/plugins/InternalPlugin.h"

#include <vector>

namespace fk::plugins
{

/// Gate-Plugin um dsp::dynamics::NoiseGate, Stereo verkoppelt: gemessen wird die Summe beider
/// Kanäle, beide bekommen dieselbe Verstärkung (sonst wandert das Stereobild). Range begrenzt, wie
/// weit das Gate absenkt: −80 dB ist zu, −10 dB nur leiser (Expander-artig, natürlicher bei Drums).
class GateProcessor final : public InternalPlugin
{
public:
    GateProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    [[nodiscard]] float getGainReductionDb() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& thresholdDb;
    std::atomic<float>& rangeDb;
    std::atomic<float>& attackMs;
    std::atomic<float>& holdMs;
    std::atomic<float>& releaseMs;
    std::atomic<float>& hysteresisDb;
    std::atomic<float>& externalSidechain;

    dsp::dynamics::NoiseGate gate;
    std::vector<float> gains;    ///< je Sample die Verstärkung des Gates (0..1)
    std::vector<float> detector; ///< Mono-Summe zum Messen (eigenes Signal oder Sidechain)
    std::atomic<float> gainReductionDb = 0.0f;
};

} // namespace fk::plugins
