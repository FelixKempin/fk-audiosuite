#pragma once

#include "fk/dsp/dynamics/Limiter.h"
#include "fk/plugins/InternalPlugin.h"

namespace fk::plugins
{

/// Limiter-Plugin um dsp::dynamics::Limiter. Meldet die Lookahead-Latenz an den Host (PDC).
class LimiterProcessor final : public InternalPlugin
{
public:
    LimiterProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    [[nodiscard]] float getGainReductionDb() const noexcept;

private:
    static constexpr double lookaheadMs = 5.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& ceilingDb;
    std::atomic<float>& releaseMs;

    dsp::dynamics::Limiter limiter;
    std::atomic<float> gainReductionDb = 0.0f;
};

} // namespace fk::plugins
