#include "fk/dsp/generators/SineOscillator.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace fk::dsp::generators
{

void SineOscillator::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    updatePhaseIncrement();
}

void SineOscillator::setFrequency (double newFrequencyHz) noexcept
{
    assert (newFrequencyHz >= 0.0);
    frequencyHz = newFrequencyHz;
    updatePhaseIncrement();
}

double SineOscillator::getFrequency() const noexcept
{
    return frequencyHz;
}

void SineOscillator::reset() noexcept
{
    phase = 0.0;
}

float SineOscillator::nextSample() noexcept
{
    const double sample = std::sin (2.0 * std::numbers::pi * phase);

    phase += phaseIncrement;

    if (phase >= 1.0)
        phase -= 1.0;

    return static_cast<float> (sample);
}

void SineOscillator::process (std::span<float> output) noexcept
{
    for (float& sample : output)
        sample = nextSample();
}

void SineOscillator::updatePhaseIncrement() noexcept
{
    // Umdrehungen pro Sample: Bei 440 Hz und 44100 Hz Samplerate etwa 0,01.
    phaseIncrement = frequencyHz / sampleRate;
}

} // namespace fk::dsp::generators
