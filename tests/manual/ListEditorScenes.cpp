#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/ListEditor.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"
#include "ManualShot.h"

#include <array>
#include <functional>

// Bilder für docs/manual/list-editor.md. Ändert sich der List Editor, hier die Szene anpassen und
// tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::saveShot;

namespace
{
/// Erstes sichtbares Label (beliebig tief), dessen Text so beginnt. Labels haben keinen Tooltip,
/// darum nicht über findByTooltip.
juce::Label& findLabelByText (juce::Component& root, const juce::String& textStart)
{
    std::function<juce::Label*(juce::Component&)> search = [&] (juce::Component& parent) -> juce::Label*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->isVisible() && label->getText().startsWith (textStart))
                    return label;

            if (auto* found = search (*child))
                return found;
        }

        return nullptr;
    };

    auto* found = search (root);
    INFO ("Kein sichtbares Label mit Text \"" << textStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Ein Projekt (120 BPM, 4/4) mit Synth-Spur und einem zweitaktigen Clip ab Takt 2: Akkord, kleine
/// Melodie, verschiedene Anschläge. Der Clip heißt "MIDI" (Vorgabe von createEmptyClip).
struct ListEditorScene
{
    ListEditorScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Manual").has_value());
        track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
        clipId = *midiClips.createEmptyClip (*track, 2.0, 4.0); // Takt 2 und 3

        struct Note
        {
            int number;
            double start;
            double length;
            int velocity;
        };

        const std::array<Note, 10> notes { { { 60, 0.0, 1.0, 100 },
                                             { 64, 0.0, 1.0, 92 },
                                             { 67, 0.0, 1.0, 88 },
                                             { 62, 1.0, 0.5, 80 },
                                             { 65, 1.5, 0.5, 76 },
                                             { 67, 2.0, 1.0, 104 },
                                             { 72, 3.0, 0.25, 70 },
                                             { 71, 3.25, 0.25, 66 },
                                             { 69, 3.5, 0.5, 90 },
                                             { 67, 4.0, 2.0, 110 } } };

        for (const auto& note : notes)
            REQUIRE (midiClips.addNote (clipId, note.number, note.start, note.length, note.velocity).has_value());
    }

    ~ListEditorScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};

juce::TableListBox& tableOf (fk::ui::ListEditor& editor)
{
    auto* table = dynamic_cast<juce::TableListBox*> (&findByTooltip (editor, "Double-click a cell"));
    REQUIRE (table != nullptr);
    return *table;
}

/// Spalte (sichtbarer Index) von der Kopfzeile bis zum unteren Rand der Tabelle, in Koordinaten des Editors.
juce::Rectangle<int> columnArea (fk::ui::ListEditor& editor, juce::TableListBox& table, int visibleIndex)
{
    auto& header = table.getHeader();
    const auto column = editor.getLocalArea (&header, header.getColumnPosition (visibleIndex));
    const auto tableArea = editor.getLocalArea (&table, table.getLocalBounds());
    return column.withBottom (tableArea.getBottom());
}
} // namespace

TEST_CASE ("Handbuch: List Editor ohne Clip", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ListEditorScene scene;
    fk::ui::ListEditor editor (scene.midiClips, scene.env.timelineService);
    editor.setSize (760, 140);
    TestEnvironment::pumpMessages (100);

    saveShot (editor, "list-editor/empty");
}

TEST_CASE ("Handbuch: List Editor Überblick mit nummerierten Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ListEditorScene scene;
    fk::ui::ListEditor editor (scene.midiClips, scene.env.timelineService);
    editor.setSize (760, 320);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);
    REQUIRE (editor.getNumRows() == 10);

    // Zwei Zeilen ausgewählt (D3 und F3, Zeilen 4 und 5), wie nach Klick und Shift+Klick rechts der Spalten.
    auto& table = tableOf (editor);
    table.selectRow (3);
    table.selectRow (4, false, false);
    TestEnvironment::pumpMessages (50);

    // Nummern wie in der Tabelle in list-editor.md, Abschnitt "The editor at a glance".
    std::vector<Callout> callouts;
    callouts.push_back (calloutFor (1, editor, findLabelByText (editor, "MIDI"), Callout::Placement::above));
    callouts.push_back (calloutFor (2, editor, findByTooltip (editor, "Delete selected notes"), Callout::Placement::above));

    for (int column = 0; column < 4; ++column)
        callouts.push_back ({ 3 + column, columnArea (editor, table, column), Callout::Placement::inside });

    const auto rows = table.getRowPosition (3, true).getUnion (table.getRowPosition (4, true));
    callouts.push_back ({ 7, editor.getLocalArea (&table, rows), Callout::Placement::inside });

    saveShot (editor, "list-editor/overview", callouts);
}

TEST_CASE ("Handbuch: List Editor Zelle bearbeiten", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ListEditorScene scene;
    fk::ui::ListEditor editor (scene.midiClips, scene.env.timelineService);
    editor.setSize (760, 200);
    editor.setClip (scene.clipId);
    editor.addToDesktop (0); // das Textfeld der Zelle nimmt den Tastaturfokus, das braucht ein Fenster
    editor.setTopLeftPosition (0, 0);
    editor.setVisible (true);
    TestEnvironment::pumpMessages (100);

    // Notenzelle der vierten Zeile (D3) im Bearbeitungsmodus, "E3" eingetippt, noch nicht übernommen.
    auto& table = tableOf (editor);
    auto* cell = dynamic_cast<juce::Label*> (table.getCellComponent (fk::ui::ListEditor::noteColumn, 3));

    if (cell == nullptr)
        WARN ("Notenzelle der Zeile 4 nicht gefunden; Bild zeigt keine Bearbeitung");
    else
    {
        cell->showEditor();

        if (auto* textEditor = cell->getCurrentTextEditor())
            textEditor->setText ("E3", false);

        TestEnvironment::pumpMessages (50);
    }

    // Ausschnitt: Tabelle links (die vier Spalten), ohne leere Fläche rechts.
    const auto area = editor.getLocalArea (&table, table.getLocalBounds());
    const auto columnsRight = columnArea (editor, table, 3).getRight();
    saveShot (editor, "list-editor/editing", {}, area.withWidth (columnsRight - area.getX() + 40));

    if (cell != nullptr)
        cell->hideEditor (true); // verwerfen, nichts ändern

    editor.removeFromDesktop();
}
