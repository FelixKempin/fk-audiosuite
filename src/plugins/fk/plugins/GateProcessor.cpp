#include "fk/plugins/GateProcessor.h"

#include <algorithm>
#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto thresholdId = "threshold";
constexpr auto rangeId = "range";
constexpr auto attackId = "attack";
constexpr auto holdId = "hold";
constexpr auto releaseId = "release";
constexpr auto hysteresisId = "hysteresis";
constexpr auto sidechainId = "sidechain";
} // namespace

GateProcessor::GateProcessor()
    : InternalPlugin (info(), createLayout())
    , thresholdDb (parameter (thresholdId))
    , rangeDb (parameter (rangeId))
    , attackMs (parameter (attackId))
    , holdMs (parameter (holdId))
    , releaseMs (parameter (releaseId))
    , hysteresisDb (parameter (hysteresisId))
    , externalSidechain (parameter (sidechainId))
{
}

InternalPlugin::Info GateProcessor::info()
{
    return { .name = "FK Gate", .category = "Dynamik", .uniqueId = 0x464b0008, .hasSidechain = true };
}

juce::AudioProcessorValueTreeState::ParameterLayout GateProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add =
        [&layout] (
            const char* id, const char* name, juce::NormalisableRange<float> range, float value, const char* unit)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, juce::translate (name), range, value, Attributes().withLabel (unit)));
    };

    add (thresholdId, NEEDS_TRANS ("Threshold"), { -80.0f, 0.0f, 0.1f }, -40.0f, "dB");
    add (rangeId, NEEDS_TRANS ("Range"), { -80.0f, 0.0f, 0.1f }, -80.0f, "dB");
    add (attackId, NEEDS_TRANS ("Attack"), { 0.05f, 50.0f, 0.01f, 0.4f }, 0.5f, "ms");
    add (holdId, NEEDS_TRANS ("Hold"), { 0.0f, 500.0f, 1.0f, 0.5f }, 50.0f, "ms");
    add (releaseId, NEEDS_TRANS ("Release"), { 5.0f, 2000.0f, 1.0f, 0.4f }, 150.0f, "ms");
    add (hysteresisId, NEEDS_TRANS ("Hysteresis"), { 0.0f, 12.0f, 0.1f }, 6.0f, "dB");
    layout.add (
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID { sidechainId, 1 }, TRANS ("Ext. Sidechain"), false));
    return layout;
}

void GateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    gate.prepare (sampleRate);
    gains.assign (static_cast<std::size_t> (std::max (samplesPerBlock, 1)), 1.0f);
    detector.assign (gains.size(), 0.0f);
}

void GateProcessor::releaseResources()
{
    gate.reset();
}

void GateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2 || gains.empty())
        return;

    gate.setThresholdDb (thresholdDb.load());
    gate.setHysteresisDb (hysteresisDb.load());
    gate.setAttackMs (attackMs.load());
    gate.setHoldMs (holdMs.load());
    gate.setReleaseMs (releaseMs.load());

    const auto floor = juce::Decibels::decibelsToGain (rangeDb.load(), -80.0f);
    const auto sidechain = externalSidechain.load() > 0.5f ? sidechainOf (buffer) : std::nullopt;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto total = static_cast<std::size_t> (buffer.getNumSamples());
    float lowestGain = 1.0f;

    for (std::size_t start = 0; start < total; start += gains.size())
    {
        const auto count = std::min (gains.size(), total - start);

        // Das Gate "bearbeitet" einen Puffer aus Einsen: Danach steht darin seine Verstärkung je Sample.
        for (std::size_t i = 0; i < count; ++i)
        {
            // Gemessen wird das eigene Signal oder, mit externer Sidechain, die andere Spur.
            const auto index = static_cast<int> (start + i);
            detector[i] = sidechain ? 0.5f * (sidechain->getSample (0, index) + sidechain->getSample (1, index))
                                    : 0.5f * (left[start + i] + right[start + i]);
            gains[i] = 1.0f;
        }

        gate.process (std::span<float> (gains.data(), count), std::span<const float> (detector.data(), count));

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto gain = floor + (1.0f - floor) * gains[i]; // Range: nie tiefer als floor
            left[start + i] *= gain;
            right[start + i] *= gain;
            lowestGain = std::min (lowestGain, gain);
        }
    }

    gainReductionDb.store (juce::Decibels::gainToDecibels (lowestGain, -80.0f));
}

float GateProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load();
}

ui::PanelDescription GateProcessor::describePanel()
{
    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Gate"),
                   .controls = { ui::knob (thresholdId, TRANS ("The gate closes below this level")),
                                 ui::knob (rangeId, TRANS ("How far it attenuates: -80 dB = closed, -10 dB = just quieter")),
                                 ui::knob (hysteresisId, TRANS ("Closes only this far below the threshold: prevents chattering")),
                                 ui::knob (sidechainId, TRANS ("Open by another track (e.g. the bass opens with the kick). Source: right-click the insert slot, then Sidechain source")),
                                 ui::gainReduction ([this] { return getGainReductionDb(); }) } },
                 { .title = TRANS ("Time"),
                   .controls = {
                       ui::knob (attackId, TRANS ("How fast it opens: short = the attack stays crisp")),
                       ui::knob (holdId, TRANS ("Stays open this long after the level drops below the threshold")),
                       ui::knob (releaseId, TRANS ("How slowly it closes: long = natural decay")) } } } };
}

} // namespace fk::plugins
