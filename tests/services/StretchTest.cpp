#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
/// Mono-WAV mit Klicks auf jedem Schlag (wie ein Drum-Loop ohne Instrumente).
juce::File createClickWav (TestEnvironment& env, const juce::String& name, double bpm, double seconds)
{
    constexpr double sampleRate = 48000.0;
    const auto file = env.tempFolder.getChildFile (name + ".wav");
    file.getParentDirectory().createDirectory();
    juce::AudioBuffer<float> buffer (1, static_cast<int> (seconds * sampleRate));
    buffer.clear();
    std::mt19937 random (1);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    const auto beatSamples = 60.0 / bpm * sampleRate;

    for (int beat = 0; beat * beatSamples < buffer.getNumSamples(); ++beat)
        for (int i = 0; i < 2000 && static_cast<int> (beat * beatSamples) + i < buffer.getNumSamples(); ++i)
            buffer.setSample (0,
                              static_cast<int> (beat * beatSamples) + i,
                              0.8f * noise (random) * std::exp (-static_cast<float> (i) / 300.0f));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = format.createWriterFor (
        stream,
        juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (24));
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return file;
}
} // namespace

TEST_CASE ("ClipService: Tempo erkennen und dem Projekttempo folgen", "[services][a5]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Tempo").has_value());
    env.timelineService.setTempoBpm (120.0);
    auto* track = *env.trackService.createAudioTrack ("Loop", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, createClickWav (env, "loop100", 100.0, 9.6), 0.0);

    const auto bpm = env.clipService.detectTempo (clipId);
    REQUIRE (bpm.has_value());
    REQUIRE (*bpm == Catch::Approx (100.0).margin (1.0));

    // Folgt dem Tempo: 16 Schläge bei 100 BPM = 9,6 s; bei 120 BPM dauern sie 8 s.
    REQUIRE (env.clipService.setFollowsTempo ({ clipId }, true, 100.0).has_value());
    TestEnvironment::pumpMessages (100);
    auto clip = env.trackService.getClips (*track).front();
    REQUIRE (clip.followsTempo);
    REQUIRE (clip.sourceBpm == Catch::Approx (100.0).margin (0.1));
    REQUIRE (clip.lengthSeconds == Catch::Approx (8.0).margin (0.05));

    REQUIRE (env.clipService.setFollowsTempo ({ clipId }, false).has_value());
    REQUIRE_FALSE (env.trackService.getClips (*track).front().followsTempo);
    REQUIRE (env.trackService.getClips (*track).front().lengthSeconds == Catch::Approx (9.6).margin (0.05));

    // Ohne Rhythmus kein Tempo: dann muss man es eingeben.
    const auto silent = *env.clipService.importAudioFile (*track, env.createSilentWav ("still", 6.0), 20.0);
    REQUIRE_FALSE (env.clipService.detectTempo (silent).has_value());
    REQUIRE_FALSE (env.clipService.setFollowsTempo ({ silent }, true).has_value());
}

TEST_CASE ("ClipService: Tonhöhe ändern und Clip strecken", "[services][a5]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Strecken").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, createClickWav (env, "klick", 120.0, 4.0), 1.0);
    const auto clip = [&] { return env.trackService.getClips (*track).front(); };

    REQUIRE (env.clipService.setPitch ({ clipId }, 3.0f).has_value());
    REQUIRE (clip().pitchSemitones == Catch::Approx (3.0f));
    REQUIRE (clip().lengthSeconds == Catch::Approx (4.0));
    REQUIRE (env.clipService.setPitch ({ clipId }, 40.0f).has_value());
    REQUIRE (clip().pitchSemitones == Catch::Approx (24.0f)); // begrenzt

    REQUIRE (env.clipService.stretchClip (clipId, 8.0).has_value());
    REQUIRE (clip().lengthSeconds == Catch::Approx (8.0));
    REQUIRE (clip().speedRatio == Catch::Approx (0.5));
    REQUIRE (clip().startSeconds == Catch::Approx (1.0));
    REQUIRE_FALSE (env.clipService.stretchClip (clipId, 100.0).has_value()); // mehr als vierfach

    env.projectService.undo();
    REQUIRE (clip().lengthSeconds == Catch::Approx (4.0));
    REQUIRE (clip().speedRatio == Catch::Approx (1.0));
}
