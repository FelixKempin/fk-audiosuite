#include "fk/services/ExportService.h"
#include "fk/services/MixerService.h"
#include "fk/services/PluginService.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
juce::File
writeWav (TestEnvironment& env, const juce::String& name, const std::function<float (int)>& sampleAt, double seconds)
{
    constexpr double sampleRate = 44100.0;
    const auto file = env.tempFolder.getChildFile (name + ".wav");
    file.getParentDirectory().createDirectory();
    juce::AudioBuffer<float> buffer (1, static_cast<int> (seconds * sampleRate));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, sampleAt (i));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = format.createWriterFor (
        stream,
        juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (24));
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return file;
}

/// Spitzenpegel der Datei im Zeitfenster [from, to).
float peakBetween (const juce::File& file, double from, double to)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    const auto first = static_cast<juce::int64> (from * reader->sampleRate);
    const auto count = static_cast<int> ((to - from) * reader->sampleRate);
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), count);
    reader->read (&buffer, 0, count, first, true, true);
    return buffer.getMagnitude (0, 0, count);
}

juce::String effectId (fk::services::PluginService& plugins, const juce::String& name)
{
    for (const auto& effect : plugins.getAvailableEffects())
        if (effect.name == name)
            return effect.id;

    return {};
}
} // namespace

TEST_CASE ("Sidechain: Spur A duckt über den FK Compressor die Spur B (Export)", "[services][a6][sidechain]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::ExportService exporter { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Sidechain").has_value());
    auto* kick = *env.trackService.createAudioTrack ("Kick", 1);
    auto* bass = *env.trackService.createAudioTrack ("Bass", 1);

    // Kick: von 1 bis 2 s laut, sonst still. Bass: durchgehend ein leiser Sinus.
    REQUIRE (env.clipService
                 .importAudioFile (*kick,
                                   writeWav (
                                       env,
                                       "kick",
                                       [] (int i) {
                                           return i >= 44100 && i < 88200
                                                      ? 0.9f * std::sin (0.05f * static_cast<float> (i))
                                                      : 0.0f;
                                       },
                                       3.0),
                                   0.0)
                 .has_value());
    REQUIRE (env.clipService
                 .importAudioFile (
                     *bass,
                     writeWav (
                         env, "bass", [] (int i) { return 0.25f * std::sin (0.03f * static_cast<float> (i)); }, 3.0),
                     0.0)
                 .has_value());

    const auto compressorId = *plugins.insertPlugin (*bass, effectId (plugins, "FK Compressor"));
    const auto sources = plugins.getSidechainSources (compressorId);
    REQUIRE (sources.size() == 1);
    REQUIRE (sources.front().name == "Kick");

    const auto inserts = plugins.getInserts (*bass);
    REQUIRE (inserts.front().canSidechain);
    REQUIRE (plugins.setSidechainSource (compressorId, sources.front().trackId).has_value());
    REQUIRE (plugins.getInserts (*bass).front().sidechainSourceName == "Kick");

    // Kick stumm: Im Mix bleibt nur der Bass. Tracktion rechnet stumme Sidechain-Quellen trotzdem
    // weiter (wie die "Geister-Kick" in Cubase), die Sidechain bekommt also ihr Signal.
    env.trackService.setMuted (*kick, true);

    fk::services::ExportOptions options;
    options.file = env.tempFolder.getChildFile ("mix.wav");
    const auto result = exporter.exportMixdown (options, {});
    REQUIRE (result.has_value());

    // Ohne Kick (0,2–0,9 s) spielt der Bass voll, mit Kick (1,2–1,9 s) wird er deutlich abgesenkt.
    const auto free = peakBetween (*result, 0.2, 0.9);
    const auto ducked = peakBetween (*result, 1.2, 1.9);
    INFO ("frei " << free << ", geduckt " << ducked);
    REQUIRE (free > 0.1f);
    REQUIRE (ducked < 0.5f * free);

    // Quelle entfernen: kein Ducken mehr.
    REQUIRE (plugins.setSidechainSource (compressorId, {}).has_value());
    REQUIRE (plugins.getInserts (*bass).front().sidechainSourceId.isEmpty());
}

TEST_CASE ("Kanal-Trim und Phase: +6 dB verdoppeln, Phase löscht eine Kopie aus, Trim ist kein Insert",
           "[services][a6]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::ExportService exporter { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Trim").has_value());
    auto* first = *env.trackService.createAudioTrack ("Eins", 1);
    const auto sine =
        writeWav (env, "sinus", [] (int i) { return 0.2f * std::sin (0.03f * static_cast<float> (i)); }, 2.0);
    REQUIRE (env.clipService.importAudioFile (*first, sine, 0.0).has_value());

    const auto exportPeak = [&] (const juce::String& name)
    {
        fk::services::ExportOptions options;
        options.file = env.tempFolder.getChildFile (name + ".wav");
        const auto result = exporter.exportMixdown (options, {});
        REQUIRE (result.has_value());
        return peakBetween (*result, 0.2, 1.8);
    };

    const auto plain = exportPeak ("ohne");
    REQUIRE (mixer.getTrimDb (*first) == Catch::Approx (0.0f));
    REQUIRE (mixer.setTrimDb (*first, 6.0206f).has_value());
    REQUIRE (mixer.getTrimDb (*first) == Catch::Approx (6.02f).margin (0.05f));
    REQUIRE (plugins.getInserts (*first).empty()); // Trim zählt nicht als Insert
    REQUIRE (exportPeak ("trim") == Catch::Approx (2.0f * plain).margin (0.02f));

    // Ein neuer Insert landet hinter dem Trim, der Trim bleibt unsichtbar.
    REQUIRE (plugins.insertPlugin (*first, effectId (plugins, "FK Gain")).has_value());
    REQUIRE (plugins.getInserts (*first).size() == 1);
    REQUIRE (mixer.setTrimDb (*first, 0.0f).has_value());

    // Zweite Spur mit derselben Datei, Phase gedreht: zusammen fast Stille.
    auto* second = *env.trackService.createAudioTrack ("Zwei", 1);
    REQUIRE (env.clipService.importAudioFile (*second, sine, 0.0).has_value());
    REQUIRE (mixer.setPhaseInverted (*second, true).has_value());
    REQUIRE (mixer.isPhaseInverted (*second));
    REQUIRE (exportPeak ("phase") < 0.01f * plain);
}
