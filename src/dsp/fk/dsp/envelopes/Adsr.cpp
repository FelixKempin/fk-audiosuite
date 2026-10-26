#include "fk/dsp/envelopes/Adsr.h"

#include <algorithm>
#include <cassert>

namespace fk::dsp::envelopes
{

void Adsr::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    reset();
}

void Adsr::setAttackMs (double milliseconds) noexcept
{
    attackMs = milliseconds;
}

void Adsr::setDecayMs (double milliseconds) noexcept
{
    decayMs = milliseconds;
}

void Adsr::setSustainLevel (float level) noexcept
{
    sustainLevel = std::clamp (level, 0.0f, 1.0f);
}

void Adsr::setReleaseMs (double milliseconds) noexcept
{
    releaseMs = milliseconds;
}

void Adsr::noteOn() noexcept
{
    stage = Stage::attack;
}

void Adsr::noteOff() noexcept
{
    if (stage != Stage::idle)
        stage = Stage::release;
}

void Adsr::reset() noexcept
{
    stage = Stage::idle;
    value = 0.0f;
}

float Adsr::nextValue() noexcept
{
    switch (stage)
    {
        case Stage::idle:
            return 0.0f;

        case Stage::attack:
            value += rateForMs (attackMs);

            if (value >= 1.0f)
            {
                value = 1.0f;
                stage = Stage::decay;
            }
            break;

        case Stage::decay:
            value -= rateForMs (decayMs) * (1.0f - sustainLevel);

            // Kleine Toleranz: Nach n Schritten à (1 - sustain)/n landet float knapp über dem Ziel.
            if (value <= sustainLevel + 1.0e-5f)
            {
                value = sustainLevel;
                stage = Stage::sustain;
            }
            break;

        case Stage::sustain:
            value = sustainLevel;
            break;

        case Stage::release:
            value -= rateForMs (releaseMs);

            if (value <= 0.0f)
            {
                value = 0.0f;
                stage = Stage::idle;
            }
            break;
    }

    return value;
}

Adsr::Stage Adsr::getStage() const noexcept
{
    return stage;
}

bool Adsr::isActive() const noexcept
{
    return stage != Stage::idle;
}

float Adsr::rateForMs (double milliseconds) const noexcept
{
    // Schritt pro Sample, damit der volle Weg (0 → 1) genau milliseconds dauert.
    const double samples = std::max (1.0, milliseconds * 0.001 * sampleRate);
    return static_cast<float> (1.0 / samples);
}

} // namespace fk::dsp::envelopes
