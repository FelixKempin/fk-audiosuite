#include "fk/plugins/DelayProcessor.h"
#include "fk/plugins/GateProcessor.h"
#include "fk/plugins/ReverbProcessor.h"

#include <juce_events/juce_events.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
void setValue (fk::plugins::InternalPlugin& plugin, const juce::String& id, float value)
{
    auto* parameter = plugin.getParameterState().getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void fillSine (juce::AudioBuffer<float>& buffer, float amplitude, int& phase)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i, ++phase)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (
                channel, i, amplitude * std::sin (2.0f * 3.14159265f * 440.0f * static_cast<float> (phase) / 48000.0f));
}
} // namespace

TEST_CASE ("FK Gate: schließt bei leisem Signal, Range begrenzt die Absenkung", "[plugins][a6]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fk::plugins::GateProcessor gate;
    setValue (gate, "threshold", -30.0f);
    setValue (gate, "release", 20.0f);
    setValue (gate, "hold", 0.0f);
    gate.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    int phase = 0;

    // Laut (−6 dB): offen, Signal unverändert.
    for (int block = 0; block < 20; ++block)
    {
        fillSine (buffer, 0.5f, phase);
        gate.processBlock (buffer, midi);
    }

    REQUIRE (buffer.getMagnitude (0, 0, 512) == Catch::Approx (0.5f).margin (0.02f));

    // Leise (−46 dB): geschlossen, praktisch stumm.
    for (int block = 0; block < 40; ++block)
    {
        fillSine (buffer, 0.005f, phase);
        gate.processBlock (buffer, midi);
    }

    REQUIRE (buffer.getMagnitude (0, 0, 512) < 0.0001f);
    REQUIRE (gate.getGainReductionDb() < -60.0f);

    // Range −12 dB: nur um 12 dB leiser.
    setValue (gate, "range", -12.0f);

    for (int block = 0; block < 10; ++block)
    {
        fillSine (buffer, 0.005f, phase);
        gate.processBlock (buffer, midi);
    }

    REQUIRE (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, 0, 512) / 0.005f) ==
             Catch::Approx (-12.0f).margin (0.5f));
}

TEST_CASE ("FK Delay: Tempo-Sync ohne Host 120 BPM, 1/8 punktiert = 375 ms; Echo an der richtigen Stelle",
           "[plugins][a6]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fk::plugins::DelayProcessor delay;
    setValue (delay, "mix", 100.0f);
    setValue (delay, "feedback", 0.0f);
    setValue (delay, "lowcut", 20.0f);
    setValue (delay, "highcut", 20000.0f);
    delay.prepareToPlay (48000.0, 512);

    REQUIRE (fk::plugins::DelayProcessor::noteLengthInQuarters (6) == Catch::Approx (0.75));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    int echoAt = -1;

    for (int block = 0; block < 50 && echoAt < 0; ++block)
    {
        buffer.clear();

        if (block == 0)
        {
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
        }

        delay.processBlock (buffer, midi);

        for (int i = 0; i < 512; ++i)
            if (std::abs (buffer.getSample (0, i)) > 0.3f)
            {
                echoAt = block * 512 + i;
                break;
            }
    }

    REQUIRE (delay.getDelaySeconds() == Catch::Approx (0.375f));
    REQUIRE (echoAt == Catch::Approx (0.375 * 48000.0).margin (3.0));
}

TEST_CASE ("FK Reverb: Mix 0 lässt das Signal unverändert, Mix 100 klingt nach", "[plugins][a6]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fk::plugins::ReverbProcessor reverb;
    setValue (reverb, "mix", 0.0f);
    reverb.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    int phase = 0;
    fillSine (buffer, 0.5f, phase);
    juce::AudioBuffer<float> original (buffer);
    reverb.processBlock (buffer, midi);

    for (int i = 0; i < 512; ++i)
        REQUIRE (buffer.getSample (0, i) == Catch::Approx (original.getSample (0, i)).margin (1.0e-6));

    setValue (reverb, "mix", 100.0f);
    setValue (reverb, "decay", 2.0f);
    REQUIRE (reverb.getTailLengthSeconds() == Catch::Approx (2.02).margin (0.01));

    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    reverb.processBlock (buffer, midi);

    // Eine halbe Sekunde später ist noch Hall da.
    float tail = 0.0f;

    for (int block = 0; block < 47; ++block)
    {
        buffer.clear();
        reverb.processBlock (buffer, midi);
        tail = buffer.getMagnitude (0, 0, 512);
    }

    REQUIRE (tail > 0.0005f);
}
