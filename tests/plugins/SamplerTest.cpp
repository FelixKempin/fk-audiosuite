#include "fk/plugins/SamplerProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

namespace
{
float peakOf (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = std::max (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return peak;
}

/// Ein kleines Instrument: 0,3 s Sinus als WAV plus eine SFZ-Datei, die ihn auf C3 legt.
struct TinyInstrument
{
    juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("FKAudioSuiteTests")
                            .getNonexistentChildFile ("sfz", "", false);
    juce::File sfz = folder.getChildFile ("tiny.sfz");

    TinyInstrument()
    {
        folder.createDirectory();
        const auto wav = folder.getChildFile ("sine.wav");
        constexpr double sampleRate = 48000.0;
        juce::AudioBuffer<float> sine (1, static_cast<int> (sampleRate * 0.3));

        for (int i = 0; i < sine.getNumSamples(); ++i)
            sine.setSample (
                0, i, 0.8f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 440.0 * i / sampleRate)));

        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (wav);
        auto writer = format.createWriterFor (
            stream,
            juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16));
        writer->writeFromAudioSampleBuffer (sine, 0, sine.getNumSamples());
        writer.reset();

        sfz.replaceWithText ("<region> sample=sine.wav key=60 loop_mode=no_loop ampeg_release=0.01\n");
    }

    ~TinyInstrument() { folder.deleteRecursively(); }
};
} // namespace

TEST_CASE ("FK Sampler: lädt SFZ, spielt Note und speichert die Datei im Zustand", "[plugins][sampler]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TinyInstrument instrument;
    fk::plugins::SamplerProcessor sampler;

    REQUIRE_FALSE (sampler.loadSfz (instrument.folder.getChildFile ("fehlt.sfz")).has_value());
    REQUIRE (sampler.getNumRegions() == 0);

    REQUIRE (sampler.loadSfz (instrument.sfz).has_value());
    REQUIRE (sampler.getNumRegions() == 1);
    REQUIRE (sampler.getSfzFile() == instrument.sfz);

    sampler.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);

    float loudest = 0.0f;
    for (int block = 0; block < 10; ++block)
    {
        sampler.processBlock (buffer, midi);
        midi.clear();
        loudest = std::max (loudest, peakOf (buffer));
    }
    REQUIRE (loudest > 0.1f);

    // Note loslassen, kurze Release: danach Stille.
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    for (int block = 0; block < 20; ++block)
    {
        sampler.processBlock (buffer, midi);
        midi.clear();
    }
    REQUIRE (peakOf (buffer) < 1.0e-3f);

    // Zustand speichern und in eine neue Instanz laden: Die Datei kommt mit.
    juce::MemoryBlock state;
    sampler.getStateInformation (state);

    fk::plugins::SamplerProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    REQUIRE (restored.getSfzFile() == instrument.sfz);
    REQUIRE (restored.getNumRegions() == 1);

    sampler.unload();
    REQUIRE (sampler.getNumRegions() == 0);
    REQUIRE (sampler.getSfzFile() == juce::File());
}
