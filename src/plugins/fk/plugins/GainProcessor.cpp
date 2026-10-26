#include "fk/plugins/GainProcessor.h"

namespace fk::plugins
{

namespace
{
constexpr auto gainId = "gain";
} // namespace

GainProcessor::GainProcessor()
    : InternalPlugin (info(), createLayout())
    , gainDb (parameter (gainId))
{
}

InternalPlugin::Info GainProcessor::info()
{
    return { "FK Gain", "Utility", 0x464b0001 };
}

juce::AudioProcessorValueTreeState::ParameterLayout GainProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainId, 1 },
                                                             TRANS ("Gain"),
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
                                                             0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

void GainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    gain.prepare ({ sampleRate, static_cast<juce::uint32> (samplesPerBlock), 2 });
    gain.setRampDurationSeconds (0.02);
    gain.setGainDecibels (gainDb.load());
}

void GainProcessor::releaseResources()
{
    gain.reset();
}

void GainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    gain.setGainDecibels (gainDb.load());

    juce::dsp::AudioBlock<float> block (buffer);
    gain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

ui::PanelDescription GainProcessor::describePanel()
{
    return { .title = info().name, .sections = { { .title = TRANS ("Level"), .controls = { ui::knob (gainId) } } } };
}

} // namespace fk::plugins
