#include "fk/services/ExpressionMapService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/StepInputService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/KeyEditor.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"
#include "ManualShot.h"

// Bilder für docs/manual/key-editor.md. Ändert sich der Key Editor, hier die Szene anpassen und
// tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::mouseEvent;
using fk::tests::manual::saveShot;

namespace
{
/// Ein Projekt in G-Dur (Akkorde in Oktave 3, damit sie im Startausschnitt C5 abwärts liegen) mit Synth-Spur und einem viertaktigen leeren Clip; addProgression füllt I, IV, V7.
struct KeyEditorScene
{
    KeyEditorScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Manual").has_value());
        REQUIRE (signature.setKey (0, 7, fk::services::ScaleKind::major).has_value());
        track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
        clipId = *midiClips.createEmptyClip (*track, 0.0, 8.0);
    }

    ~KeyEditorScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    void addProgression()
    {
        using fk::services::ChordKind;
        using fk::services::ScaleKind;
        using fk::services::SignatureService;

        const std::array<std::pair<int, ChordKind>, 3> degrees { { { 1, ChordKind::triad },
                                                                   { 4, ChordKind::triad },
                                                                   { 5, ChordKind::seventh } } };

        for (std::size_t bar = 0; bar < degrees.size(); ++bar)
            REQUIRE (midiClips
                         .addChord (clipId,
                                    SignatureService::getChordNotes (7, ScaleKind::major, degrees[bar].first, degrees[bar].second, 0, 3),
                                    4.0 * static_cast<double> (bar),
                                    4.0,
                                    100)
                         .has_value());

        // Verschiedene Anschläge, damit die Velocity-Lane Balken unterschiedlicher Höhe zeigt.
        const auto notes = midiClips.getClip (clipId)->notes;

        for (std::size_t i = 0; i < notes.size(); ++i)
            REQUIRE (midiClips.setNoteVelocity (clipId, notes[i].index, 40 + static_cast<int> (i) * 9).has_value());
    }

    fk::ui::KeyEditor::Services services()
    {
        return { midiClips, midiControllers, notePreview, transport, env.timelineService, signature, &stepInput, &expressionMaps };
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::MidiControllerService midiControllers { env.projectService };
    fk::services::NotePreviewService notePreview { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::SignatureService signature { env.projectService };
    fk::services::StepInputService stepInput { env.projectService, midiClips };
    fk::services::ExpressionMapService expressionMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("Maps") };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};
} // namespace

TEST_CASE ("Handbuch: Key Editor ohne Clip", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (900, 260);
    TestEnvironment::pumpMessages (100);

    saveShot (editor, "key-editor/empty");
}

TEST_CASE ("Handbuch: Key Editor Überblick mit nummerierten Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    scene.addProgression();
    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (1840, 460);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    // Nummern wie in der Tabelle in key-editor.md, Abschnitt "The editor at a glance". Zwei Bilder,
    // damit die Leiste im PDF lesbar bleibt: links Leiste bis Step mit den Flächen, rechts der Rest der Leiste.
    const std::array<const char*, 16> toolbar { "Select:", "Pencil:", "Eraser:", "Audition:",
                                                "Grid:", "Length of new notes", "Quantize:", "Quantize options",
                                                "MIDI functions", "Step input", "Expression map of the track", "Key at the clip start",
                                                "Degree relative", "Chord type", "Inversion", "Insert the chord" };
    const std::array<const char*, 4> areas { "Click a key to audition", "Click selects", "Velocity per note", "Add controller lane" };
    std::vector<Callout> left;
    std::vector<Callout> right;

    for (std::size_t i = 0; i < toolbar.size(); ++i)
        (i < 10 ? left : right).push_back (calloutFor (static_cast<int> (i) + 1, editor, findByTooltip (editor, toolbar[i]), Callout::Placement::above));

    for (std::size_t i = 0; i < areas.size(); ++i)
        left.push_back (calloutFor (static_cast<int> (toolbar.size() + i) + 1, editor, findByTooltip (editor, areas[i]), Callout::Placement::inside));

    // Linkes Bild endet hinter Step, rechtes beginnt vor der Expression-Map-Liste.
    const auto stepRight = editor.getLocalArea (&findByTooltip (editor, "Step input"), findByTooltip (editor, "Step input").getLocalBounds()).getRight();
    const auto mapLeft = editor.getLocalArea (&findByTooltip (editor, "Expression map of the track"),
                                              findByTooltip (editor, "Expression map of the track").getLocalBounds()).getX();
    saveShot (editor, "key-editor/overview", left, juce::Rectangle<int> (0, 0, stepRight + 12, editor.getHeight()));
    saveShot (editor, "key-editor/toolbar-chords", right,
              juce::Rectangle<int> (mapLeft - 8, 0, editor.getWidth() - mapLeft + 8, 36));
}

TEST_CASE ("Handbuch: Key Editor Auswahl und Anschlag", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    scene.addProgression();
    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (900, 420);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    // Die drei Noten des IV-Akkords (Takt 2) per Auswahlrechteck: Auswahlwerkzeug, dann im Raster ziehen.
    dynamic_cast<juce::Button&> (findByTooltip (editor, "Select:")).triggerClick();
    TestEnvironment::pumpMessages (50);

    auto& grid = dynamic_cast<fk::ui::NoteGrid&> (findByTooltip (editor, "Click selects"));
    const auto ppb = static_cast<float> (grid.getPixelsPerBeat());
    const auto rowY = [] (int noteNumber)
    { return static_cast<float> ((fk::ui::NoteGrid::numNotes - 1 - noteNumber) * fk::ui::NoteGrid::rowHeight); };
    // Rechteck von einer Zeile über dem höchsten bis eine unter dem tiefsten Ton des Akkords.
    int lowest = 127;
    int highest = 0;

    const auto clip = scene.midiClips.getClip (scene.clipId);
    REQUIRE (clip.has_value());

    for (const auto& note : clip->notes)
        if (std::abs (note.startBeats - 4.0) < 1.0e-6)
        {
            lowest = std::min (lowest, note.noteNumber);
            highest = std::max (highest, note.noteNumber);
        }

    const juce::Point<float> from { 4.0f * ppb + 2.0f, rowY (highest + 1) + 6.0f };
    const juce::Point<float> to { 6.0f * ppb, rowY (lowest - 1) + 6.0f };

    grid.mouseDown (mouseEvent (grid, from, from));
    grid.mouseDrag (mouseEvent (grid, to, from));
    grid.mouseUp (mouseEvent (grid, to, from));
    TestEnvironment::pumpMessages (100);
    REQUIRE (grid.getSelectedNotes().size() == 3);

    saveShot (editor, "key-editor/selection");
}

TEST_CASE ("Handbuch: Key Editor Step-Eingabe", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (1100, 380);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    // Drei Schritte gespielt (C, D, E), dann Step-Cursor an der vierten Stelle.
    dynamic_cast<juce::Button&> (findByTooltip (editor, "Step input")).triggerClick();
    TestEnvironment::pumpMessages (50);
    REQUIRE (scene.stepInput.isActive());

    for (const int note : { 60, 62, 64 })
    {
        scene.stepInput.keysChanged ({ note }, { 100 }, {});
        scene.stepInput.keysChanged ({}, {}, { note });
    }

    TestEnvironment::pumpMessages (100);

    saveShot (editor,
              "key-editor/step-input",
              { calloutFor (1, editor, findByTooltip (editor, "Step input"), Callout::Placement::above),
                calloutFor (2, editor, findByTooltip (editor, "Step cursor back"), Callout::Placement::above),
                calloutFor (3, editor, findByTooltip (editor, "Rest:"), Callout::Placement::above) });
    scene.stepInput.stop();
}

TEST_CASE ("Handbuch: Key Editor Controller-Lanes", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    scene.addProgression();

    // Sustain je Takt treten und lösen, Mod-Wheel als Rampe.
    using namespace fk::services::controllerTypes;

    for (int bar = 0; bar < 3; ++bar)
    {
        REQUIRE (scene.midiControllers.addControllerEvent (scene.clipId, sustain, bar * 4.0 + 0.25, 127).has_value());
        REQUIRE (scene.midiControllers.addControllerEvent (scene.clipId, sustain, bar * 4.0 + 3.75, 0).has_value());
    }

    for (int step = 0; step <= 12; ++step)
        REQUIRE (scene.midiControllers.addControllerEvent (scene.clipId, modWheel, step, step * 10).has_value());

    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (900, 520);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (50);

    // Erste Lane Sustain, zweite Mod-Wheel (Reihenfolge des Buttons).
    auto& addLane = dynamic_cast<juce::Button&> (findByTooltip (editor, "Add controller lane"));
    addLane.triggerClick();
    TestEnvironment::pumpMessages (50);
    addLane.triggerClick();
    TestEnvironment::pumpMessages (100);

    saveShot (editor,
              "key-editor/controller-lanes",
              { calloutFor (1, editor, findByTooltip (editor, "Which controller data"), Callout::Placement::inside),
                calloutFor (2, editor, findByTooltip (editor, "Remove this controller lane"), Callout::Placement::inside),
                calloutFor (3, editor, addLane, Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Key Editor Spielweisen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    KeyEditorScene scene;
    REQUIRE (scene.expressionMaps.setMapForTrack (*scene.track, fk::services::ExpressionMapService::orchestraStringsId).has_value());

    // Eine kurze Linie mit Legato, Staccato und Akzent.
    std::vector<fk::services::EditableNote> line;
    const std::array<int, 8> pitches { 55, 57, 59, 60, 62, 60, 59, 57 };

    for (std::size_t i = 0; i < pitches.size(); ++i)
        line.push_back ({ -1, pitches[i], static_cast<double> (i), 1.0, 90, {} });

    REQUIRE (scene.midiClips.addNotes (scene.clipId, line, "Line").has_value());
    const auto notes = scene.midiClips.getClip (scene.clipId)->notes;
    REQUIRE (scene.midiClips.setArticulation (scene.clipId, { notes[0].index, notes[1].index, notes[2].index, notes[3].index }, "legato").has_value());
    REQUIRE (scene.midiClips.setArticulation (scene.clipId, { notes[4].index, notes[5].index }, "stacc").has_value());
    REQUIRE (scene.midiClips.setArticulation (scene.clipId, { notes[6].index }, "accent").has_value());

    fk::ui::KeyEditor editor (scene.services());
    editor.setSize (1240, 640);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    saveShot (editor,
              "key-editor/articulations",
              { calloutFor (1, editor, findByTooltip (editor, "Expression map of the track"), Callout::Placement::above),
                calloutFor (2, editor, findByTooltip (editor, "Articulations:"), Callout::Placement::inside) });
}
