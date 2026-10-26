#include "fk/dsp/nonlinear/Waveshaper.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestSignals.h"

TEST_CASE ("Waveshaper: hartes Clipping schneidet exakt bei ±1 ab", "[dsp]")
{
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (3.0f, fk::dsp::nonlinear::ShapeType::hard) == 1.0f);
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (-3.0f, fk::dsp::nonlinear::ShapeType::hard) == -1.0f);
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (0.5f, fk::dsp::nonlinear::ShapeType::hard) == 0.5f);
}

TEST_CASE ("Waveshaper: weiche Sättigung bleibt unter 1 und ist bei kleinen Pegeln fast linear", "[dsp]")
{
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (10.0f, fk::dsp::nonlinear::ShapeType::soft) <=
             1.0f); // tanh(10) ist in float exakt 1
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (2.0f, fk::dsp::nonlinear::ShapeType::soft) < 0.97f);
    REQUIRE (fk::dsp::nonlinear::Waveshaper::shape (0.01f, fk::dsp::nonlinear::ShapeType::soft) ==
             Catch::Approx (0.01f).margin (1e-5f));
}

TEST_CASE ("Waveshaper: Röhrenkennlinie ist asymmetrisch", "[dsp]")
{
    const float positive = fk::dsp::nonlinear::Waveshaper::shape (1.0f, fk::dsp::nonlinear::ShapeType::tube);
    const float negative = fk::dsp::nonlinear::Waveshaper::shape (-1.0f, fk::dsp::nonlinear::ShapeType::tube);
    REQUIRE (positive != Catch::Approx (-negative));
}

TEST_CASE ("Waveshaper: Drive erhöht die Verzerrung, Ausgang skaliert den Pegel", "[dsp]")
{
    fk::dsp::nonlinear::Waveshaper shaper;
    shaper.setShape (fk::dsp::nonlinear::ShapeType::soft);
    shaper.setDriveDb (20.0f);
    shaper.setOutputDb (-6.0206f);

    auto signal = fk::tests::makeSine (440.0, 48000.0, 4800, 0.5f);
    shaper.process (signal);

    // Mit 20 dB Drive wird ein 0,5-Sinus praktisch zum Rechteck nahe 1, dann halbiert.
    REQUIRE (fk::tests::peak (signal) == Catch::Approx (0.5f).margin (0.01f));
    REQUIRE (fk::tests::rms (signal) > 0.45f); // Rechteck: RMS ≈ Spitze; Sinus wäre 0,35
}
