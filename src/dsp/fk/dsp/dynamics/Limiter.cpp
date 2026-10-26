#include "fk/dsp/dynamics/Limiter.h"

#include "fk/dsp/units/Decibels.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fk::dsp::dynamics
{

void Limiter::prepare (double sampleRate, double lookaheadMs)
{
    assert (sampleRate > 0.0 && lookaheadMs >= 0.0);

    lookaheadSamples = static_cast<std::size_t> (std::ceil (lookaheadMs * 0.001 * sampleRate));
    leftDelay.prepare (lookaheadSamples);
    rightDelay.prepare (lookaheadSamples);
    // Fenster = Lookahead + 1: Es muss auch das Sample enthalten, das gerade verzögert
    // ausgegeben wird, sonst kann genau dieses den Ceiling um ein paar Tausendstel dB reißen.
    window.assign (lookaheadSamples + 1, 0.0f);

    // Attack 0: Die Absenkung greift sofort, die Vorausschau liefert die Zeit dafür.
    releaseEnvelope.prepare (sampleRate);
    releaseEnvelope.setAttackMs (0.0);
    reset();
}

void Limiter::reset() noexcept
{
    leftDelay.reset();
    rightDelay.reset();
    std::fill (window.begin(), window.end(), 0.0f);
    windowIndex = 0;
    releaseEnvelope.reset();
    currentReductionDb = 0.0f;
}

void Limiter::setCeilingDb (float ceilingDb) noexcept
{
    ceilingLinear = units::decibelsToGain (ceilingDb);
}

void Limiter::setReleaseMs (double releaseMs) noexcept
{
    releaseEnvelope.setReleaseMs (releaseMs);
}

void Limiter::process (std::span<float> left, std::span<float> right) noexcept
{
    assert (left.size() == right.size());

    for (std::size_t i = 0; i < left.size(); ++i)
    {
        // Neuestes Sample ins Fenster, dann die nötige Absenkung aus der größten Spitze berechnen.
        window[windowIndex] = std::max (std::abs (left[i]), std::abs (right[i]));
        windowIndex = (windowIndex + 1) % window.size();

        // Nötige Absenkung linear rechnen, nicht in dB: Der Umweg über log/pow in float würde
        // den Ceiling um Rundungsfehler überschreiten, und "Brickwall" heißt: nie darüber.
        const float peak = peakInWindow();
        const float wantedReduction =
            peak > ceilingLinear ? 1.0f - ceilingLinear / peak : 0.0f; // 0 = keine, 1 = volle Absenkung
        const float reduction = releaseEnvelope.processSample (wantedReduction);
        const float gain = 1.0f - reduction;

        // Verstärkung auf das verzögerte Signal: Die Spitze, für die wir absenken, kommt erst jetzt an.
        left[i] = leftDelay.pushAndRead (left[i], lookaheadSamples) * gain;
        right[i] = rightDelay.pushAndRead (right[i], lookaheadSamples) * gain;
    }

    currentReductionDb = units::gainToDecibels (1.0f - releaseEnvelope.getCurrentValue());
}

std::size_t Limiter::getLatencySamples() const noexcept
{
    return lookaheadSamples;
}

float Limiter::getCurrentGainReductionDb() const noexcept
{
    return currentReductionDb;
}

float Limiter::peakInWindow() const noexcept
{
    return *std::max_element (window.begin(), window.end());
}

} // namespace fk::dsp::dynamics
