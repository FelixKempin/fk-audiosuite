#include "fk/dsp/filters/StateVariableFilter.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace fk::dsp::filters
{

void StateVariableFilter::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    reset();
    setCutoff (1000.0, 0.0f);
}

void StateVariableFilter::reset() noexcept
{
    ic1eq = 0.0f;
    ic2eq = 0.0f;
}

void StateVariableFilter::setMode (Mode newMode) noexcept
{
    mode = newMode;
}

void StateVariableFilter::setCutoff (double cutoffHz, float resonance) noexcept
{
    const double limited = std::clamp (cutoffHz, 20.0, sampleRate * 0.45);
    g = static_cast<float> (std::tan (std::numbers::pi * limited / sampleRate));
    k = 2.0f - 2.0f * std::clamp (resonance, 0.0f, 0.98f);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
}

float StateVariableFilter::processSample (float input) noexcept
{
    // Formeln nach Andrew Simper ("Solving the continuous SVF equations ...", Cytomic 2013).
    const float v3 = input - ic2eq;
    const float v1 = a1 * ic1eq + a2 * v3;
    const float v2 = ic2eq + g * v1;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    switch (mode)
    {
        case Mode::lowPass:
            return v2;
        case Mode::bandPass:
            return v1;
        case Mode::highPass:
            return input - k * v1 - v2;
    }

    return v2;
}

void StateVariableFilter::process (std::span<float> buffer) noexcept
{
    for (float& sample : buffer)
        sample = processSample (sample);
}

} // namespace fk::dsp::filters
