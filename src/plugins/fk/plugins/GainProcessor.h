#pragma once

#include "fk/plugins/InternalPlugin.h"

#include <juce_dsp/juce_dsp.h>

namespace fk::plugins
{

/// Das einfachste Plugin: Lautstärke in dB, mit Rampe gegen Knackser.
/// Dient als Vorlage für alle weiteren (→ LEARNING.md §10).
class GainProcessor final : public InternalPlugin
{
public:
    GainProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& gainDb;
    juce::dsp::Gain<float> gain;
};

} // namespace fk::plugins
