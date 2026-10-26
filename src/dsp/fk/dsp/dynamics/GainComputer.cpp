#include "fk/dsp/dynamics/GainComputer.h"

#include <algorithm>

namespace fk::dsp::dynamics
{

void GainComputer::setThresholdDb (float newThresholdDb) noexcept
{
    thresholdDb = newThresholdDb;
}

void GainComputer::setRatio (float newRatio) noexcept
{
    ratio = std::max (1.0f, newRatio);
}

void GainComputer::setKneeDb (float kneeWidthDb) noexcept
{
    kneeDb = std::max (0.0f, kneeWidthDb);
}

float GainComputer::computeGainReductionDb (float inputLevelDb) const noexcept
{
    const float overshoot = inputLevelDb - thresholdDb;
    const float halfKnee = kneeDb * 0.5f;

    // Unter dem Knie: nichts. Im Knie: quadratischer Übergang. Darüber: gerade Linie mit Ratio.
    if (overshoot <= -halfKnee)
        return 0.0f;

    const float slope = 1.0f / ratio - 1.0f; // z. B. Ratio 4 → -0,75 dB Ausgang pro dB Eingang über Threshold

    if (overshoot < halfKnee && kneeDb > 0.0f)
    {
        const float x = overshoot + halfKnee;
        return slope * x * x / (2.0f * kneeDb);
    }

    return slope * overshoot;
}

} // namespace fk::dsp::dynamics
