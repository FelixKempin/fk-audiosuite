#include "fk/plugins/TrimProcessor.h"

namespace fk::plugins
{

TrimProcessor::TrimProcessor()
    : InternalPlugin (info(), createLayout())
    , gainDb (parameter (gainId))
    , invert (parameter (invertId))
{
}

InternalPlugin::Info TrimProcessor::info()
{
    return { "FK Trim", "Kanal", 0x464b00f0 };
}

juce::AudioProcessorValueTreeState::ParameterLayout TrimProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainId, 1 },
                                                             TRANS ("Trim"),
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
                                                             0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { invertId, 1 }, TRANS ("Phase"), false));
    return layout;
}

void TrimProcessor::prepareToPlay (double sampleRate, int)
{
    smoothedGain.reset (sampleRate, 0.02); // 20 ms gleiten: kein Knacken beim Drehen
    smoothedGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainDb.load()));
}

void TrimProcessor::releaseResources() {}

void TrimProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto sign = invert.load() > 0.5f ? -1.0f : 1.0f;
    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (gainDb.load()));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto gain = sign * smoothedGain.getNextValue();

        for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
            buffer.setSample (channel, i, buffer.getSample (channel, i) * gain);
    }
}

} // namespace fk::plugins
