#include "fk/dsp/dynamics/Compressor.h"
#include "fk/dsp/dynamics/EnvelopeFollower.h"
#include "fk/dsp/dynamics/GainComputer.h"
#include "fk/dsp/dynamics/Limiter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestSignals.h"

namespace
{
constexpr double sampleRate = 48000.0;
} // namespace

TEST_CASE ("GainComputer: unter dem Threshold keine Absenkung, darüber nach Ratio", "[dsp]")
{
    fk::dsp::dynamics::GainComputer computer;
    computer.setThresholdDb (-20.0f);
    computer.setRatio (4.0f);
    computer.setKneeDb (0.0f);

    REQUIRE (computer.computeGainReductionDb (-30.0f) == Catch::Approx (0.0f));
    // 12 dB über Threshold bei Ratio 4: Ausgang steigt nur 3 dB → 9 dB Absenkung.
    REQUIRE (computer.computeGainReductionDb (-8.0f) == Catch::Approx (-9.0f));
}

TEST_CASE ("GainComputer: weiches Knie beginnt vor dem Threshold und ist stetig", "[dsp]")
{
    fk::dsp::dynamics::GainComputer computer;
    computer.setThresholdDb (-20.0f);
    computer.setRatio (2.0f);
    computer.setKneeDb (10.0f);

    REQUIRE (computer.computeGainReductionDb (-26.0f) == Catch::Approx (0.0f));
    REQUIRE (computer.computeGainReductionDb (-22.0f) < 0.0f);
    REQUIRE (computer.computeGainReductionDb (-22.0f) > computer.computeGainReductionDb (-18.0f));
    // Am Knie-Ende (+5 dB) muss die Kurve nahtlos in die Gerade übergehen: -0,5 * 5 = -2,5 dB.
    REQUIRE (computer.computeGainReductionDb (-15.0f) == Catch::Approx (-2.5f).margin (1e-4));
}

TEST_CASE ("EnvelopeFollower: steigt mit Attack, fällt mit Release", "[dsp]")
{
    fk::dsp::dynamics::EnvelopeFollower follower;
    follower.prepare (sampleRate);
    follower.setAttackMs (10.0);
    follower.setReleaseMs (100.0);

    // Nach einer Attack-Zeit (480 Samples) sind etwa 63 % erreicht.
    float value = 0.0f;
    for (int i = 0; i < 480; ++i)
        value = follower.processSample (1.0f);
    REQUIRE (value == Catch::Approx (0.632f).margin (0.01f));

    // Dann Stille: nach einer Release-Zeit ist noch etwa 37 % des Werts übrig.
    const float before = value;
    for (int i = 0; i < 4800; ++i)
        value = follower.processSample (0.0f);
    REQUIRE (value == Catch::Approx (before * 0.368f).margin (0.01f));
}

TEST_CASE ("Compressor: senkt einen lauten Sinus ab und lässt einen leisen in Ruhe", "[dsp]")
{
    fk::dsp::dynamics::Compressor compressor;
    compressor.prepare (sampleRate);
    compressor.setThresholdDb (-20.0f);
    compressor.setRatio (4.0f);
    compressor.setKneeDb (0.0f);
    compressor.setAttackMs (1.0);
    compressor.setReleaseMs (50.0);

    auto loudLeft = fk::tests::makeSine (440.0, sampleRate, 48000, 1.0f); // 0 dBFS Spitze
    auto loudRight = loudLeft;
    compressor.process (loudLeft, loudRight);

    // 20 dB über Threshold bei Ratio 4 → 15 dB Absenkung (eingeschwungen).
    REQUIRE (compressor.getCurrentGainReductionDb() == Catch::Approx (-15.0f).margin (0.5f));
    const std::vector<float> settled (loudLeft.begin() + 24000, loudLeft.end());
    REQUIRE (fk::tests::peak (settled) == Catch::Approx (0.178f).margin (0.02f));

    compressor.reset();
    auto quietLeft = fk::tests::makeSine (440.0, sampleRate, 4800, 0.01f); // -40 dBFS
    auto quietRight = quietLeft;
    compressor.process (quietLeft, quietRight);
    REQUIRE (compressor.getCurrentGainReductionDb() == Catch::Approx (0.0f).margin (1e-3f));
    REQUIRE (fk::tests::peak (quietLeft) == Catch::Approx (0.01f).margin (1e-4f));
}

TEST_CASE ("Limiter: kein Sample überschreitet den Ceiling, Latenz entspricht dem Lookahead", "[dsp]")
{
    fk::dsp::dynamics::Limiter limiter;
    limiter.prepare (sampleRate, 5.0);
    limiter.setCeilingDb (-1.0f);
    limiter.setReleaseMs (50.0);

    REQUIRE (limiter.getLatencySamples() == 240);

    // Stille, dann ein plötzlicher lauter Block (+6 dB), dann wieder Stille.
    std::vector<float> left (9600, 0.0f);
    for (std::size_t i = 2400; i < 4800; ++i)
        left[i] = (i % 40 < 20) ? 2.0f : -2.0f;
    auto right = left;

    limiter.process (left, right);

    const float ceilingLinear = std::pow (10.0f, -1.0f / 20.0f);
    REQUIRE (fk::tests::peak (left) <= ceilingLinear + 1e-4f);
    REQUIRE (fk::tests::peak (right) <= ceilingLinear + 1e-4f);
    REQUIRE (fk::tests::peak (left) > ceilingLinear * 0.9f); // und er limitiert, statt nur leise zu machen
}
