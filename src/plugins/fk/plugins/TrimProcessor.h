#pragma once

#include "fk/plugins/InternalPlugin.h"

namespace fk::plugins
{

/// Eingangs-Trim des Kanalzugs (Cubase: Eingangsverstärkung und Phase): Gain vor den Inserts und
/// Phasenumkehr je Kanal. Kein Effekt zum Auswählen: Der Mixer legt es bei Bedarf unsichtbar vor die
/// Inserts (hinter das Instrument) und zeigt Trim und Phase als Regler im Kanal.
class TrimProcessor final : public InternalPlugin
{
public:
    TrimProcessor();

    [[nodiscard]] static Info info();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    static constexpr auto gainId = "trim";
    static constexpr auto invertId = "phase";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& gainDb;
    std::atomic<float>& invert;
    juce::SmoothedValue<float> smoothedGain { 1.0f };
};

} // namespace fk::plugins
