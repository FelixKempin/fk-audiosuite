#include "fk/dsp/reverb/FdnReverb.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fk::dsp::reverb
{

namespace
{
// Leitungslängen in Millisekunden bei size = 1: teilerfremd zueinander, damit sich die Echos
// nicht zu einem Raster ordnen (gleiche Abstände klingen wie ein Kamm).
constexpr std::array<double, FdnReverb::numLines> lineMs { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.7, 73.1 };
constexpr std::array<double, 4> diffuserMs { 4.77, 3.59, 12.73, 9.31 };
constexpr float maxSize = 1.5f;
constexpr double maxPreDelayMs = 250.0;
constexpr double modulationRateHz = 0.37;
constexpr double maxModulationMs = 0.6;

// Vorzeichen für die beiden Ausgangssummen: orthogonal, damit links und rechts dekorreliert sind.
constexpr std::array<float, FdnReverb::numLines> leftSigns { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr std::array<float, FdnReverb::numLines> rightSigns { 1, 1, -1, -1, 1, 1, -1, -1 };
} // namespace

void FdnReverb::Allpass::prepare (std::size_t delaySamples)
{
    buffer.assign (std::max<std::size_t> (1, delaySamples), 0.0f);
    index = 0;
}

void FdnReverb::Allpass::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    index = 0;
}

float FdnReverb::Allpass::processSample (float input, float gain) noexcept
{
    // w(t) = x(t) + g·w(t−D);  y(t) = w(t−D) − g·w(t)
    const auto delayed = buffer[index];
    const auto w = input + gain * delayed;
    buffer[index] = w;
    index = index + 1 == buffer.size() ? 0 : index + 1;
    return delayed - gain * w;
}

void FdnReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    preDelay.prepare (static_cast<std::size_t> (maxPreDelayMs * 0.001 * sampleRate) + 2);

    for (std::size_t i = 0; i < diffusers.size(); ++i)
        diffusers[i].prepare (static_cast<std::size_t> (diffuserMs[i] * 0.001 * sampleRate));

    for (std::size_t i = 0; i < numLines; ++i)
    {
        lines[i].prepare (static_cast<std::size_t> ((lineMs[i] * static_cast<double> (maxSize) + maxModulationMs) *
                                                    0.001 * sampleRate) +
                          8);
        damping[i].prepare (sampleRate, filters::OnePole::Mode::lowPass, current.dampingHz);
    }

    lowCut.prepare (sampleRate, filters::OnePole::Mode::highPass, current.lowCutHz);
    setSettings (current);
    reset();
}

void FdnReverb::reset() noexcept
{
    preDelay.reset();
    lowCut.reset();

    for (auto& diffuser : diffusers)
        diffuser.reset();

    for (std::size_t i = 0; i < numLines; ++i)
    {
        lines[i].reset();
        damping[i].reset();
    }

    modulationPhase = 0.0;
}

void FdnReverb::setSettings (const Settings& settings) noexcept
{
    current = settings;
    current.size = std::clamp (current.size, 0.5f, maxSize);
    current.decaySeconds = std::clamp (current.decaySeconds, 0.2f, 20.0f);

    preDelaySamples = static_cast<std::size_t> (
        std::clamp (static_cast<double> (current.preDelayMs), 0.0, maxPreDelayMs) * 0.001 * sampleRate);
    lowCut.setCutoff (current.lowCutHz);
    modulationDepthSamples = static_cast<float> (static_cast<double> (std::clamp (current.modulation, 0.0f, 1.0f)) *
                                                 maxModulationMs * 0.001 * sampleRate);

    for (std::size_t i = 0; i < numLines; ++i)
    {
        lineLengths[i] = static_cast<float> (lineMs[i] * static_cast<double> (current.size) * 0.001 * sampleRate);
        // −60 dB nach decaySeconds: je Umlauf um die Leitungslänge der passende Bruchteil davon.
        lineGains[i] = static_cast<float> (std::pow (10.0,
                                                     -3.0 * static_cast<double> (lineLengths[i]) /
                                                         (static_cast<double> (current.decaySeconds) * sampleRate)));
        damping[i].setCutoff (current.dampingHz);
    }
}

void FdnReverb::process (std::span<const float> inLeft,
                         std::span<const float> inRight,
                         std::span<float> outLeft,
                         std::span<float> outRight) noexcept
{
    const auto count = std::min ({ inLeft.size(), inRight.size(), outLeft.size(), outRight.size() });
    const auto diffusionGain = 0.75f * std::clamp (current.diffusion, 0.0f, 1.0f);
    const auto phaseStep = 2.0 * std::numbers::pi * modulationRateHz / sampleRate;
    const auto width = std::clamp (current.width, 0.0f, 1.0f);
    constexpr auto householder = 2.0f / static_cast<float> (numLines);
    constexpr auto outputGain = 1.0f / 2.8f; // ≈ 1/√8: Summe von acht Leitungen auf Eingangspegel

    std::array<float, numLines> outputs {};

    for (std::size_t n = 0; n < count; ++n)
    {
        // Eingang: Mono-Summe, Vorverzögerung, Low-Cut, Diffusion.
        auto input = 0.5f * (inLeft[n] + inRight[n]);
        input = preDelaySamples > 0 ? preDelay.pushAndRead (input, preDelaySamples) : input;
        input = lowCut.processSample (input);

        for (auto& diffuser : diffusers)
            input = diffuser.processSample (input, diffusionGain);

        // Leitungen lesen (zwei davon moduliert), dämpfen, verlustbehaftet zurückführen.
        const auto modulation = static_cast<float> (std::sin (modulationPhase)) * modulationDepthSamples;
        modulationPhase += phaseStep;

        if (modulationPhase > 2.0 * std::numbers::pi)
            modulationPhase -= 2.0 * std::numbers::pi;

        float sum = 0.0f;

        for (std::size_t i = 0; i < numLines; ++i)
        {
            const auto length = lineLengths[i] + (i == 1 ? modulation : i == 5 ? -modulation : 0.0f);
            outputs[i] = damping[i].processSample (lines[i].read (length)) * lineGains[i];
            sum += outputs[i];
        }

        // Householder: y_i = x_i − 2/N · Σx. Orthogonal, mischt jede Leitung in jede.
        const auto mixTerm = householder * sum;

        for (std::size_t i = 0; i < numLines; ++i)
            lines[i].push (outputs[i] - mixTerm + (i % 2 == 0 ? input : -input));

        float left = 0.0f;
        float right = 0.0f;

        for (std::size_t i = 0; i < numLines; ++i)
        {
            left += leftSigns[i] * outputs[i];
            right += rightSigns[i] * outputs[i];
        }

        left *= outputGain;
        right *= outputGain;

        // Breite: Mitte bleibt, Seite skaliert.
        const auto mid = 0.5f * (left + right);
        const auto side = 0.5f * (left - right) * width;
        outLeft[n] = mid + side;
        outRight[n] = mid - side;
    }
}

} // namespace fk::dsp::reverb
