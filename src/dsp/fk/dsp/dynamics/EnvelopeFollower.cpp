#include "fk/dsp/dynamics/EnvelopeFollower.h"

#include <cassert>
#include <cmath>

namespace fk::dsp::dynamics
{

void EnvelopeFollower::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    attackCoefficient = coefficientForTime (attackMs);
    releaseCoefficient = coefficientForTime (releaseMs);
    reset();
}

void EnvelopeFollower::reset() noexcept
{
    envelope = 0.0f;
}

void EnvelopeFollower::setAttackMs (double newAttackMs) noexcept
{
    attackMs = newAttackMs;
    attackCoefficient = coefficientForTime (attackMs);
}

void EnvelopeFollower::setReleaseMs (double newReleaseMs) noexcept
{
    releaseMs = newReleaseMs;
    releaseCoefficient = coefficientForTime (releaseMs);
}

float EnvelopeFollower::processSample (float input) noexcept
{
    // Steigt das Signal, folgt die Hüllkurve mit der Attack-Zeit, sonst mit der Release-Zeit.
    const float coefficient = input > envelope ? attackCoefficient : releaseCoefficient;
    envelope = coefficient * envelope + (1.0f - coefficient) * input;
    return envelope;
}

float EnvelopeFollower::getCurrentValue() const noexcept
{
    return envelope;
}

float EnvelopeFollower::coefficientForTime (double timeMs) const noexcept
{
    if (timeMs <= 0.0)
        return 0.0f; // sofort

    return static_cast<float> (std::exp (-1.0 / (timeMs * 0.001 * sampleRate)));
}

} // namespace fk::dsp::dynamics
