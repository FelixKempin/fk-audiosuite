#include "fk/plugins/SubtractiveSynthProcessor.h"

#include <juce_events/juce_events.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return peak;
}
} // namespace

TEST_CASE ("FK Synth: spielt auf Note-On einen Ton und verstummt nach Note-Off", "[plugins]")
{
    const juce::ScopedJuceInitialiser_GUI
        juceInitialiser; // AudioProcessor braucht JUCEs Laufzeit (Timer, MessageManager)
    fk::plugins::SubtractiveSynthProcessor synth;
    synth.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);

    // Ein paar Blöcke mit gehaltener Note: Pegel muss hörbar sein.
    float loudest = 0.0f;
    for (int block = 0; block < 10; ++block)
    {
        synth.processBlock (buffer, midi);
        midi.clear();
        loudest = std::max (loudest, peakOf (buffer));
    }
    REQUIRE (loudest > 0.05f);

    // Note loslassen, Release abwarten (200 ms = ~19 Blöcke), dann Stille.
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    for (int block = 0; block < 60; ++block)
    {
        synth.processBlock (buffer, midi);
        midi.clear();
    }
    REQUIRE (peakOf (buffer) < 1.0e-4f);
}

TEST_CASE ("FK Synth: ist als Instrument beschrieben", "[plugins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fk::plugins::SubtractiveSynthProcessor synth;
    juce::PluginDescription description;
    synth.fillInPluginDescription (description);

    REQUIRE (description.isInstrument);
    REQUIRE (description.numInputChannels == 0);
    REQUIRE (synth.acceptsMidi());
}

namespace
{
/// Zählt Nulldurchgänge: ein grobes, aber verlässliches Maß für die Tonhöhe eines Oszillators.
int countZeroCrossings (const juce::AudioBuffer<float>& buffer)
{
    int crossings = 0;
    const auto* data = buffer.getReadPointer (0);

    for (int i = 1; i < buffer.getNumSamples(); ++i)
        if ((data[i - 1] < 0.0f) != (data[i] < 0.0f))
            ++crossings;

    return crossings;
}

int zeroCrossingsWith (const juce::MidiMessage& extra)
{
    fk::plugins::SubtractiveSynthProcessor synth;
    synth.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (extra, 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 1);
    int crossings = 0;

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        synth.processBlock (buffer, midi);
        midi.clear();

        if (block >= 10) // Einschwingen der Hüllkurve überspringen
            crossings += countZeroCrossings (buffer);
    }

    return crossings;
}
} // namespace

TEST_CASE ("FK Synth: Pitch-Bend hebt die Tonhöhe, Mod-Wheel ohne Bend lässt sie im Mittel", "[plugins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser; // AudioProcessor braucht JUCEs Laufzeit (Timer)
    const int neutral = zeroCrossingsWith (juce::MidiMessage::pitchWheel (1, 8192));
    const int bentUp = zeroCrossingsWith (juce::MidiMessage::pitchWheel (1, 16383)); // +2 Halbtöne ≈ +12 %
    const int bentDown = zeroCrossingsWith (juce::MidiMessage::pitchWheel (1, 0));   // -2 Halbtöne ≈ -11 %
    const int vibrato = zeroCrossingsWith (juce::MidiMessage::controllerEvent (1, 1, 127));

    REQUIRE (neutral > 100);
    REQUIRE (bentUp > neutral * 1.08);
    REQUIRE (bentDown < neutral * 0.92);
    REQUIRE (std::abs (vibrato - neutral) < neutral * 0.03); // Vibrato schwankt um die Mitte
}
