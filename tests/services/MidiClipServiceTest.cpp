#include "fk/services/MidiClipService.h"

#include "fk/services/MidiControllerService.h"
#include "fk/services/NoteValue.h"
#include "fk/services/PluginService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
struct MidiProject
{
    explicit MidiProject (TestEnvironment& environment)
        : env (environment)
    {
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Midi").has_value());
        track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
        clipId = *midiClips.createEmptyClip (*track, 2.0, 2.0); // 120 BPM: 2 s = 1 Takt
    }

    TestEnvironment& env;
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::MidiControllerService controllers { env.projectService };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};
} // namespace

TEST_CASE ("MidiClipService: leerer Clip hat die Länge in Schlägen und keine Noten", "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);

    const auto clip = project.midiClips.getClip (project.clipId);
    REQUIRE (clip.has_value());
    REQUIRE (clip->lengthBeats == Catch::Approx (4.0));
    REQUIRE (clip->notes.empty());

    const auto clips = env.trackService.getClips (*project.track);
    REQUIRE (clips.size() == 1);
    REQUIRE (clips[0].isMidi);
    REQUIRE (clips[0].startSeconds == Catch::Approx (2.0));
}

TEST_CASE ("MidiClipService: Noten einfügen, verschieben, verlängern, löschen", "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);

    REQUIRE (project.midiClips.addNote (project.clipId, 60, 0.0, 1.0, 100).has_value());
    REQUIRE (project.midiClips.addNote (project.clipId, 64, 1.0, 0.5, 90).has_value());

    auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes.size() == 2);
    REQUIRE (clip.notes[0].noteNumber == 60);
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (1.0));

    REQUIRE (project.midiClips.moveNote (project.clipId, 1, 67, 2.0).has_value());
    REQUIRE (project.midiClips.setNoteLength (project.clipId, 0, 2.0).has_value());
    REQUIRE (project.midiClips.setNoteVelocity (project.clipId, 0, 50).has_value());

    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[1].noteNumber == 67);
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (2.0));
    REQUIRE (clip.notes[0].lengthBeats == Catch::Approx (2.0));
    REQUIRE (clip.notes[0].velocity == 50);

    // Die Noten erscheinen auch in der Arrangement-Ansicht, in Sekunden (120 BPM: 1 Schlag = 0,5 s).
    const auto arrangeNotes = env.trackService.getClips (*project.track)[0].notes;
    REQUIRE (arrangeNotes.size() == 2);
    REQUIRE (arrangeNotes[1].startSeconds == Catch::Approx (3.0));

    REQUIRE (project.midiClips.removeNote (project.clipId, 0).has_value());
    REQUIRE (project.midiClips.getClip (project.clipId)->notes.size() == 1);
    REQUIRE_FALSE (project.midiClips.removeNote (project.clipId, 7).has_value());
}

TEST_CASE ("MidiClipService: quantisieren rückt Notenanfänge aufs Raster, Undo stellt zurück", "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);

    REQUIRE (project.midiClips.addNote (project.clipId, 60, 0.13, 0.5, 100).has_value());
    REQUIRE (project.midiClips.addNote (project.clipId, 62, 1.9, 0.5, 100).has_value());

    REQUIRE (project.midiClips.quantise (project.clipId, 0.25).has_value());
    auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[0].startBeats == Catch::Approx (0.25));
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (2.0));

    env.projectService.undo();
    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[0].startBeats == Catch::Approx (0.13));
}

TEST_CASE ("MidiClipService: addChord setzt mehrere Noten in einem Undo-Schritt", "[services][midi]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Akkord").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto clipId = *midiClips.createEmptyClip (*track, 2.0, 2.0);

    REQUIRE (midiClips.addChord (clipId, { 60, 64, 67 }, 1.0, 2.0, 90).has_value());
    auto info = *midiClips.getClip (clipId);
    REQUIRE (info.notes.size() == 3);
    REQUIRE (info.startSeconds == Catch::Approx (2.0));
    REQUIRE (info.notes[0].startBeats == Catch::Approx (1.0));
    REQUIRE (info.notes[2].noteNumber == 67);

    env.projectService.undo();
    REQUIRE (midiClips.getClip (clipId)->notes.empty());

    REQUIRE_FALSE (midiClips.addChord (clipId, {}, 0.0, 1.0, 90).has_value());
}

TEST_CASE ("MidiClipService: Controller-Ereignisse setzen, ändern, löschen; Pitch-Bend hat eigenen Bereich",
           "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);
    using namespace fk::services::controllerTypes;

    REQUIRE (project.controllers.addControllerEvent (project.clipId, sustain, 1.0, 127).has_value());
    REQUIRE (project.controllers.addControllerEvent (project.clipId, sustain, 3.0, 0).has_value());
    REQUIRE (project.controllers.addControllerEvent (project.clipId, pitchBend, 0.5, 20000).has_value()); // begrenzt
    REQUIRE (project.controllers.addControllerEvent (project.clipId, modWheel, 2.0, 300).has_value());    // begrenzt

    auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.controllers.size() == 4);
    REQUIRE (clip.controllers[0].type == pitchBend); // nach Zeit sortiert
    REQUIRE (clip.controllers[0].value == 16383);
    REQUIRE (clip.controllers[0].beats == Catch::Approx (0.5));
    REQUIRE (clip.controllers[1].value == 127);
    REQUIRE (clip.controllers[2].type == modWheel);
    REQUIRE (clip.controllers[2].value == 127);

    REQUIRE (project.controllers.setControllerEvent (project.clipId, 1, 1.5, 64).has_value());
    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.controllers[1].beats == Catch::Approx (1.5));
    REQUIRE (clip.controllers[1].value == 64);

    REQUIRE (project.controllers.removeControllerEvent (project.clipId, 0).has_value());
    REQUIRE (project.midiClips.getClip (project.clipId)->controllers.size() == 3);
    REQUIRE_FALSE (project.controllers.removeControllerEvent (project.clipId, 10).has_value());

    // Bereich löschen trifft nur die Art: Sustain-Ereignisse zwischen 0 und 2 Schlägen weg, Mod-Wheel bleibt.
    REQUIRE (project.controllers.removeControllerEvents (project.clipId, sustain, 0.0, 2.0).has_value());
    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.controllers.size() == 2);
    env.projectService.undo();
    REQUIRE (project.midiClips.getClip (project.clipId)->controllers.size() == 3);

    REQUIRE (name (sustain) == "Sustain (CC64)");
    REQUIRE (name (11).startsWith ("CC11"));
    REQUIRE (defaultValue (pitchBend) == 8192);
}

TEST_CASE ("MidiClipService: mehrere Noten in einem Undo-Schritt verschieben, kopieren, löschen",
           "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);
    REQUIRE (project.midiClips.addNote (project.clipId, 60, 0.0, 1.0, 100).has_value());
    REQUIRE (project.midiClips.addNote (project.clipId, 64, 1.0, 1.0, 90).has_value());

    auto notes = project.midiClips.getClip (project.clipId)->notes;
    REQUIRE (notes.size() == 2);

    // Beide eine Oktave höher und einen halben Schlag später, linke Kante getrimmt (Länge kürzer).
    for (auto& note : notes)
    {
        note.noteNumber += 12;
        note.startBeats += 0.5;
        note.lengthBeats -= 0.5;
        note.velocity = 127;
    }

    REQUIRE (project.midiClips.updateNotes (project.clipId, notes, "Noten verschieben").has_value());
    auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[0].noteNumber == 72);
    REQUIRE (clip.notes[1].noteNumber == 76);
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (1.5));
    REQUIRE (clip.notes[1].lengthBeats == Catch::Approx (0.5));
    REQUIRE (clip.notes[0].velocity == 127);

    env.projectService.undo(); // ein Schritt für alles
    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[0].noteNumber == 60);
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (1.0));

    REQUIRE (project.midiClips.addNotes (project.clipId, clip.notes, "Noten kopieren").has_value());
    REQUIRE (project.midiClips.getClip (project.clipId)->notes.size() == 4);
    REQUIRE (project.midiClips.removeNotes (project.clipId, { 0, 1 }).has_value());
    REQUIRE (project.midiClips.getClip (project.clipId)->notes.size() == 2);
    REQUIRE_FALSE (project.midiClips.removeNotes (project.clipId, { 7 }).has_value());

    // Sehr kurze Noten (1/128-Triole) bleiben erhalten statt auf 1/64 aufzurunden.
    const double shortest = fk::services::NoteValue { 128, fk::services::NoteValue::Feel::triplet }.beats();
    REQUIRE (project.midiClips.addNote (project.clipId, 50, 3.0, shortest, 80).has_value());
    bool found = false;
    const auto withShortNote = *project.midiClips.getClip (project.clipId);

    for (const auto& note : withShortNote.notes)
        found = found || (note.noteNumber == 50 && note.lengthBeats == Catch::Approx (shortest));

    REQUIRE (found);
}

TEST_CASE ("MidiClipService: Quantisieren nur der Auswahl", "[services][midiclip]")
{
    TestEnvironment env;
    MidiProject project (env);
    REQUIRE (project.midiClips.addNote (project.clipId, 60, 0.1, 0.5, 100).has_value());
    REQUIRE (project.midiClips.addNote (project.clipId, 62, 1.1, 0.5, 100).has_value());

    REQUIRE (project.midiClips.quantise (project.clipId, 1.0, { 1 }).has_value());
    const auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.notes[0].startBeats == Catch::Approx (0.1)); // nicht ausgewählt
    REQUIRE (clip.notes[1].startBeats == Catch::Approx (1.0));
}

TEST_CASE ("MidiControllerService: Zeichnen ersetzt den Bereich in einem Undo-Schritt und meldet sich",
           "[services][midiclip]")
{
    // Regression 2026-10-11: Änderungen an Controller-Ereignissen lösten kein midiClipChanged aus,
    // die Lanes im Key Editor zeigten neue Punkte erst nach einer anderen Änderung.
    struct Counter final : fk::services::MidiClipService::Listener
    {
        void midiClipChanged() override { ++calls; }
        int calls = 0;
    };

    TestEnvironment env;
    MidiProject project (env);
    Counter counter;
    project.midiClips.addListener (counter);
    TestEnvironment::pumpMessages (20);
    counter.calls = 0;

    using namespace fk::services::controllerTypes;
    REQUIRE (project.controllers.addControllerEvent (project.clipId, modWheel, 0.0, 10).has_value());
    TestEnvironment::pumpMessages (20);
    REQUIRE (counter.calls >= 1);

    REQUIRE (project.controllers
                 .replaceControllerEvents (
                     project.clipId, modWheel, 0.0, 2.0, { { 0.0, 20 }, { 0.5, 40 }, { 1.0, 60 }, { 1.5, 80 } })
                 .has_value());
    auto clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.controllers.size() == 4);
    REQUIRE (clip.controllers[0].value == 20);

    env.projectService.undo();
    clip = *project.midiClips.getClip (project.clipId);
    REQUIRE (clip.controllers.size() == 1);
    REQUIRE (clip.controllers[0].value == 10);
    project.midiClips.removeListener (counter);
}

TEST_CASE ("NoteValue: Längen wie in Cubase, gerade, punktiert, triolisch", "[services][midiclip]")
{
    using fk::services::NoteValue;
    REQUIRE (NoteValue { 1, NoteValue::Feel::straight }.beats() == Catch::Approx (4.0));
    REQUIRE (NoteValue { 4, NoteValue::Feel::straight }.beats() == Catch::Approx (1.0));
    REQUIRE (NoteValue { 8, NoteValue::Feel::dotted }.beats() == Catch::Approx (0.75));
    REQUIRE (NoteValue { 8, NoteValue::Feel::triplet }.beats() == Catch::Approx (1.0 / 3.0));
    REQUIRE (NoteValue { 128, NoteValue::Feel::straight }.beats() == Catch::Approx (1.0 / 32.0));
    REQUIRE (NoteValue::all().size() == 24); // 1/1 bis 1/128, je drei Arten
    REQUIRE (NoteValue { 16, NoteValue::Feel::triplet }.name() == "1/16 triplet");
    REQUIRE (NoteValue { 4, NoteValue::Feel::dotted }.shortName() == "1/4.");
}
