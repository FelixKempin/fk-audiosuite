#include "fk/dsp/filters/Biquad.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestSignals.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr std::size_t numSamples = 48000;

// Verhältnis der Ausgangs- zur Eingangslautstärke in dB für eine Frequenz.
float gainAtFrequencyDb (
    fk::dsp::filters::FilterType type, double cutoffHz, double q, double gainDb, double testFrequencyHz)
{
    fk::dsp::filters::Biquad<float> filter;
    filter.setCoefficients (fk::dsp::filters::Biquad<float>::makeCoefficients (type, sampleRate, cutoffHz, q, gainDb));

    auto signal = fk::tests::makeSine (testFrequencyHz, sampleRate, numSamples);
    const auto inputRms = fk::tests::rms (signal);

    filter.process (signal);

    // Einschwingen abschneiden: nur die zweite Hälfte messen.
    const std::vector<float> settled (signal.begin() + static_cast<long> (numSamples / 2), signal.end());
    return 20.0f * std::log10 (fk::tests::rms (settled) / inputRms);
}
} // namespace

TEST_CASE ("Biquad: Tiefpass lässt tiefe Frequenzen durch und dämpft hohe", "[dsp]")
{
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::lowPass, 1000.0, 0.707, 0.0, 100.0) ==
             Catch::Approx (0.0).margin (0.1));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::lowPass, 1000.0, 0.707, 0.0, 1000.0) ==
             Catch::Approx (-3.0).margin (0.3));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::lowPass, 1000.0, 0.707, 0.0, 10000.0) < -35.0);
}

TEST_CASE ("Biquad: Hochpass ist das Spiegelbild", "[dsp]")
{
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::highPass, 1000.0, 0.707, 0.0, 10000.0) ==
             Catch::Approx (0.0).margin (0.1));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::highPass, 1000.0, 0.707, 0.0, 100.0) < -35.0);
}

TEST_CASE ("Biquad: Glockenfilter hebt an der Mittenfrequenz um den eingestellten Wert an", "[dsp]")
{
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::peak, 2000.0, 1.0, 6.0, 2000.0) ==
             Catch::Approx (6.0).margin (0.2));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::peak, 2000.0, 1.0, 6.0, 100.0) ==
             Catch::Approx (0.0).margin (0.2));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::peak, 2000.0, 1.0, -9.0, 2000.0) ==
             Catch::Approx (-9.0).margin (0.2));
}

TEST_CASE ("Biquad: Shelf-Filter wirken jenseits der Eckfrequenz voll", "[dsp]")
{
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::lowShelf, 500.0, 0.707, 4.0, 40.0) ==
             Catch::Approx (4.0).margin (0.3));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::lowShelf, 500.0, 0.707, 4.0, 8000.0) ==
             Catch::Approx (0.0).margin (0.3));
    REQUIRE (gainAtFrequencyDb (fk::dsp::filters::FilterType::highShelf, 4000.0, 0.707, -5.0, 15000.0) ==
             Catch::Approx (-5.0).margin (0.4));
}

TEST_CASE ("Biquad: funktioniert mit double genauso", "[dsp]")
{
    fk::dsp::filters::Biquad<double> filter;
    filter.setCoefficients (fk::dsp::filters::Biquad<double>::makeCoefficients (
        fk::dsp::filters::FilterType::lowPass, sampleRate, 1000.0, 0.707));

    std::vector<double> impulse (64, 0.0);
    impulse[0] = 1.0;
    filter.process (impulse);

    // Erste Antwort eines Tiefpasses ist positiv und kleiner als der Impuls.
    REQUIRE (impulse[0] > 0.0);
    REQUIRE (impulse[0] < 1.0);
}

TEST_CASE ("Biquad: magnitudeDb stimmt mit dem gemessenen Frequenzgang überein", "[dsp]")
{
    using fk::dsp::filters::Biquad;
    using fk::dsp::filters::FilterType;

    const auto peak = Biquad<float>::makeCoefficients (FilterType::peak, sampleRate, 1000.0, 1.0, 6.0);
    REQUIRE (Biquad<float>::magnitudeDb (peak, 1000.0, sampleRate) == Catch::Approx (6.0).margin (0.05));
    REQUIRE (Biquad<float>::magnitudeDb (peak, 20.0, sampleRate) == Catch::Approx (0.0).margin (0.05));
    REQUIRE (Biquad<float>::magnitudeDb (peak, 1000.0, sampleRate) ==
             Catch::Approx (gainAtFrequencyDb (FilterType::peak, 1000.0, 1.0, 6.0, 1000.0)).margin (0.2));

    const auto lowPass = Biquad<float>::makeCoefficients (FilterType::lowPass, sampleRate, 1000.0, 0.707);
    REQUIRE (Biquad<float>::magnitudeDb (lowPass, 1000.0, sampleRate) == Catch::Approx (-3.0).margin (0.1));
    REQUIRE (Biquad<float>::magnitudeDb (lowPass, 10000.0, sampleRate) < -35.0);
}
