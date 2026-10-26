#include "fk/dsp/analysis/TempoEstimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace fk::dsp::analysis
{

namespace
{
constexpr std::size_t hopSize = 512;
constexpr double preferredBpm = 120.0;
constexpr double preferenceWidthOctaves = 1.0; // Standardabweichung der Gewichtung in Oktaven

/// Anstieg der logarithmischen Blockenergie, je Block ein Wert.
std::vector<double> onsetStrength (std::span<const float> samples)
{
    const auto numBlocks = samples.size() / hopSize;
    std::vector<double> strength (numBlocks, 0.0);
    double previous = 0.0;

    for (std::size_t block = 0; block < numBlocks; ++block)
    {
        double energy = 0.0;

        for (std::size_t i = 0; i < hopSize; ++i)
        {
            const auto sample = static_cast<double> (samples[block * hopSize + i]);
            energy += sample * sample;
        }

        const auto level = std::log (1.0e-9 + energy / static_cast<double> (hopSize));
        strength[block] = block > 0 ? std::max (0.0, level - previous) : 0.0;
        previous = level;
    }

    // Leicht glätten (Dreieck über fünf Blöcke): Eine Schlagdauer liegt selten genau auf einer ganzen
    // Blockzahl; ohne Glättung verschmiert ihr Gipfel und das doppelte Tempo gewinnt.
    std::vector<double> smoothed (numBlocks, 0.0);
    constexpr double kernel[] = { 1.0, 2.0, 3.0, 2.0, 1.0 };

    for (std::size_t block = 0; block < numBlocks; ++block)
        for (std::size_t k = 0; k < 5; ++k)
            if (block + k >= 2 && block + k - 2 < numBlocks)
                smoothed[block] += kernel[k] / 9.0 * strength[block + k - 2];

    strength = std::move (smoothed);

    // Mittelwert abziehen: Nur Abweichungen tragen zur Korrelation bei.
    const auto mean = std::accumulate (strength.begin(), strength.end(), 0.0) /
                      static_cast<double> (std::max<std::size_t> (1, numBlocks));

    for (auto& value : strength)
        value -= mean;

    return strength;
}

double autocorrelation (const std::vector<double>& values, std::size_t lag)
{
    double sum = 0.0;

    for (std::size_t i = lag; i < values.size(); ++i)
        sum += values[i] * values[i - lag];

    return sum / static_cast<double> (values.size() - lag);
}
} // namespace

std::optional<double> TempoEstimator::estimate (std::span<const float> samples, double sampleRate)
{
    if (sampleRate <= 0.0 || static_cast<double> (samples.size()) < 4.0 * sampleRate)
        return std::nullopt;

    auto strength = onsetStrength (samples);
    const auto blocksPerSecond = sampleRate / static_cast<double> (hopSize);
    const auto minLag = static_cast<std::size_t> (std::floor (blocksPerSecond * 60.0 / maxBpm));
    const auto maxLag = static_cast<std::size_t> (std::ceil (blocksPerSecond * 60.0 / minBpm));

    if (maxLag + 2 >= strength.size() || minLag < 1)
        return std::nullopt;

    std::vector<double> scores (maxLag + 2, 0.0);
    std::vector<double> raw (maxLag + 2, 0.0);

    for (auto lag = minLag; lag <= maxLag + 1; ++lag)
    {
        const auto bpm = 60.0 * blocksPerSecond / static_cast<double> (lag);
        const auto octaves = std::log2 (bpm / preferredBpm) / preferenceWidthOctaves;
        raw[lag] = autocorrelation (strength, lag);
        scores[lag] = raw[lag] * std::exp (-0.5 * octaves * octaves);
    }

    const auto best = static_cast<std::size_t> (
        std::distance (scores.begin(),
                       std::max_element (scores.begin() + static_cast<std::ptrdiff_t> (minLag),
                                         scores.begin() + static_cast<std::ptrdiff_t> (maxLag + 1))));

    // Kein deutlicher Gipfel: Rauschen, Flächen, gesprochene Sprache. Gemessen an der Korrelation bei
    // Verzögerung 0 (der Gesamtenergie der Einsatzstärke): Ein echter Takt wiederholt einen guten Teil davon.
    constexpr double minimumPeriodicity = 0.15;
    const auto energy = autocorrelation (strength, 0);

    if (energy <= 0.0 || raw[best] < minimumPeriodicity * energy)
        return std::nullopt;

    // Parabel durch die drei Punkte um den Gipfel: Verzögerung zwischen zwei Blöcken.
    double lag = static_cast<double> (best);

    if (best > minLag && best <= maxLag)
    {
        const auto left = scores[best - 1];
        const auto centre = scores[best];
        const auto right = scores[best + 1];
        const auto denominator = left - 2.0 * centre + right;

        if (std::abs (denominator) > 1.0e-12)
            lag += 0.5 * (left - right) / denominator;
    }

    return 60.0 * blocksPerSecond / lag;
}

} // namespace fk::dsp::analysis
