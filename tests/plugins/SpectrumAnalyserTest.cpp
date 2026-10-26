#include "fk/plugins/analysis/SpectrumAnalyser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

using fk::plugins::analysis::SpectrumAnalyser;

TEST_CASE ("SpectrumAnalyser: Sinus erscheint als Spitze im richtigen Bin, Stille bleibt leer", "[plugins][analysis]")
{
    constexpr double sampleRate = 48000.0;
    constexpr float frequencyHz = 1000.0f;
    SpectrumAnalyser analyser;
    analyser.prepare (sampleRate);

    REQUIRE_FALSE (analyser.update()); // noch keine Samples

    // Stereo-Sinus mit Vollaussteuerung in Blöcken zu 512 Samples, wie ein Host ihn liefert.
    juce::AudioBuffer<float> block (2, 512);
    double phase = 0.0;

    for (int i = 0; i < SpectrumAnalyser::fftSize / 512 + 1; ++i)
    {
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto value = static_cast<float> (std::sin (phase));
            block.setSample (0, sample, value);
            block.setSample (1, sample, value);
            phase += 2.0 * std::numbers::pi * frequencyHz / sampleRate;
        }

        analyser.push (block);
    }

    REQUIRE (analyser.update());

    const auto magnitudes = analyser.getMagnitudesDb();
    REQUIRE (magnitudes.size() == static_cast<std::size_t> (SpectrumAnalyser::numBins));

    int peakBin = 0;
    for (int bin = 1; bin < SpectrumAnalyser::numBins; ++bin)
        if (magnitudes[static_cast<std::size_t> (bin)] > magnitudes[static_cast<std::size_t> (peakBin)])
            peakBin = bin;

    REQUIRE (analyser.getFrequencyForBin (peakBin) == Catch::Approx (frequencyHz).margin (30.0));
    REQUIRE (magnitudes[static_cast<std::size_t> (peakBin)] == Catch::Approx (0.0).margin (1.5));
    REQUIRE (magnitudes[static_cast<std::size_t> (peakBin * 4)] < -40.0f); // weit weg vom Ton: Rauschteppich

    // Ohne neue Samples ändert sich nichts.
    REQUIRE_FALSE (analyser.update());
}
