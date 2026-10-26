#include "fk/dsp/units/Decibels.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Decibels: 0 dB ist Faktor 1, -6 dB etwa die Hälfte, +20 dB das Zehnfache", "[dsp]")
{
    REQUIRE (fk::dsp::units::decibelsToGain (0.0f) == Catch::Approx (1.0f));
    REQUIRE (fk::dsp::units::decibelsToGain (-6.0206f) == Catch::Approx (0.5f).margin (1e-4));
    REQUIRE (fk::dsp::units::decibelsToGain (20.0f) == Catch::Approx (10.0f));
}

TEST_CASE ("Decibels: Hin- und Rückweg sind konsistent, Stille bleibt Stille", "[dsp]")
{
    REQUIRE (fk::dsp::units::gainToDecibels (fk::dsp::units::decibelsToGain (-12.5f)) ==
             Catch::Approx (-12.5f).margin (1e-4));
    REQUIRE (fk::dsp::units::decibelsToGain (fk::dsp::units::minusInfinityDb) == 0.0f);
    REQUIRE (fk::dsp::units::gainToDecibels (0.0f) == fk::dsp::units::minusInfinityDb);
}
