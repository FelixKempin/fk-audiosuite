#include "fk/dsp/dynamics/NoiseGate.h"

#include "fk/dsp/units/Decibels.h"

#include <cassert>
#include <cmath>

namespace fk::dsp::dynamics
{

void NoiseGate::prepare (double newSampleRate) noexcept
{
    assert (newSampleRate > 0.0);
    sampleRate = newSampleRate;
    detector.prepare (sampleRate);
    detector.setAttackMs (0.5);
    detector.setReleaseMs (20.0);
    gainSmoother.prepare (sampleRate);
    gainSmoother.setAttackMs (1.0);
    gainSmoother.setReleaseMs (60.0);
    setHoldMs (holdMs);
    reset();
}

void NoiseGate::reset() noexcept
{
    detector.reset();
    gainSmoother.reset();
    holdCounter = 0;
    open = false;
}

void NoiseGate::setThresholdDb (float newThresholdDb) noexcept
{
    thresholdDb = newThresholdDb;
    openThreshold = units::decibelsToGain (thresholdDb);
    closeThreshold = units::decibelsToGain (thresholdDb - hysteresisDb);
}

void NoiseGate::setHysteresisDb (float newHysteresisDb) noexcept
{
    hysteresisDb = std::fmax (newHysteresisDb, 0.0f);
    setThresholdDb (thresholdDb);
}

void NoiseGate::setAttackMs (double attackMs) noexcept
{
    gainSmoother.setAttackMs (attackMs);
}

void NoiseGate::setHoldMs (double newHoldMs) noexcept
{
    holdMs = newHoldMs;
    holdSamples = static_cast<std::size_t> (holdMs * 0.001 * sampleRate);
}

void NoiseGate::setReleaseMs (double releaseMs) noexcept
{
    gainSmoother.setReleaseMs (releaseMs);
}

void NoiseGate::process (std::span<float> buffer, std::span<const float> sidechain) noexcept
{
    assert (buffer.size() == sidechain.size());

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        const float level = detector.processSample (std::fabs (sidechain[i]));

        // Zustandsmaschine mit zwei Schwellen: Öffnen bei der oberen, Schließen erst unter der
        // unteren und erst, wenn die Haltezeit abgelaufen ist.
        if (level > openThreshold)
        {
            open = true;
            holdCounter = holdSamples;
        }
        else if (level < closeThreshold)
        {
            if (holdCounter > 0)
                --holdCounter;
            else
                open = false;
        }

        // Das Ziel (1 oder 0) wird mit Attack/Release geglättet: kein Klicken beim Schalten.
        const float gain = gainSmoother.processSample (open ? 1.0f : 0.0f);
        buffer[i] *= gain;
    }
}

bool NoiseGate::isOpen() const noexcept
{
    return open;
}

float NoiseGate::getCurrentGain() const noexcept
{
    return gainSmoother.getCurrentValue();
}

} // namespace fk::dsp::dynamics
