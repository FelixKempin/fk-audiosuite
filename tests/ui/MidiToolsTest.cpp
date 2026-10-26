#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"
#include "fk/services/StepInputService.h"
#include "fk/ui/ListEditor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

#include <algorithm>

using fk::services::EditableNote;
using fk::services::MidiClipService;
using fk::services::MidiFunction;
using fk::tests::TestEnvironment;

namespace
{
std::vector<EditableNote> sampleNotes()
{
    return { { 0, 60, 0.0, 0.5, 100 }, { 1, 64, 0.0, 0.25, 90 }, { 2, 67, 1.0, 0.25, 80 }, { 3, 60, 3.0, 0.5, 70 } };
}
} // namespace

TEST_CASE ("B3a: MIDI-Funktionen rechnen richtig", "[services][midi][b3]")
{
    const auto transposed = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::transpose, .semitones = 12 }, 4.0);
    REQUIRE (transposed[0].noteNumber == 72);
    REQUIRE (MidiClipService::transform ({ { 0, 120, 0.0, 1.0, 100 } }, { .function = MidiFunction::transpose, .semitones = 12 }, 4.0)[0].noteNumber == 127);

    const auto fixed = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::fixedLength, .beats = 1.0 }, 4.0);
    REQUIRE (std::ranges::all_of (fixed, [] (const auto& n) { return n.lengthBeats == 1.0; }));

    // Legato: Akkord bei 0 reicht bis 1, die Note bei 1 bis 3, die letzte bis zum Clip-Ende.
    const auto legato = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::legato }, 4.0);
    REQUIRE (legato[0].lengthBeats == Catch::Approx (1.0));
    REQUIRE (legato[1].lengthBeats == Catch::Approx (1.0));
    REQUIRE (legato[2].lengthBeats == Catch::Approx (2.0));
    REQUIRE (legato[3].lengthBeats == Catch::Approx (1.0));

    const auto loud = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::fixedVelocity, .velocity = 127 }, 4.0);
    REQUIRE (std::ranges::all_of (loud, [] (const auto& n) { return n.velocity == 127; }));
    const auto scaled = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::scaleVelocity, .amount = 1.5 }, 4.0);
    REQUIRE (scaled[0].velocity == 127); // begrenzt
    REQUIRE (scaled[3].velocity == 105);

    // Humanize: bleibt im Rahmen, gleicher Startwert = gleiches Ergebnis, aber nicht unverändert.
    const auto humanA = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::humanizeTiming, .amount = 0.1, .randomSeed = 7 }, 4.0);
    const auto humanB = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::humanizeTiming, .amount = 0.1, .randomSeed = 7 }, 4.0);
    bool moved = false;
    for (std::size_t i = 0; i < humanA.size(); ++i)
    {
        REQUIRE (humanA[i].startBeats == humanB[i].startBeats);
        REQUIRE (std::abs (humanA[i].startBeats - sampleNotes()[i].startBeats) <= 0.1 + 1.0e-9);
        moved = moved || humanA[i].startBeats != sampleNotes()[i].startBeats;
    }
    REQUIRE (moved);

    const auto velocityHuman = MidiClipService::transform (sampleNotes(), { .function = MidiFunction::humanizeVelocity, .amount = 10.0, .randomSeed = 3 }, 4.0);
    for (std::size_t i = 0; i < velocityHuman.size(); ++i)
        REQUIRE (std::abs (velocityHuman[i].velocity - sampleNotes()[i].velocity) <= 10);

    auto doubled = sampleNotes();
    doubled.push_back ({ 4, 60, 0.001, 0.25, 50 });
    REQUIRE (MidiClipService::transform (doubled, { .function = MidiFunction::deleteDoubles }, 4.0).size() == 4);
}

TEST_CASE ("B3a: Funktionen im Projekt und Listen-Editor", "[services][ui][midi][b3]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    MidiClipService midiClips { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "MidiTools").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
    const auto clipId = *midiClips.createEmptyClip (*track, 2.0, 4.0); // ab Takt 2, zwei Takte
    for (const auto& note : sampleNotes())
        REQUIRE (midiClips.addNote (clipId, note.noteNumber, note.startBeats, note.lengthBeats, note.velocity).has_value());

    // Nur die Auswahl transponieren (Index 2 = G3), ein Undo-Schritt.
    REQUIRE (midiClips.applyFunction (clipId, { .function = MidiFunction::transpose, .semitones = -12 }, { 2 }).has_value());
    REQUIRE (env.projectService.getUndoHistory().done.back() == "Transpose");
    auto clip = *midiClips.getClip (clipId);
    REQUIRE (std::ranges::count (clip.notes, 55, &EditableNote::noteNumber) == 1);
    REQUIRE (std::ranges::count (clip.notes, 60, &EditableNote::noteNumber) == 2);

    REQUIRE (midiClips.addNote (clipId, 60, 0.0, 0.5, 30).has_value()); // doppelt
    REQUIRE (midiClips.applyFunction (clipId, { .function = MidiFunction::deleteDoubles }).has_value());
    REQUIRE (midiClips.getClip (clipId)->notes.size() == 4);

    // Listen-Editor: sortiert nach Anfang, Position im Projekt (Takt 2 = Clip-Anfang bei 2 s, 120 BPM).
    fk::ui::ListEditor list { midiClips, env.timelineService };
    list.setSize (600, 300);
    list.setClip (clipId);
    REQUIRE (list.getNumRows() == 4);
    REQUIRE (list.getCellText (0, fk::ui::ListEditor::positionColumn) == "002.01.000");
    REQUIRE (list.getCellText (0, fk::ui::ListEditor::noteColumn) == "C3 (60)");

    // Zelle bearbeiten: Note als Name, Position, Länge, Anschlag; Unsinn wird abgelehnt.
    REQUIRE (list.editCell (0, fk::ui::ListEditor::velocityColumn, "45"));
    REQUIRE (list.editCell (0, fk::ui::ListEditor::lengthColumn, "0,75"));
    REQUIRE_FALSE (list.editCell (0, fk::ui::ListEditor::noteColumn, "H9x"));
    REQUIRE_FALSE (list.editCell (0, fk::ui::ListEditor::positionColumn, "abc"));
    REQUIRE (list.editCell (3, fk::ui::ListEditor::positionColumn, "3.2.240")); // letzte Note → Takt 3, Schlag 2,5
    REQUIRE (list.editCell (0, fk::ui::ListEditor::noteColumn, "F#2"));

    clip = *midiClips.getClip (clipId);
    REQUIRE (std::ranges::count (clip.notes, 54, &EditableNote::noteNumber) == 1); // F#2 = 54
    const auto moved = std::ranges::find_if (clip.notes, [] (const auto& n) { return std::abs (n.startBeats - 5.5) < 1.0e-6; });
    // Takt 3, Schlag 2,5 = Schlag 9,5 im Projekt; der Clip beginnt bei Schlag 4 → 5,5 im Clip.
    REQUIRE (moved != clip.notes.end());

    // Notennamen: Mitte C = C3 = 60.
    REQUIRE (fk::ui::ListEditor::parseNoteName ("C3") == 60);
    REQUIRE (fk::ui::ListEditor::parseNoteName ("Bb2") == 58);
    REQUIRE (fk::ui::ListEditor::parseNoteName ("C-2") == 0);
    REQUIRE (fk::ui::ListEditor::parseNoteName ("64") == 64);
    REQUIRE (fk::ui::ListEditor::parseNoteName ("C3 (60)") == 60);
    REQUIRE_FALSE (fk::ui::ListEditor::parseNoteName ("X1").has_value());
    REQUIRE_FALSE (fk::ui::ListEditor::parseNoteName ("200").has_value());

    // Löschen über die Auswahl.
    list.selectRow (0);
    list.deleteSelected();
    REQUIRE (midiClips.getClip (clipId)->notes.size() == 3);
}

TEST_CASE ("B3c: Quantisieren mit Stärke, Swing und Groove", "[services][midi][b3]")
{
    fk::services::QuantiseSettings settings;
    settings.gridBeats = 0.5;
    REQUIRE (MidiClipService::quantisedStart (0.6, settings) == Catch::Approx (0.5));

    settings.strength = 0.5;
    REQUIRE (MidiClipService::quantisedStart (0.6, settings) == Catch::Approx (0.55)); // halber Weg

    settings.strength = 1.0;
    settings.swing = 1.0; // jeder zweite Schritt um ein Drittel nach hinten
    REQUIRE (MidiClipService::quantisedStart (0.45, settings) == Catch::Approx (0.5 + 0.5 / 3.0));
    REQUIRE (MidiClipService::quantisedStart (1.05, settings) == Catch::Approx (1.0)); // gerader Schritt bleibt

    settings.swing = 0.0;
    settings.groove = { 0.0, 0.1 };
    REQUIRE (MidiClipService::quantisedStart (1.48, settings) == Catch::Approx (1.6)); // Schritt 3 → zweiter Groove-Wert

    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    MidiClipService midiClips { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Groove").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));

    // Groove-Vorlage: Achtel, jede zweite 0,05 Schläge zu spät.
    const auto grooveClip = *midiClips.createEmptyClip (*track, 0.0, 2.0);
    for (int i = 0; i < 8; ++i)
        REQUIRE (midiClips.addNote (grooveClip, 42, i * 0.5 + (i % 2 == 1 ? 0.05 : 0.0), 0.25, 100).has_value());

    const auto groove = midiClips.extractGroove (grooveClip, 0.5, 8);
    REQUIRE (groove.has_value());
    REQUIRE ((*groove)[0] == Catch::Approx (0.0));
    REQUIRE ((*groove)[1] == Catch::Approx (0.05));

    // Auf einen anderen Clip anwenden.
    const auto target = *midiClips.createEmptyClip (*track, 4.0, 2.0);
    REQUIRE (midiClips.addNote (target, 60, 0.52, 0.25, 100).has_value());
    settings = {};
    settings.gridBeats = 0.5;
    settings.groove = *groove;
    REQUIRE (midiClips.quantise (target, settings).has_value());
    REQUIRE (midiClips.getClip (target)->notes.front().startBeats == Catch::Approx (0.55));

    REQUIRE (midiClips.listClips().size() == 2);
}

TEST_CASE ("B3c: Step-Eingabe setzt Noten und Akkorde am Cursor", "[services][midi][b3]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    MidiClipService midiClips { env.projectService };
    fk::services::StepInputService stepInput { env.projectService, midiClips };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Step").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0);

    int changes = 0;
    stepInput.onChanged = [&changes] { ++changes; };
    REQUIRE_FALSE (stepInput.start ("gibt-es-nicht", 0.0, 0.25).has_value());
    REQUIRE (stepInput.start (clipId, 0.0, 0.5).has_value());
    REQUIRE (stepInput.isActive());

    // Einzelne Taste: Note bei 0, Cursor auf 0,5.
    stepInput.keysChanged ({ 60 }, { 90 }, {});
    stepInput.keysChanged ({}, {}, { 60 });
    REQUIRE (stepInput.getPositionBeats() == Catch::Approx (0.5));

    // Akkord: zweite Taste kommt, während die erste noch gehalten ist → gleiche Stelle.
    stepInput.keysChanged ({ 64 }, { 100 }, {});
    stepInput.keysChanged ({ 67 }, { 100 }, {});
    stepInput.keysChanged ({}, {}, { 64, 67 });
    REQUIRE (stepInput.getPositionBeats() == Catch::Approx (1.0));

    // Pause und zurück.
    stepInput.advance();
    REQUIRE (stepInput.getPositionBeats() == Catch::Approx (1.5));
    stepInput.advance (-2);
    REQUIRE (stepInput.getPositionBeats() == Catch::Approx (0.5));

    const auto clip = *midiClips.getClip (clipId);
    REQUIRE (clip.notes.size() == 3);
    for (const auto& note : clip.notes)
    {
        REQUIRE (note.lengthBeats == Catch::Approx (0.5));
        REQUIRE (note.startBeats == Catch::Approx (note.noteNumber == 60 ? 0.0 : 0.5));
    }

    stepInput.stop();
    REQUIRE_FALSE (stepInput.isActive());
    stepInput.keysChanged ({ 72 }, { 100 }, {}); // aus: nichts passiert
    REQUIRE (midiClips.getClip (clipId)->notes.size() == 3);
    REQUIRE (changes > 0);
}
