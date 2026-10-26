#include "fk/dsp/dynamics/Compressor.h"

#include "fk/dsp/units/Decibels.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fk::dsp::dynamics
{

void Compressor::prepare (double sampleRate) noexcept
{
    envelope.prepare (sampleRate);
    reset();
}

void Compressor::reset() noexcept
{
    envelope.reset();
    currentReductionDb = 0.0f;
}

void Compressor::setThresholdDb (float thresholdDb) noexcept
{
    gainComputer.setThresholdDb (thresholdDb);
}

void Compressor::setRatio (float ratio) noexcept
{
    gainComputer.setRatio (ratio);
}

void Compressor::setKneeDb (float kneeDb) noexcept
{
    gainComputer.setKneeDb (kneeDb);
}

void Compressor::setAttackMs (double attackMs) noexcept
{
    envelope.setAttackMs (attackMs);
}

void Compressor::setReleaseMs (double releaseMs) noexcept
{
    envelope.setReleaseMs (releaseMs);
}

void Compressor::setMakeupGainDb (float makeupDb) noexcept
{
    makeupGainLinear = units::decibelsToGain (makeupDb);
}

void Compressor::process (std::span<float> left, std::span<float> right) noexcept
{
    process (left, right, left, right);
}

void Compressor::process (std::span<float> left,
                          std::span<float> right,
                          std::span<const float> detectLeft,
                          std::span<const float> detectRight) noexcept
{
    assert (left.size() == right.size() && detectLeft.size() >= left.size() && detectRight.size() >= left.size());

    for (std::size_t i = 0; i < left.size(); ++i)
    {
        // Detektor: der lautere Kanal, als Pegel in dB. Erst messen, dann ändern (Detektor und Signal
        // dürfen derselbe Puffer sein).
        const float peak = std::max (std::abs (detectLeft[i]), std::abs (detectRight[i]));
        const float levelDb = units::gainToDecibels (peak);

        // Die Hüllkurve läuft auf der gewünschten Absenkung (positiv gerechnet), nicht auf dem
        // Signal: So wirken Attack und Release direkt auf das Hörbare, die Pegeländerung.
        const float wantedReduction = -gainComputer.computeGainReductionDb (levelDb);
        const float smoothedReduction = envelope.processSample (wantedReduction);

        const float gain = units::decibelsToGain (-smoothedReduction) * makeupGainLinear;
        left[i] *= gain;
        right[i] *= gain;
    }

    currentReductionDb = -envelope.getCurrentValue();
}

float Compressor::getCurrentGainReductionDb() const noexcept
{
    return currentReductionDb;
}

} // namespace fk::dsp::dynamics
