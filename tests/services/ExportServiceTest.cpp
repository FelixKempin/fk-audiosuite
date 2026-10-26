#include "fk/services/ExportService.h"

#include "fk/services/ClipService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
float peakOfFile (const juce::File& file, double& lengthSeconds)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    REQUIRE (reader != nullptr);

    lengthSeconds = static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels),
                                     static_cast<int> (reader->lengthInSamples));
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
    return buffer.getMagnitude (0, buffer.getNumSamples());
}
} // namespace

TEST_CASE ("ExportService: Mixdown eines Synth-Clips ergibt eine hörbare WAV-Datei", "[services][export]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Export").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0);
    REQUIRE (midiClips.addNote (clipId, 60, 0.0, 4.0, 100).has_value()); // ein ganzer Takt Ton

    fk::services::ExportOptions options;
    options.file = env.tempFolder.getChildFile ("mixdown.wav");
    options.format = fk::services::ExportFormat::wav;
    options.bitDepth = 24;

    float lastProgress = -1.0f;
    const auto result = exporter.exportMixdown (options, [&] (float p) { lastProgress = p; });

    REQUIRE (result.has_value());
    REQUIRE (result->existsAsFile());
    REQUIRE (lastProgress >= 0.99f);

    double lengthSeconds = 0.0;
    const float peak = peakOfFile (*result, lengthSeconds);
    REQUIRE (peak > 0.01f);                                      // nicht still
    REQUIRE (lengthSeconds == Catch::Approx (2.0).margin (0.6)); // Projektlänge ≈ Clip-Länge (+ Ausklingen)
}

TEST_CASE ("ExportService: FLAC und Loop-Bereich, Fehler bei leerem Projekt", "[services][export]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::TimelineService timeline { env.projectService };
    fk::services::ExportService exporter { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Flac").has_value());

    fk::services::ExportOptions empty;
    empty.file = env.tempFolder.getChildFile ("leer.wav");
    REQUIRE_FALSE (exporter.exportMixdown (empty).has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 4.0);
    REQUIRE (midiClips.addNote (clipId, 64, 0.0, 8.0, 100).has_value());

    timeline.setLoopRange ({ 1.0, 2.0 });

    fk::services::ExportOptions options;
    options.file = env.tempFolder.getChildFile ("loop.flac");
    options.format = fk::services::ExportFormat::flac;
    options.range = fk::services::ExportRange::loopRange;
    options.bitDepth = 16;

    const auto result = exporter.exportMixdown (options);
    REQUIRE (result.has_value());

    double lengthSeconds = 0.0;
    REQUIRE (peakOfFile (*result, lengthSeconds) > 0.01f);
    REQUIRE (lengthSeconds == Catch::Approx (1.0).margin (0.1));
}
