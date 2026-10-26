#include "fk/services/DrumMapService.h"
#include "fk/services/ExportService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/DrumEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

#include <algorithm>
#include <cmath>
#include <set>

using fk::tests::TestEnvironment;

namespace
{
juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> position, juce::Point<float> down)
{
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             &target, &target,
             juce::Time::getCurrentTime(),
             down,
             juce::Time::getCurrentTime(),
             1,
             position != down };
}
} // namespace

TEST_CASE ("B2: Drum-Maps mitgeliefert, automatisch, zugeordnet, als JSON", "[services][drums][b2]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    const auto folder = env.tempFolder.getChildFile ("DrumMaps");
    fk::services::DrumMapService drumMaps { env.projectService, env.trackService, folder };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Drums").has_value());

    const auto maps = drumMaps.getMaps();
    REQUIRE (maps.size() == 2);
    const auto gm = *drumMaps.getMap (fk::services::DrumMapService::gmMapId);
    REQUIRE (gm.nameFor (36) == "Bass drum");
    REQUIRE (gm.nameFor (42) == "Closed hi-hat");
    const auto fkDrums = *drumMaps.getMap (fk::services::DrumMapService::fkDrumsMapId);
    REQUIRE (fkDrums.nameFor (40) == "Snare rimshot");
    REQUIRE (fkDrums.nameFor (21) == "Hi-hat foot splash");

    // Jede Note höchstens einmal je Map.
    for (const auto& map : maps)
    {
        std::set<int> notes;
        for (const auto& entry : map.entries)
            REQUIRE (notes.insert (entry.note).second);
    }

    // Synth-Spur: automatisch keine Map; Zuordnung von Hand, dann "keine", dann wieder automatisch.
    auto* synth = *env.trackService.createInstrumentTrack ("Bass", TestEnvironment::synthId (plugins));
    REQUIRE_FALSE (drumMaps.getMapForTrack (*synth).has_value());
    REQUIRE (drumMaps.setMapForTrack (*synth, fk::services::DrumMapService::gmMapId).has_value());
    REQUIRE (drumMaps.getMapForTrack (*synth)->id == "gm");
    REQUIRE (drumMaps.setMapForTrack (*synth, fk::services::DrumMapService::noMapId).has_value());
    REQUIRE_FALSE (drumMaps.getMapForTrack (*synth).has_value());
    REQUIRE (drumMaps.setMapForTrack (*synth, "").has_value());
    REQUIRE_FALSE (drumMaps.setMapForTrack (*synth, "gibt-es-nicht").has_value());

    // Audiospuren haben nie eine Map.
    auto* audio = *env.trackService.createAudioTrack ("Gesang", 1);
    REQUIRE_FALSE (drumMaps.getMapForTrack (*audio).has_value());

    // Eigene Map: Export, Import, Löschen; mitgelieferte lassen sich nicht löschen.
    const auto file = env.tempFolder.getChildFile ("export.json");
    REQUIRE (drumMaps.exportMap ("gm", file).has_value());
    const auto imported = drumMaps.importMap (file);
    REQUIRE (imported.has_value());
    REQUIRE (imported->entries.size() == gm.entries.size());
    REQUIRE (drumMaps.getMaps().size() == 3);
    REQUIRE_FALSE (drumMaps.deleteMap ("gm").has_value());
    REQUIRE (drumMaps.deleteMap (imported->id).has_value());
    REQUIRE (drumMaps.getMaps().size() == 2);

    REQUIRE_FALSE (fk::services::DrumMapService::fromJson ("{\"name\":\"x\",\"entries\":[{\"note\":200}]}").has_value());
    REQUIRE_FALSE (fk::services::DrumMapService::fromJson ("kein json").has_value());
}

TEST_CASE ("B2: Drum-Editor setzt, löscht und ändert Schläge", "[ui][drums][b2]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::NotePreviewService preview { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::DrumMapService drumMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("DrumMaps") };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    REQUIRE (env.projectService.createProject (env.tempFolder, "DrumEdit").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Drums", TestEnvironment::synthId (plugins));
    REQUIRE (drumMaps.setMapForTrack (*track, "gm").has_value());
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0); // ein Takt
    REQUIRE (midiClips.addNote (clipId, 99, 0.0, 0.25, 100).has_value()); // nicht in der GM-Map

    fk::ui::DrumEditor editor ({ midiClips, preview, transport, env.timelineService, drumMaps, env.trackService, sampler });
    editor.setSize (900, 400);
    editor.setClip (clipId);

    const auto& rows = editor.getRows();
    REQUIRE (rows.size() == 42);                 // 41 GM-Instrumente + die fremde Note
    REQUIRE (rows.front().name == "Bass drum 2"); // Reihenfolge der Map
    REQUIRE_FALSE (rows.back().inMap);
    REQUIRE (rows.back().note == 99);

    // Klick in die Bassdrum-Zeile bei Schlag 2 (Sechzehntel-Raster): neuer Schlag mit Anschlag 110.
    auto& grid = editor.getGrid();
    const int kickRow = grid.rowForNote (36);
    const auto at = juce::Point<float> (static_cast<float> (grid.xForBeats (2.0)) + 2.0f, static_cast<float> (grid.yForRow (kickRow)));
    grid.mouseDown (mouseAt (grid, at, at));
    grid.mouseUp (mouseAt (grid, at, at));
    TestEnvironment::pumpMessages (50);

    auto clip = *midiClips.getClip (clipId);
    REQUIRE (clip.notes.size() == 2);
    const auto kick = std::ranges::find (clip.notes, 36, &fk::services::EditableNote::noteNumber);
    REQUIRE (kick != clip.notes.end());
    REQUIRE (kick->startBeats == 2.0);
    REQUIRE (kick->velocity == 110);

    // Raute nach oben ziehen: lauter.
    const auto onHit = juce::Point<float> (static_cast<float> (grid.xForBeats (2.0)), static_cast<float> (grid.yForRow (kickRow)));
    const auto above = onHit.translated (0.0f, -10.0f);
    grid.mouseDown (mouseAt (grid, onHit, onHit));
    grid.mouseDrag (mouseAt (grid, above, onHit));
    grid.mouseUp (mouseAt (grid, above, onHit));
    TestEnvironment::pumpMessages (50);
    clip = *midiClips.getClip (clipId);
    REQUIRE (std::ranges::find (clip.notes, 36, &fk::services::EditableNote::noteNumber)->velocity == 120);

    // Klick auf die Raute ohne Ziehen: gelöscht.
    grid.mouseDown (mouseAt (grid, onHit, onHit));
    grid.mouseUp (mouseAt (grid, onHit, onHit));
    TestEnvironment::pumpMessages (50);
    REQUIRE (midiClips.getClip (clipId)->notes.size() == 1);

    // Nur benutzte Instrumente: bleibt die fremde Note.
    editor.setShowUsedOnly (true);
    REQUIRE (editor.getRows().size() == 1);
}

namespace
{
juce::File writeSine (const juce::File& file, double frequency)
{
    file.getParentDirectory().createDirectory();
    constexpr double sampleRate = 48000.0;
    juce::AudioBuffer<float> sine (1, static_cast<int> (sampleRate * 0.3));

    for (int i = 0; i < sine.getNumSamples(); ++i)
        sine.setSample (0, i, 0.8f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * frequency * i / sampleRate)));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    auto writer = format.createWriterFor (
        stream, juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16));
    writer->writeFromAudioSampleBuffer (sine, 0, sine.getNumSamples());
    return file;
}

float peakBetween (const juce::File& file, double from, double to)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    REQUIRE (reader != nullptr);
    const auto start = static_cast<juce::int64> (from * reader->sampleRate);
    const auto end = std::min (reader->lengthInSamples, static_cast<juce::int64> (to * reader->sampleRate));

    if (end <= start)
        return 0.0f;

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (end - start));
    reader->read (&buffer, 0, buffer.getNumSamples(), start, true, true);
    return buffer.getMagnitude (0, buffer.getNumSamples());
}
} // namespace

TEST_CASE ("B2: Sampler-Spur aus einer Datei und als Drum-Kit", "[services][ui][sampler][b2]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::NotePreviewService preview { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::DrumMapService drumMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("DrumMaps") };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Sampler").has_value());

    const auto kick = writeSine (env.tempFolder.getChildFile ("samples/kick.wav"), 60.0);
    const auto snare = writeSine (env.tempFolder.getChildFile ("samples/snare.wav"), 220.0);

    // Aus einer Datei: chromatisch, das Sample liegt danach im Projektordner.
    auto* melodic = *sampler.createSamplerTrack (snare);
    REQUIRE (sampler.isSamplerTrack (*melodic));
    REQUIRE (sampler.getRegions (*melodic).size() == 1);
    REQUIRE (sampler.getRegions (*melodic).front().chromatic);
    REQUIRE (sampler.getRegions (*melodic).front().sample.isAChildOf (env.projectService.getProjectFolder()));
    REQUIRE_FALSE (drumMaps.getMapForTrack (*melodic).has_value()); // Key Editor, kein Drum-Editor

    // Leeres Kit: GM-Map, ein Undo-Schritt.
    auto* kit = *sampler.createSamplerTrack();
    REQUIRE (env.projectService.getUndoHistory().done.back() == "Add track");
    REQUIRE (drumMaps.getMapForTrack (*kit)->id == "gm");
    REQUIRE (sampler.getRegions (*kit).empty());

    // Im Drum-Editor auf die Bassdrum-Zeile ablegen.
    const auto clipId = *midiClips.createEmptyClip (*kit, 0.0, 2.0);
    fk::ui::DrumEditor editor ({ midiClips, preview, transport, env.timelineService, drumMaps, env.trackService, sampler });
    editor.setSize (900, 600);
    editor.setClip (clipId);
    const int kickRow = editor.getGrid().rowForNote (36);
    const int viewportTop = 34; // Kopfzeile
    editor.dropSample (kick, viewportTop + kickRow * fk::ui::DrumGrid::rowHeight + 5);
    REQUIRE (sampler.getRegions (*kit).size() == 1);
    REQUIRE (sampler.getRegions (*kit).front().note == 36);
    REQUIRE (editor.getRows()[static_cast<std::size_t> (kickRow)].sampleName == "kick");

    REQUIRE (sampler.assignSample (*kit, snare, 38).has_value());
    REQUIRE (sampler.assignSample (*kit, kick, 36).has_value()); // ersetzt, verdoppelt nicht
    REQUIRE (sampler.getRegions (*kit).size() == 2);

    // Synth-Spuren nehmen keine Samples.
    auto* synth = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
    REQUIRE_FALSE (sampler.assignSample (*synth, kick, 36).has_value());

    // Hörbar: Bassdrum bei 0 s, Snare-Note 38 bei 1 s, eine Note ohne Sample (40) bei 1,5 s still.
    REQUIRE (midiClips.addNote (clipId, 36, 0.0, 0.25, 110).has_value());
    REQUIRE (midiClips.addNote (clipId, 38, 2.0, 0.25, 110).has_value());
    REQUIRE (midiClips.addNote (clipId, 40, 3.0, 0.25, 110).has_value());

    const auto render = [&] (const juce::String& name)
    {
        fk::services::ExportOptions options;
        options.file = env.tempFolder.getChildFile ("Stems-" + name);
        options.includeTails = false;
        const auto stems = exporter.exportStems (options, { env.trackService.getTrackId (*kit) });
        REQUIRE (stems.has_value());
        return stems->front();
    };

    auto stem = render ("a");
    REQUIRE (peakBetween (stem, 0.0, 0.25) > 0.05f);
    REQUIRE (peakBetween (stem, 1.0, 1.25) > 0.05f);
    REQUIRE (peakBetween (stem, 1.5, 1.75) < 0.001f);

    // Sample entfernen: Bassdrum still.
    REQUIRE (sampler.removeSample (*kit, 36).has_value());
    REQUIRE_FALSE (sampler.removeSample (*kit, 36).has_value());
    stem = render ("b");
    REQUIRE (peakBetween (stem, 0.0, 0.25) < 0.001f);
    REQUIRE (peakBetween (stem, 1.0, 1.25) > 0.05f);

    // Speichern und öffnen: Das Kit lädt seine SFZ wieder und klingt.
    const auto kitId = env.trackService.getTrackId (*kit);
    editor.setClip ({});
    REQUIRE (env.projectService.saveProject().has_value());
    const auto file = env.tempFolder.getChildFile ("Sampler/Sampler.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());
    kit = env.trackService.findTrack (kitId);
    REQUIRE (kit != nullptr);
    REQUIRE (sampler.getRegions (*kit).size() == 1);
    stem = render ("c");
    REQUIRE (peakBetween (stem, 1.0, 1.25) > 0.05f);

    // SFZ-Text: Pfade relativ, Pads mit key=, chromatisch mit Grundton.
    const auto sfz = fk::services::SamplerService::createSfz (
        { { env.tempFolder.getChildFile ("a/kick.wav"), 36, false }, { env.tempFolder.getChildFile ("a/pad.wav"), 60, true } },
        env.tempFolder.getChildFile ("a"));
    REQUIRE (sfz.contains ("sample=kick.wav key=36"));
    REQUIRE (sfz.contains ("sample=pad.wav lokey=0 hikey=127 pitch_keycenter=60"));
}
