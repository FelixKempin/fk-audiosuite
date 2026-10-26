#include "fk/engine/TestToneSource.h"

#include <span>

namespace fk::engine
{

void TestToneSource::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled);
}

void TestToneSource::setFrequency (double newFrequencyHz) noexcept
{
    frequencyHz.store (newFrequencyHz);
}

void TestToneSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    oscillator.prepare (sampleRate);
    oscillator.reset();
}

void TestToneSource::releaseResources() {}

void TestToneSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (! enabled.load())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    oscillator.setFrequency (frequencyHz.load());

    auto& buffer = *bufferToFill.buffer;
    const auto numSamples = static_cast<std::size_t> (bufferToFill.numSamples);

    // Kanal 0 berechnen, alle weiteren Kanäle kopieren: Der Ton ist mono.
    const std::span<float> firstChannel (buffer.getWritePointer (0, bufferToFill.startSample), numSamples);
    oscillator.process (firstChannel);

    for (float& sample : firstChannel)
        sample *= outputGain;

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom (
            channel, bufferToFill.startSample, buffer, 0, bufferToFill.startSample, bufferToFill.numSamples);
}

} // namespace fk::engine
