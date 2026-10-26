#include "fk/services/ProjectSettingsService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
/// Mono-WAV mit einem 440-Hz-Sinus der gegebenen Spitze.
juce::File createSineWav (TestEnvironment& env, const juce::String& name, double seconds, float peak)
{
    constexpr double sampleRate = 48000.0;
    const auto file = env.tempFolder.getChildFile (name + ".wav");
    file.getParentDirectory().createDirectory();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = format.createWriterFor (
        stream,
        juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (24));

    juce::AudioBuffer<float> buffer (1, static_cast<int> (seconds * sampleRate));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (
            0,
            i,
            peak * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate)));

    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return file;
}

float firstSampleAfter (const juce::File& file, int index)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    juce::AudioBuffer<float> buffer (1, index + 1);
    reader->read (&buffer, 0, index + 1, 0, true, false);
    return buffer.getSample (0, index);
}
} // namespace

TEST_CASE ("ClipService: Normalisieren setzt das Clip-Gain auf die Spitze", "[services][a4]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Normalisieren").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, createSineWav (env, "halb", 1.0, 0.5f), 0.0);

    REQUIRE (env.clipService.normalizeClips ({ clipId }).has_value());
    REQUIRE (env.trackService.getClips (*track).front().gainDb == Catch::Approx (6.02).margin (0.05));

    REQUIRE (env.clipService.normalizeClips ({ clipId }, -6.0f).has_value());
    REQUIRE (env.trackService.getClips (*track).front().gainDb == Catch::Approx (0.0).margin (0.05));

    // Stille lässt sich nicht normalisieren.
    const auto silentId = *env.clipService.importAudioFile (*track, env.createSilentWav ("still", 1.0), 3.0);
    REQUIRE_FALSE (env.clipService.normalizeClips ({ silentId }).has_value());
}

TEST_CASE ("ClipService: Umkehren und Phase umkehren, mit Undo", "[services][a4]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Phase").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gitarre", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, createSineWav (env, "sinus", 1.0, 0.5f), 1.0);
    const auto clip = [&] { return env.trackService.getClips (*track).front(); };
    const auto originalFile = clip().audioFile;

    REQUIRE (env.clipService.setClipsReversed ({ clipId }, true).has_value());
    REQUIRE (clip().reversed);
    REQUIRE_FALSE (env.clipService.invertPhase ({ clipId }).has_value()); // rückwärts: abgelehnt
    REQUIRE (env.clipService.setClipsReversed ({ clipId }, false).has_value());

    REQUIRE (env.clipService.invertPhase ({ clipId }).has_value());
    const auto inverted = clip().audioFile;
    REQUIRE (inverted != originalFile);
    REQUIRE (inverted.isAChildOf (env.projectService.getProjectFolder()));
    REQUIRE (clip().startSeconds == Catch::Approx (1.0));
    REQUIRE (clip().offsetSeconds == Catch::Approx (0.0));
    REQUIRE (firstSampleAfter (inverted, 10) == Catch::Approx (-firstSampleAfter (originalFile, 10)).margin (1.0e-4));

    env.projectService.undo();
    REQUIRE (clip().audioFile == originalFile);
}

TEST_CASE ("ProjectSettingsService: überlappende Clips blenden automatisch über", "[services][a4]")
{
    TestEnvironment env;
    fk::services::ProjectSettingsService settings (env.engineContext, env.projectService);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Crossfade").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    REQUIRE (env.clipService.importAudioFile (*track, createSineWav (env, "a", 2.0, 0.5f), 0.0).has_value());
    REQUIRE (env.clipService.importAudioFile (*track, createSineWav (env, "b", 2.0, 0.5f), 1.5).has_value());
    TestEnvironment::pumpMessages (100);

    auto clips = env.trackService.getClips (*track);
    REQUIRE (clips.size() == 2);
    REQUIRE (clips[0].fadeOutSeconds == Catch::Approx (0.5).margin (0.01));
    REQUIRE (clips[1].fadeInSeconds == Catch::Approx (0.5).margin (0.01));

    auto off = settings.getSettings();
    off.autoCrossfades = false;
    REQUIRE (settings.setSettings (off).has_value());
    TestEnvironment::pumpMessages (100);
    clips = env.trackService.getClips (*track);
    REQUIRE (clips[1].fadeInSeconds < 0.01);
}

TEST_CASE ("ClipService: Bereich löschen, entfernen, Stille einfügen, teilen", "[services][a4]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Bereich").has_value());
    auto* a = *env.trackService.createAudioTrack ("A", 1);
    auto* b = *env.trackService.createAudioTrack ("B", 1);
    auto* untouched = *env.trackService.createAudioTrack ("C", 1);
    REQUIRE (env.clipService.importAudioFile (*a, createSineWav (env, "a", 4.0, 0.5f), 0.0).has_value());
    REQUIRE (env.clipService.importAudioFile (*b, createSineWav (env, "b", 4.0, 0.5f), 0.0).has_value());
    REQUIRE (env.clipService.importAudioFile (*b, createSineWav (env, "b2", 1.0, 0.5f), 6.0).has_value());
    REQUIRE (env.clipService.importAudioFile (*untouched, createSineWav (env, "c", 4.0, 0.5f), 0.0).has_value());

    const auto starts = [&] (fk::services::TrackHandle* track)
    {
        std::vector<double> result;

        for (const auto& clip : env.trackService.getClips (*track))
            result.push_back (clip.startSeconds);

        std::ranges::sort (result);
        return result;
    };

    REQUIRE (env.clipService.getClipsInRange ({ a, b }, 1.0, 2.0).size() == 2);
    REQUIRE (env.clipService.getClipsInRange ({ a, b }, 5.0, 5.5).empty());

    // Löschen mit Lücke: zwei Stücke je Spur, die hintere Hälfte bleibt bei 2 s.
    REQUIRE (env.clipService.deleteRange ({ a, b }, 1.0, 2.0, false).has_value());
    REQUIRE (starts (a) == std::vector<double> { 0.0, 2.0 });
    REQUIRE (starts (untouched) == std::vector<double> { 0.0 });
    env.projectService.undo();
    REQUIRE (starts (a) == std::vector<double> { 0.0 });

    // Entfernen schließt die Lücke: Das hintere Stück und der spätere Clip rücken eine Sekunde vor.
    REQUIRE (env.clipService.deleteRange ({ b }, 1.0, 2.0, true).has_value());
    const auto bStarts = starts (b);
    REQUIRE (bStarts.size() == 3);
    REQUIRE (bStarts[1] == Catch::Approx (1.0));
    REQUIRE (bStarts[2] == Catch::Approx (5.0));
    env.projectService.undo();

    // Stille einfügen: geteilt bei 1 s, alles dahinter zwei Sekunden später.
    REQUIRE (env.clipService.insertSilence ({ b }, 1.0, 3.0).has_value());
    const auto silenced = starts (b);
    REQUIRE (silenced.size() == 3);
    REQUIRE (silenced[1] == Catch::Approx (3.0));
    REQUIRE (silenced[2] == Catch::Approx (8.0));
    REQUIRE (starts (untouched) == std::vector<double> { 0.0 }); // andere Spuren bleiben
    env.projectService.undo();

    REQUIRE (env.clipService.splitAtRange ({ a }, 1.0, 2.5).has_value());
    REQUIRE (starts (a) == std::vector<double> { 0.0, 1.0, 2.5 });
}

TEST_CASE ("ClipService: Abschnitt bearbeiten (Stille, Verstärken, Fade-in), mit Undo", "[services][a5]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Sample").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gitarre", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, createSineWav (env, "sinus", 2.0, 0.5f), 1.0);
    const auto clip = [&] { return env.trackService.getClips (*track).front(); };
    const auto original = clip().audioFile;

    const auto peakIn = [] (const juce::File& file, double from, double to)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        const auto first = static_cast<int> (from * reader->sampleRate);
        const auto count = static_cast<int> ((to - from) * reader->sampleRate);
        juce::AudioBuffer<float> buffer (1, count);
        reader->read (&buffer, 0, count, first, true, false);
        return buffer.getMagnitude (0, count);
    };

    using P = fk::services::ClipService::AudioProcess;
    REQUIRE (env.clipService.processAudio ({ clipId }, P::silence, fk::services::ClipService::ClipRange { 0.5, 1.0 })
                 .has_value());
    const auto silenced = clip().audioFile;
    REQUIRE (silenced != original);
    REQUIRE (peakIn (silenced, 0.55, 0.95) < 1.0e-6f);
    REQUIRE (peakIn (silenced, 1.1, 1.9) == Catch::Approx (0.5f).margin (0.01f));
    REQUIRE (clip().startSeconds == Catch::Approx (1.0)); // Position bleibt

    REQUIRE (env.clipService.processAudio ({ clipId }, P::gain, std::nullopt, 6.0206f).has_value());
    REQUIRE (peakIn (clip().audioFile, 1.1, 1.9) == Catch::Approx (1.0f).margin (0.01f));

    REQUIRE (env.clipService.processAudio ({ clipId }, P::fadeIn, fk::services::ClipService::ClipRange { 1.0, 2.0 })
                 .has_value());
    REQUIRE (peakIn (clip().audioFile, 1.0, 1.05) < 0.1f);

    REQUIRE_FALSE (
        env.clipService.processAudio ({ clipId }, P::silence, fk::services::ClipService::ClipRange { 3.0, 4.0 })
            .has_value());

    env.projectService.undo();
    env.projectService.undo();
    env.projectService.undo();
    REQUIRE (clip().audioFile == original);
}
