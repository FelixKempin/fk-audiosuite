#include "fk/plugins/LimiterProcessor.h"

#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto ceilingId = "ceiling";
constexpr auto releaseId = "release";
} // namespace

LimiterProcessor::LimiterProcessor()
    : InternalPlugin (info(), createLayout())
    , ceilingDb (parameter (ceilingId))
    , releaseMs (parameter (releaseId))
{
}

InternalPlugin::Info LimiterProcessor::info()
{
    return { "FK Limiter", "Dynamik", 0x464b0004 };
}

juce::AudioProcessorValueTreeState::ParameterLayout LimiterProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ceilingId, 1 },
                                                             TRANS ("Ceiling"),
                                                             juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f),
                                                             -0.3f,
                                                             Attributes().withLabel ("dB")));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { releaseId, 1 },
                                                     TRANS ("Release"),
                                                     juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f),
                                                     100.0f,
                                                     Attributes().withLabel ("ms")));
    return layout;
}

void LimiterProcessor::prepareToPlay (double sampleRate, int)
{
    limiter.prepare (sampleRate, lookaheadMs);
    setLatencySamples (static_cast<int> (limiter.getLatencySamples()));
}

void LimiterProcessor::releaseResources()
{
    limiter.reset();
}

void LimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    limiter.setCeilingDb (ceilingDb.load());
    limiter.setReleaseMs (releaseMs.load());

    const auto numSamples = static_cast<std::size_t> (buffer.getNumSamples());
    limiter.process (std::span<float> (buffer.getWritePointer (0), numSamples),
                     std::span<float> (buffer.getWritePointer (1), numSamples));

    gainReductionDb.store (limiter.getCurrentGainReductionDb());
}

float LimiterProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load();
}

ui::PanelDescription LimiterProcessor::describePanel()
{
    return { .title = info().name,
             .sections = { { .title = TRANS ("Limiter"),
                             .controls = { ui::knob (ceilingId),
                                           ui::knob (releaseId),
                                           ui::gainReduction ([this] { return getGainReductionDb(); }) } } } };
}

} // namespace fk::plugins
