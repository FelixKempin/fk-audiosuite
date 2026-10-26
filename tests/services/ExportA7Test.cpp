#include "fk/services/ExportService.h"

#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SignatureService.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

#include <cmath>

using fk::tests::TestEnvironment;

namespace
{
struct FileFacts
{
    double sampleRate = 0.0;
    unsigned int bitsPerSample = 0;
    double lengthSeconds = 0.0;
    float peak = 0.0f;
};

FileFacts readFacts (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    REQUIRE (reader != nullptr);

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels),
                                     static_cast<int> (reader->lengthInSamples));
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
    return { reader->sampleRate,
             reader->bitsPerSample,
             static_cast<double> (reader->lengthInSamples) / reader->sampleRate,
             buffer.getMagnitude (0, buffer.getNumSamples()) };
}

/// Projekt mit einer Synth-Spur und einem Ton über zwei Takte (bei 120 BPM 4 s).
struct SynthProject
{
    TestEnvironment env;
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::TrackHandle* synth = nullptr;

    explicit SynthProject (const juce::String& name)
    {
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, name).has_value());
        synth = addSynth ("Synth", 60);
    }

    fk::services::TrackHandle* addSynth (const juce::String& name, int note)
    {
        auto* track = *env.trackService.createInstrumentTrack (name, TestEnvironment::synthId (plugins));
        const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 4.0);
        REQUIRE (midiClips.addNote (clipId, note, 0.0, 8.0, 100).has_value());
        return track;
    }
};
} // namespace

TEST_CASE ("Export A7: OGG und MP3 sind hörbar, MP3 nur mit libmp3lame", "[services][export][a7]")
{
    SynthProject project ("Lossy");

    fk::services::ExportOptions options;
    options.file = project.env.tempFolder.getChildFile ("song");
    options.format = fk::services::ExportFormat::ogg;
    REQUIRE_FALSE (project.exporter.getQualityChoices (fk::services::ExportFormat::ogg).empty());
    REQUIRE (project.exporter.getQualityChoices (fk::services::ExportFormat::wav).empty());

    const auto ogg = project.exporter.exportMixdown (options);
    REQUIRE (ogg.has_value());
    REQUIRE (ogg->hasFileExtension (".ogg"));
    REQUIRE (readFacts (*ogg).peak > 0.01f);

    if (! project.exporter.isFormatAvailable (fk::services::ExportFormat::mp3))
    {
        WARN ("libmp3lame fehlt, MP3-Export nicht prüfbar");
        options.format = fk::services::ExportFormat::mp3;
        REQUIRE_FALSE (project.exporter.exportMixdown (options).has_value());
        return;
    }

    options.format = fk::services::ExportFormat::mp3;
    const auto mp3 = project.exporter.exportMixdown (options);
    REQUIRE (mp3.has_value());
    REQUIRE (mp3->hasFileExtension (".mp3"));
    REQUIRE (mp3->getSize() > 1000);
    REQUIRE (readFacts (*mp3).peak > 0.01f); // JUCE liest MP3 (JUCE_USE_MP3AUDIOFORMAT)
}

TEST_CASE ("Export A7: Samplerate, 16 Bit mit Dither, AIFF", "[services][export][a7]")
{
    SynthProject project ("Rate");

    fk::services::ExportOptions options;
    options.file = project.env.tempFolder.getChildFile ("cd");
    options.format = fk::services::ExportFormat::wav;
    options.sampleRate = 44100.0;
    options.bitDepth = 16;
    options.dither = true;

    const auto cd = project.exporter.exportMixdown (options);
    REQUIRE (cd.has_value());
    const auto facts = readFacts (*cd);
    REQUIRE (facts.sampleRate == Catch::Approx (44100.0));
    REQUIRE (facts.bitsPerSample == 16);
    REQUIRE (facts.peak > 0.01f);
    REQUIRE (facts.lengthSeconds >= 3.9); // zwei Takte bei 120 BPM, die Rate ändert die Dauer nicht

    options.format = fk::services::ExportFormat::aiff;
    options.sampleRate = 96000.0;
    options.bitDepth = 24;
    const auto aiff = project.exporter.exportMixdown (options);
    REQUIRE (aiff.has_value());
    REQUIRE (aiff->hasFileExtension (".aiff"));
    REQUIRE (readFacts (*aiff).sampleRate == Catch::Approx (96000.0));
}

TEST_CASE ("Export A7: Lautheit −14 und −23 LUFS liegen 9 dB auseinander", "[services][export][a7]")
{
    SynthProject project ("Lufs");

    const auto peakAt = [&project] (float targetLufs, const juce::String& name)
    {
        fk::services::ExportOptions options;
        options.file = project.env.tempFolder.getChildFile (name);
        options.bitDepth = 32;
        options.normalise = fk::services::NormaliseMode::loudness;
        options.normaliseTargetDb = targetLufs;
        const auto file = project.exporter.exportMixdown (options);
        REQUIRE (file.has_value());
        return readFacts (*file).peak;
    };

    const auto loud = peakAt (-14.0f, "streaming");
    const auto quiet = peakAt (-23.0f, "rundfunk");
    REQUIRE (quiet > 0.001f);
    REQUIRE (juce::Decibels::gainToDecibels (loud / quiet) == Catch::Approx (9.0).margin (0.5));
}

TEST_CASE ("Export A7: Stems je Spur und Gruppe", "[services][export][a7]")
{
    SynthProject project ("Stems");
    auto* bass = project.addSynth ("Bass", 36);
    auto* group = *project.env.trackService.createGroupTrack ("Keys");
    REQUIRE (project.env.trackService.setOutput (*project.synth, project.env.trackService.getTrackId (*group))
                 .has_value());

    fk::services::ExportOptions options;
    options.file = project.env.tempFolder.getChildFile ("Stems");
    options.format = fk::services::ExportFormat::wav;

    REQUIRE_FALSE (project.exporter.exportStems (options, {}).has_value());
    REQUIRE_FALSE (project.exporter.exportStems (options, { "gibt-es-nicht" }).has_value());

    float lastProgress = -1.0f;
    const auto stems = project.exporter.exportStems (
        options,
        { project.env.trackService.getTrackId (*bass), project.env.trackService.getTrackId (*group) },
        [&lastProgress] (float p) { lastProgress = p; });

    REQUIRE (stems.has_value());
    REQUIRE (stems->size() == 2);
    REQUIRE (lastProgress >= 0.99f);

    for (const auto& file : *stems)
    {
        REQUIRE (file.getParentDirectory() == options.file);
        REQUIRE (readFacts (file).peak > 0.01f); // die Gruppe enthält den Synth, der in sie läuft
    }

    REQUIRE ((*stems)[0].getFileNameWithoutExtension().contains ("Bass"));
    REQUIRE ((*stems)[1].getFileNameWithoutExtension().contains ("Keys"));
}

TEST_CASE ("Export A7: MIDI-Datei mit Tempo, Taktart und einer Spur je Instrument", "[services][export][a7][midi]")
{
    SynthProject project ("MidiOut");
    fk::services::SignatureService signature { project.env.projectService };
    REQUIRE (signature.setTempoPoint (0, 100.0, false).has_value());

    // Zweiter Ton auf Schlag 2 (eine Viertel nach dem Anfang).
    const auto clips = project.env.trackService.getClips (*project.synth);
    REQUIRE (project.midiClips.addNote (clips.front().id, 67, 1.0, 1.0, 90).has_value());

    // Stumme Spur ohne MIDI fehlt, ein leeres Projekt wird abgelehnt.
    const auto file = project.exporter.exportMidi (project.env.tempFolder.getChildFile ("noten.mid"));
    REQUIRE (file.has_value());

    juce::MidiFile midi;
    juce::FileInputStream in (*file);
    REQUIRE (midi.readFrom (in));
    REQUIRE (midi.getNumTracks() == 2); // Tempospur + Synth
    REQUIRE (midi.getTimeFormat() == 960);

    juce::MidiMessageSequence tempos;
    midi.findAllTempoEvents (tempos);
    REQUIRE (tempos.getNumEvents() == 1);
    REQUIRE (tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote() == Catch::Approx (0.6));

    std::vector<double> noteOnTicks;
    for (const auto* event : *midi.getTrack (1))
        if (event->message.isNoteOn())
            noteOnTicks.push_back (event->message.getTimeStamp());

    REQUIRE (noteOnTicks.size() == 2);
    REQUIRE (noteOnTicks[0] == Catch::Approx (0.0));
    REQUIRE (noteOnTicks[1] == Catch::Approx (960.0));
}

TEST_CASE ("MIDI-Import A7: Tempo und Taktart der Datei übernehmen", "[services][midi][a7]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "TempoImport").has_value());
    fk::services::SignatureService signature { env.projectService };

    // Datei mit 90 BPM in 3/4, ab Takt 3 (Viertel 6) 140 BPM.
    const auto file = env.tempFolder.getChildFile ("walzer.mid");
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence meta;
        meta.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60.0e6 / 90.0)), 0.0);
        meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0.0);
        meta.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60.0e6 / 140.0)), 6.0 * 480.0);
        midi.addTrack (meta);
        juce::MidiMessageSequence notes;
        notes.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0.0);
        notes.addEvent (juce::MidiMessage::noteOff (1, 60), 480.0);
        midi.addTrack (notes);
        juce::FileOutputStream out (file);
        REQUIRE (midi.writeTo (out));
    }

    const auto changes = fk::services::SignatureService::readMidiFileTempo (file);
    REQUIRE (changes.size() == 2);
    REQUIRE (changes[0].quarterBpm == Catch::Approx (90.0).margin (0.01));
    REQUIRE (changes[0].signature.numerator == 3);
    REQUIRE (changes[1].quarterNote == Catch::Approx (6.0));

    REQUIRE (signature.midiFileTempoDiffers (file, 0.0));
    REQUIRE_FALSE (signature.midiFileTempoDiffers (env.createMidiFile ("ohneTempo"), 0.0));
    REQUIRE (signature.applyMidiFileTempo (file, 0.0).has_value());

    REQUIRE (signature.getTempoAt (0.5) == Catch::Approx (90.0).margin (0.01));
    // Sechs Viertel bei 90 BPM = 4 s; danach 140 BPM.
    REQUIRE (signature.getTempoAt (3.9) == Catch::Approx (90.0).margin (0.01));
    REQUIRE (signature.getTempoAt (4.1) == Catch::Approx (140.0).margin (0.01));
    REQUIRE (signature.getTimeSignaturePoints().front().signature.numerator == 3);

    // Ein Undo-Schritt stellt das alte Tempo wieder her.
    env.projectService.undo();
    REQUIRE (signature.getTempoAt (0.5) == Catch::Approx (120.0).margin (0.01));
}
