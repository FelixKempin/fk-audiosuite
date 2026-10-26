#include "fk/dsp/generators/SineOscillator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

namespace
{

constexpr double sampleRate = 48000.0;

// Zählt die Übergänge von negativ nach positiv. Ein Sinus hat davon genau einen pro Periode.
std::size_t countRisingZeroCrossings (const std::vector<float>& samples)
{
    std::size_t crossings = 0;

    for (std::size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f)
            ++crossings;

    return crossings;
}

} // namespace

TEST_CASE ("SineOscillator: beginnt bei Phase 0 mit Sample 0", "[dsp]")
{
    fk::dsp::generators::SineOscillator oscillator;
    oscillator.prepare (sampleRate);

    REQUIRE (oscillator.nextSample() == Catch::Approx (0.0f).margin (1e-6));
}

TEST_CASE ("SineOscillator: erreicht nach einer Viertelperiode den Scheitel 1", "[dsp]")
{
    fk::dsp::generators::SineOscillator oscillator;
    oscillator.prepare (sampleRate);
    oscillator.setFrequency (1000.0);

    // Eine Periode bei 1000 Hz und 48 kHz sind 48 Samples, ein Viertel also 12.
    std::vector<float> quarterPeriod (12);
    oscillator.process (quarterPeriod);

    REQUIRE (oscillator.nextSample() == Catch::Approx (1.0f).margin (1e-5));
}

TEST_CASE ("SineOscillator: erzeugt die eingestellte Frequenz", "[dsp]")
{
    fk::dsp::generators::SineOscillator oscillator;
    oscillator.prepare (sampleRate);
    oscillator.setFrequency (440.0);

    // Über zehn Sekunden zählen: Die Phase wird als Gleitkommazahl aufsummiert, deshalb
    // kann ein einzelner Nulldurchgang genau auf die Puffergrenze fallen. Ein Zähler Abweichung
    // bei 4400 Perioden ist eine Frequenzabweichung von 0,02 % und damit vernachlässigbar.
    constexpr double seconds = 10.0;
    std::vector<float> samples (static_cast<std::size_t> (sampleRate * seconds));
    oscillator.process (samples);

    const double measuredHz = static_cast<double> (countRisingZeroCrossings (samples)) / seconds;

    REQUIRE (measuredHz == Catch::Approx (440.0).margin (0.1));
}

TEST_CASE ("SineOscillator: alle Samples liegen in [-1, 1]", "[dsp]")
{
    fk::dsp::generators::SineOscillator oscillator;
    oscillator.prepare (sampleRate);
    oscillator.setFrequency (12345.0);

    std::vector<float> samples (4096);
    oscillator.process (samples);

    for (const float sample : samples)
    {
        REQUIRE (sample >= -1.0f);
        REQUIRE (sample <= 1.0f);
    }
}
