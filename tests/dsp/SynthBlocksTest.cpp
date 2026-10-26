#include "fk/dsp/envelopes/Adsr.h"
#include "fk/dsp/filters/StateVariableFilter.h"
#include "fk/dsp/generators/Oscillator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestSignals.h"

namespace
{
constexpr double sampleRate = 48000.0;

std::vector<float> render (fk::dsp::generators::Oscillator& oscillator, std::size_t numSamples)
{
    std::vector<float> out (numSamples);
    for (auto& s : out)
        s = oscillator.nextSample();
    return out;
}

std::size_t countRisingZeroCrossings (const std::vector<float>& samples)
{
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f)
            ++crossings;
    return crossings;
}
} // namespace

TEST_CASE ("Oscillator: Sägezahn und Rechteck haben die eingestellte Frequenz und bleiben in [-1, 1]", "[dsp]")
{
    for (const auto waveform : { fk::dsp::generators::Waveform::saw,
                                 fk::dsp::generators::Waveform::square,
                                 fk::dsp::generators::Waveform::sine })
    {
        fk::dsp::generators::Oscillator oscillator;
        oscillator.prepare (sampleRate);
        oscillator.setWaveform (waveform);
        oscillator.setFrequency (220.0);

        const auto samples = render (oscillator, static_cast<std::size_t> (sampleRate * 10.0));
        REQUIRE (static_cast<double> (countRisingZeroCrossings (samples)) / 10.0 == Catch::Approx (220.0).margin (0.2));
        REQUIRE (fk::tests::peak (samples) <= 1.05f); // PolyBLEP darf minimal überschwingen
    }
}

TEST_CASE ("Oscillator: PolyBLEP-Sägezahn hat weniger Energie über 10 kHz als der naive", "[dsp]")
{
    // Grober Aliasing-Test: Hochpass-gefilterte Energie des bandbegrenzten Sägezahns muss klein sein.
    fk::dsp::generators::Oscillator oscillator;
    oscillator.prepare (sampleRate);
    oscillator.setWaveform (fk::dsp::generators::Waveform::saw);
    oscillator.setFrequency (3000.0);
    auto samples = render (oscillator, 48000);

    // Erste Differenz als sehr grober Hochpass: naiver Sägezahn hätte Sprünge von 2.0.
    float maxJump = 0.0f;
    for (std::size_t i = 1; i < samples.size(); ++i)
        maxJump = std::max (maxJump, std::abs (samples[i] - samples[i - 1]));

    REQUIRE (maxJump < 1.5f);
}

TEST_CASE ("Adsr: durchläuft Attack, Decay, Sustain und Release mit den eingestellten Zeiten", "[dsp]")
{
    fk::dsp::envelopes::Adsr envelope;
    envelope.prepare (sampleRate);
    envelope.setAttackMs (10.0); // 480 Samples
    envelope.setDecayMs (20.0);  // 960 Samples bis Sustain
    envelope.setSustainLevel (0.5f);
    envelope.setReleaseMs (10.0);

    REQUIRE_FALSE (envelope.isActive());
    envelope.noteOn();

    float value = 0.0f;
    for (int i = 0; i < 480; ++i)
        value = envelope.nextValue();
    REQUIRE (value == Catch::Approx (1.0f).margin (0.01f));
    REQUIRE (envelope.getStage() == fk::dsp::envelopes::Adsr::Stage::decay);

    for (int i = 0; i < 960; ++i)
        value = envelope.nextValue();
    REQUIRE (value == Catch::Approx (0.5f).margin (0.01f));
    REQUIRE (envelope.getStage() == fk::dsp::envelopes::Adsr::Stage::sustain);

    envelope.noteOff();
    for (int i = 0; i < 480; ++i)
        value = envelope.nextValue();
    REQUIRE (value == Catch::Approx (0.0f).margin (0.01f));
    REQUIRE_FALSE (envelope.isActive());
}

TEST_CASE ("StateVariableFilter: Tiefpass dämpft hohe Frequenzen, Resonanz hebt am Cutoff an", "[dsp]")
{
    auto gainDb = [] (double cutoff, float resonance, double testHz)
    {
        fk::dsp::filters::StateVariableFilter filter;
        filter.prepare (sampleRate);
        filter.setCutoff (cutoff, resonance);
        auto signal = fk::tests::makeSine (testHz, sampleRate, 48000);
        const auto inRms = fk::tests::rms (signal);
        filter.process (signal);
        const std::vector<float> settled (signal.begin() + 24000, signal.end());
        return 20.0f * std::log10 (fk::tests::rms (settled) / inRms);
    };

    REQUIRE (gainDb (1000.0, 0.0f, 100.0) == Catch::Approx (0.0f).margin (0.2f));
    REQUIRE (gainDb (1000.0, 0.0f, 10000.0) < -30.0f);
    REQUIRE (gainDb (1000.0, 0.9f, 1000.0) > 6.0f);
}
