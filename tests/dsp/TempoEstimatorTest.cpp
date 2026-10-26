#include "fk/dsp/analysis/TempoEstimator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

using fk::dsp::analysis::TempoEstimator;

namespace
{
/// Klick-Spur: kurze, abklingende Rauschstöße auf jedem Schlag, dazwischen leises Rauschen.
std::vector<float> clickTrack (double bpm, double seconds, double sampleRate, bool accentOffbeats = false)
{
    std::vector<float> samples (static_cast<std::size_t> (seconds * sampleRate), 0.0f);
    std::mt19937 random (42);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    const auto beatSamples = 60.0 / bpm * sampleRate;

    for (auto& sample : samples)
        sample = 0.01f * noise (random);

    for (int beat = 0; beat * beatSamples < static_cast<double> (samples.size()); ++beat)
    {
        const auto start = static_cast<std::size_t> (beat * beatSamples);
        const float gain = accentOffbeats && beat % 2 == 1 ? 0.4f : 0.9f;

        for (std::size_t i = 0; i < 2000 && start + i < samples.size(); ++i)
            samples[start + i] += gain * noise (random) * std::exp (-static_cast<float> (i) / 300.0f);
    }

    return samples;
}
} // namespace

TEST_CASE ("TempoEstimator: erkennt das Tempo einer Klick-Spur", "[dsp][tempo]")
{
    for (const double bpm : { 92.0, 120.0, 128.0, 150.0 })
    {
        const auto samples = clickTrack (bpm, 12.0, 48000.0);
        const auto estimated = TempoEstimator::estimate (samples, 48000.0);
        INFO ("Tempo " << bpm);
        REQUIRE (estimated.has_value());
        REQUIRE (*estimated == Catch::Approx (bpm).margin (1.0));
    }
}

TEST_CASE ("TempoEstimator: Betonung jedes zweiten Schlags führt nicht zum halben Tempo", "[dsp][tempo]")
{
    const auto samples = clickTrack (110.0, 12.0, 44100.0, true);
    const auto estimated = TempoEstimator::estimate (samples, 44100.0);
    REQUIRE (estimated.has_value());
    REQUIRE (*estimated == Catch::Approx (110.0).margin (1.0));
}

TEST_CASE ("TempoEstimator: Rauschen und zu kurzes Material liefern kein Tempo", "[dsp][tempo]")
{
    std::vector<float> noise (48000 * 8);
    std::mt19937 random (7);
    std::uniform_real_distribution<float> distribution (-0.5f, 0.5f);

    for (auto& sample : noise)
        sample = distribution (random);

    REQUIRE_FALSE (TempoEstimator::estimate (noise, 48000.0).has_value());
    REQUIRE_FALSE (TempoEstimator::estimate (clickTrack (120.0, 2.0, 48000.0), 48000.0).has_value());
}
