#include "fk/dsp/delay/StereoDelay.h"

#include <algorithm>
#include <cmath>

namespace fk::dsp::delay
{

void StereoDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    left.prepare (static_cast<std::size_t> (maxDelaySeconds * sampleRate) + 4);
    right.prepare (static_cast<std::size_t> (maxDelaySeconds * sampleRate) + 4);
    lowCutLeft.prepare (sampleRate, filters::OnePole::Mode::highPass, current.lowCutHz);
    lowCutRight.prepare (sampleRate, filters::OnePole::Mode::highPass, current.lowCutHz);
    highCutLeft.prepare (sampleRate, filters::OnePole::Mode::lowPass, current.highCutHz);
    highCutRight.prepare (sampleRate, filters::OnePole::Mode::lowPass, current.highCutHz);
    smoothing = static_cast<float> (1.0 - std::exp (-1.0 / (0.05 * sampleRate)));
    setSettings (current);
    reset();
}

void StereoDelay::reset() noexcept
{
    snapToTarget = true;
    left.reset();
    right.reset();
    lowCutLeft.reset();
    lowCutRight.reset();
    highCutLeft.reset();
    highCutRight.reset();
}

void StereoDelay::setSettings (const Settings& settings) noexcept
{
    current = settings;
    current.feedback = std::clamp (current.feedback, 0.0f, 0.95f);
    targetDelaySamples = std::clamp (static_cast<float> (static_cast<double> (current.delaySeconds) * sampleRate),
                                     FractionalDelayLine::minimumDelay,
                                     static_cast<float> (maxDelaySeconds * sampleRate));
    lowCutLeft.setCutoff (current.lowCutHz);
    lowCutRight.setCutoff (current.lowCutHz);
    highCutLeft.setCutoff (current.highCutHz);
    highCutRight.setCutoff (current.highCutHz);
}

void StereoDelay::process (std::span<const float> inLeft,
                           std::span<const float> inRight,
                           std::span<float> outLeft,
                           std::span<float> outRight) noexcept
{
    const auto count = std::min ({ inLeft.size(), inRight.size(), outLeft.size(), outRight.size() });

    if (snapToTarget)
    {
        smoothedDelaySamples = targetDelaySamples;
        snapToTarget = false;
    }

    for (std::size_t n = 0; n < count; ++n)
    {
        smoothedDelaySamples += smoothing * (targetDelaySamples - smoothedDelaySamples);

        // Lesen vor dem Schreiben: Das Echo ist der Eingang von vor delay Samples plus Rückkopplung.
        const auto echoLeft = highCutLeft.processSample (lowCutLeft.processSample (left.read (smoothedDelaySamples)));
        const auto echoRight =
            highCutRight.processSample (lowCutRight.processSample (right.read (smoothedDelaySamples)));

        if (current.pingPong)
        {
            // Eingang nur links hinein (Mono-Summe), Rückkopplung über Kreuz: L → R → L.
            left.push (0.5f * (inLeft[n] + inRight[n]) + current.feedback * echoRight);
            right.push (current.feedback * echoLeft);
        }
        else
        {
            left.push (inLeft[n] + current.feedback * echoLeft);
            right.push (inRight[n] + current.feedback * echoRight);
        }

        outLeft[n] = echoLeft;
        outRight[n] = echoRight;
    }
}

} // namespace fk::dsp::delay
