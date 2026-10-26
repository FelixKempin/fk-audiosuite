#include "fk/dsp/generators/Oscillator.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace fk::dsp::generators
{

void Oscillator::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    reset();
}

void Oscillator::reset() noexcept
{
    phase = 0.0;
    triangleState = 0.0f;
}

void Oscillator::setWaveform (Waveform newWaveform) noexcept
{
    waveform = newWaveform;
}

void Oscillator::setFrequency (double frequencyHz) noexcept
{
    phaseIncrement = frequencyHz / sampleRate;
}

float Oscillator::nextSample() noexcept
{
    float sample = 0.0f;

    switch (waveform)
    {
        case Waveform::sine:
            sample = static_cast<float> (std::sin (2.0 * std::numbers::pi * phase));
            break;

        case Waveform::saw:
            // Naiver Sägezahn -1..1, dann den Sprung bei Phase 0 glätten.
            sample = static_cast<float> (2.0 * phase - 1.0) - polyBlep (phase);
            break;

        case Waveform::square:
        {
            // Rechteck: Sprung nach oben bei 0, nach unten bei 0,5.
            sample = phase < 0.5 ? 1.0f : -1.0f;
            sample += polyBlep (phase);
            sample -= polyBlep (std::fmod (phase + 0.5, 1.0));
            break;
        }

        case Waveform::triangle:
        {
            // Dreieck als Integral des bandbegrenzten Rechtecks (Leaky Integrator gegen Drift).
            float square = phase < 0.5 ? 1.0f : -1.0f;
            square += polyBlep (phase);
            square -= polyBlep (std::fmod (phase + 0.5, 1.0));
            const auto increment = static_cast<float> (phaseIncrement);
            triangleState = 0.999f * triangleState + 4.0f * increment * square;
            sample = triangleState;
            break;
        }
    }

    phase += phaseIncrement;

    if (phase >= 1.0)
        phase -= 1.0;

    return sample;
}

float Oscillator::polyBlep (double t) const noexcept
{
    const double dt = phaseIncrement;

    if (t < dt)
    {
        // Kurz nach dem Sprung: Parabel, die den Sprung weich macht.
        const double x = t / dt;
        return static_cast<float> (x + x - x * x - 1.0);
    }

    if (t > 1.0 - dt)
    {
        // Kurz vor dem Sprung.
        const double x = (t - 1.0) / dt;
        return static_cast<float> (x * x + x + x + 1.0);
    }

    return 0.0f;
}

} // namespace fk::dsp::generators
