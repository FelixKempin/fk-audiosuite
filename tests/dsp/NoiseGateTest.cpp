#include "fk/dsp/dynamics/NoiseGate.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestSignals.h"

using fk::dsp::dynamics::NoiseGate;

namespace
{
constexpr double sampleRate = 48000.0;

NoiseGate makeGate()
{
    NoiseGate gate;
    gate.prepare (sampleRate);
    gate.setThresholdDb (-40.0f);
    gate.setHysteresisDb (6.0f);
    gate.setAttackMs (1.0);
    gate.setHoldMs (20.0);
    gate.setReleaseMs (30.0);
    return gate;
}
} // namespace

TEST_CASE ("NoiseGate: schließt bei Rauschen unter der Schwelle und öffnet beim Spielen", "[dsp][amp]")
{
    auto gate = makeGate();

    auto quiet = fk::tests::makeSine (200.0, sampleRate, 9600, 0.001f); // -60 dB
    gate.process (quiet, quiet);
    REQUIRE_FALSE (gate.isOpen());
    const std::vector<float> quietTail (quiet.begin() + 4800, quiet.end());
    REQUIRE (fk::tests::peak (quietTail) < 0.0001f);

    auto loud = fk::tests::makeSine (200.0, sampleRate, 9600, 0.1f); // -20 dB
    gate.process (loud, loud);
    REQUIRE (gate.isOpen());
    const std::vector<float> loudTail (loud.begin() + 4800, loud.end());
    REQUIRE (fk::tests::peak (loudTail) == Catch::Approx (0.1f).margin (0.005f));
}

TEST_CASE ("NoiseGate: Hysterese hält zwischen den Schwellen den Zustand", "[dsp][amp]")
{
    auto gate = makeGate();

    // -43 dB liegt zwischen Schließen (-46) und Öffnen (-40): aus dem geschlossenen Zustand bleibt zu …
    auto between = fk::tests::makeSine (200.0, sampleRate, 9600, 0.00708f);
    gate.process (between, between);
    REQUIRE_FALSE (gate.isOpen());

    // … aus dem offenen Zustand bleibt es offen.
    auto loud = fk::tests::makeSine (200.0, sampleRate, 4800, 0.1f);
    gate.process (loud, loud);
    REQUIRE (gate.isOpen());
    auto betweenAgain = fk::tests::makeSine (200.0, sampleRate, 9600, 0.00708f);
    gate.process (betweenAgain, betweenAgain);
    REQUIRE (gate.isOpen());
}
