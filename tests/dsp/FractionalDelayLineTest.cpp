#include "fk/dsp/delay/FractionalDelayLine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

using fk::dsp::delay::FractionalDelayLine;

TEST_CASE ("FractionalDelayLine: ganzzahlige Verzögerung liefert das alte Sample exakt", "[dsp][waveguide]")
{
    FractionalDelayLine line;
    line.prepare (64);

    for (int i = 0; i < 100; ++i)
        line.push (static_cast<float> (i));

    REQUIRE (line.read (1.0f) == 98.0f);
    REQUIRE (line.read (5.0f) == 94.0f);
    REQUIRE (line.read (64.0f) == 35.0f);
}

TEST_CASE ("FractionalDelayLine: ein Polynom dritten Grades wird zwischen den Samples exakt getroffen",
           "[dsp][waveguide]")
{
    const auto polynomial = [] (double t) { return 0.5 + 0.01 * t - 0.0003 * t * t + 0.00001 * t * t * t; };
    FractionalDelayLine line;
    line.prepare (32);

    for (int i = 0; i <= 20; ++i)
        line.push (static_cast<float> (polynomial (i)));

    // Zeitpunkt des zuletzt geschriebenen Samples ist 20.
    REQUIRE (line.read (3.37f) == Catch::Approx (polynomial (20.0 - 3.37)).margin (1e-6));
    REQUIRE (line.read (7.5f) == Catch::Approx (polynomial (20.0 - 7.5)).margin (1e-6));
}

TEST_CASE ("FractionalDelayLine: ein Sinus wird um Bruchteile eines Samples verschoben", "[dsp][waveguide]")
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 1000.0;
    constexpr float delay = 10.25f;
    FractionalDelayLine line;
    line.prepare (64);
    double worstError = 0.0;

    for (int n = 0; n < 2000; ++n)
    {
        const auto sine = [&] (double t) { return std::sin (2.0 * std::numbers::pi * frequency * t / sampleRate); };
        line.push (static_cast<float> (sine (n)));

        if (n > 64)
            worstError = std::max (worstError, std::abs (line.read (delay) - sine (n - static_cast<double> (delay))));
    }

    REQUIRE (worstError < 1.0e-3);
}

TEST_CASE ("FractionalDelayLine: Verzögerungen außerhalb des Bereichs werden begrenzt", "[dsp][waveguide]")
{
    FractionalDelayLine line;
    line.prepare (16);

    for (int i = 0; i < 40; ++i)
        line.push (static_cast<float> (i));

    REQUIRE (line.read (0.2f) == line.read (FractionalDelayLine::minimumDelay));
    REQUIRE (line.read (500.0f) == line.read (16.0f));
}
