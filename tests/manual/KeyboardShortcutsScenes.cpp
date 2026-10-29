#include "fk/services/ShortcutService.h"
#include "fk/ui/Commands.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/ShortcutEditor.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"
#include "ManualShot.h"

#include <map>
#include <vector>

// Bilder für docs/manual/keyboard-shortcuts.md. Ändert sich der Shortcut-Editor, hier die Szene anpassen
// und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).
//
// Die Szenen ändern Tastenbelegungen. TestEnvironment nutzt eigene Einstellungen ("FKAudioSuiteTests"),
// nicht die des Nutzers; trotzdem merkt sich ShortcutStoreGuard die vorhandenen Abweichungen und stellt
// sie am Ende wieder her.

using fk::tests::TestEnvironment;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::saveShot;
using fk::ui::Command;

namespace
{
/// Merkt sich die gespeicherten Abweichungen, startet mit der Standardbelegung und stellt am Ende her.
struct ShortcutStoreGuard
{
    explicit ShortcutStoreGuard (fk::services::ShortcutService& shortcutStore)
        : store (shortcutStore)
        , saved (shortcutStore.getOverrides())
    {
        store.clearAll();
    }

    ~ShortcutStoreGuard()
    {
        store.clearAll();

        for (const auto& [commandKey, keyDescriptions] : saved)
            store.setOverride (commandKey, keyDescriptions);
    }

    fk::services::ShortcutService& store;
    std::map<juce::String, juce::StringArray> saved;

    JUCE_DECLARE_NON_COPYABLE (ShortcutStoreGuard)
};

/// Standardbelegung (Cubase-Layout) in eigenen Test-Einstellungen, Look and Feel der DAW.
struct ShortcutScene
{
    ShortcutScene() { juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel); }
    ~ShortcutScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::ShortcutService store { env.engineContext };
    ShortcutStoreGuard guard { store };
    fk::ui::KeyBindings bindings { store };
};

/// Direkte Kinder eines Typs, in der Reihenfolge, in der der Editor sie hinzufügt.
template <typename ComponentType>
std::vector<ComponentType*> shortcutChildrenOfType (juce::Component& root)
{
    std::vector<ComponentType*> found;

    for (auto* child : root.getChildren())
        if (auto* typed = dynamic_cast<ComponentType*> (child))
            found.push_back (typed);

    return found;
}

juce::TextEditor& shortcutSearchField (fk::ui::ShortcutEditor& editor)
{
    const auto fields = shortcutChildrenOfType<juce::TextEditor> (editor);
    INFO ("Suchfeld des Shortcut-Editors nicht gefunden");
    REQUIRE (fields.size() == 1);
    return *fields.front();
}

juce::ListBox& shortcutCommandList (fk::ui::ShortcutEditor& editor)
{
    auto* list = dynamic_cast<juce::ListBox*> (&findByTooltip (editor, "Double-click: record a new key"));
    REQUIRE (list != nullptr);
    return *list;
}

/// Die Tastenliste rechts hat keinen Tooltip (die Befehlsliste schon).
juce::ListBox& shortcutKeyList (fk::ui::ShortcutEditor& editor)
{
    for (auto* list : shortcutChildrenOfType<juce::ListBox> (editor))
        if (list->getTooltip().isEmpty())
            return *list;

    INFO ("Tastenliste des Shortcut-Editors nicht gefunden");
    FAIL();
    return shortcutCommandList (editor); // nie erreicht
}

/// Labels: zuerst der Titel des gewählten Befehls, dann der Hinweis (Reihenfolge im Konstruktor).
std::vector<juce::Label*> shortcutLabels (fk::ui::ShortcutEditor& editor)
{
    auto labels = shortcutChildrenOfType<juce::Label> (editor);
    INFO ("Titel und Hinweis des Shortcut-Editors nicht gefunden");
    REQUIRE (labels.size() == 2);
    return labels;
}

/// Markierung für ein breites, flaches Element (Button, Feld): Die Nummer steht innen am rechten Ende,
/// denn rechts daneben ist kein Platz (Bildrand oder rechte Spalte).
Callout shortcutCalloutInside (int number, juce::Component& root, juce::Component& target)
{
    auto callout = calloutFor (number, root, target, Callout::Placement::inside);
    callout.target = callout.target.withTrimmedRight (26);
    return callout;
}

/// Markierung für eine Zeile der Befehlsliste: nur der Bereich des Namens, damit die Nummer zwischen
/// Name und Taste sitzt (Spalten wie ShortcutEditor::paintListBoxItem: Kategorie 8–98, Name ab 100,
/// Taste in den rechten 125 Pixeln).
Callout shortcutRowCallout (int number, fk::ui::ShortcutEditor& editor, juce::ListBox& list, int row)
{
    const auto rowArea = list.getRowPosition (row, true);
    const auto nameArea = rowArea.withLeft (rowArea.getX() + 96).withRight (rowArea.getRight() - 150);
    const auto inEditor = editor.getLocalArea (&list, nameArea).getIntersection (editor.getLocalArea (&list, list.getLocalBounds()));
    return { number, inEditor, Callout::Placement::inside };
}
} // namespace

TEST_CASE ("Handbuch: Shortcut-Editor Überblick mit nummerierten Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ShortcutScene scene;
    fk::ui::ShortcutEditor editor (scene.bindings);
    REQUIRE (editor.getWidth() <= 1000);

    // Redo hat zwei Standardtasten (Ctrl+Y, Ctrl+Shift+Z): Die Tastenliste zeigt beide.
    editor.selectCommand (Command::redo);
    TestEnvironment::pumpMessages (100);
    REQUIRE (scene.bindings.keysFor (Command::redo).size() == 2);

    const auto labels = shortcutLabels (editor);

    // Nummern wie in der Tabelle in keyboard-shortcuts.md, Abschnitt "The shortcut editor at a glance".
    std::vector<Callout> callouts;
    callouts.push_back (shortcutCalloutInside (1, editor, shortcutSearchField (editor)));
    callouts.push_back (calloutFor (2, editor, shortcutCommandList (editor), Callout::Placement::inside));
    callouts.push_back (shortcutCalloutInside (3, editor, *labels[0]));
    callouts.push_back (calloutFor (4, editor, shortcutKeyList (editor), Callout::Placement::inside));
    callouts.push_back (shortcutCalloutInside (5, editor, findByTooltip (editor, "Then press the key you want")));
    callouts.push_back (shortcutCalloutInside (6, editor, findByTooltip (editor, "Remove the selected key")));
    callouts.push_back (shortcutCalloutInside (7, editor, findByTooltip (editor, "Reset this command")));
    callouts.push_back (calloutFor (8, editor, *labels[1], Callout::Placement::inside));
    callouts.push_back (shortcutCalloutInside (9, editor, findByTooltip (editor, "Discard all custom changes")));

    saveShot (editor, "keyboard-shortcuts/overview", callouts);
}

TEST_CASE ("Handbuch: Shortcut-Editor Suche", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ShortcutScene scene;
    fk::ui::ShortcutEditor editor (scene.bindings);

    // "Zoom" findet die Kategorie Zoom (auch "Show Entire Project") und das Zoom-Werkzeug.
    editor.setSearchText ("Zoom");
    editor.selectCommand (Command::zoomIn);
    TestEnvironment::pumpMessages (100);

    if (editor.getNumRows() != 5)
        WARN ("Suche \"Zoom\" findet " << editor.getNumRows() << " Befehle, der Text nennt 5");

    saveShot (editor, "keyboard-shortcuts/search");
}

TEST_CASE ("Handbuch: Shortcut-Editor wartet auf eine Taste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ShortcutScene scene;
    fk::ui::ShortcutEditor editor (scene.bindings);

    // "Overview Bar On/Off" hat keine Standardtaste: leere Tastenliste, dann "Add Key..." geklickt.
    editor.selectCommand (Command::overview);
    editor.startCapture();
    TestEnvironment::pumpMessages (100);
    REQUIRE (editor.isCapturing());

    const auto labels = shortcutLabels (editor);
    std::vector<Callout> callouts;
    callouts.push_back (shortcutCalloutInside (1, editor, findByTooltip (editor, "Then press the key you want")));
    callouts.push_back (calloutFor (2, editor, *labels[1], Callout::Placement::inside));

    saveShot (editor, "keyboard-shortcuts/capture", callouts);

    // Abbrechen wie mit Esc: Die Belegung bleibt unverändert.
    REQUIRE (editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (scene.bindings.keysFor (Command::overview).empty());
}

TEST_CASE ("Handbuch: Shortcut-Editor Taste war schon belegt", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ShortcutScene scene;
    fk::ui::ShortcutEditor editor (scene.bindings);

    // Suche "Tool" zeigt die acht Werkzeuge: Select Tool (Zeile 0), Range Tool (Zeile 1), ...
    editor.setSearchText ("Tool");
    TestEnvironment::pumpMessages (50);
    REQUIRE (editor.getNumRows() == 8);

    // Range Tool bekommt zusätzlich "1"; Select Tool verliert die Taste, der Hinweis nennt es.
    editor.selectCommand (Command::toolRange);
    editor.startCapture();
    REQUIRE (editor.keyPressed (juce::KeyPress ('1')));
    TestEnvironment::pumpMessages (100);

    REQUIRE (scene.bindings.keysFor (Command::toolRange).size() == 2);
    REQUIRE (scene.bindings.keysFor (Command::toolSelect).empty());
    REQUIRE (editor.getMessage().contains (fk::ui::commandName (Command::toolSelect)));

    auto& list = shortcutCommandList (editor);
    const auto labels = shortcutLabels (editor);

    std::vector<Callout> callouts;
    callouts.push_back (shortcutRowCallout (1, editor, list, 1));
    callouts.push_back (shortcutRowCallout (2, editor, list, 0));
    callouts.push_back (calloutFor (3, editor, shortcutKeyList (editor), Callout::Placement::inside));
    callouts.push_back (calloutFor (4, editor, *labels[1], Callout::Placement::inside));

    saveShot (editor, "keyboard-shortcuts/conflict", callouts);

    scene.bindings.resetAll(); // Szene hinterlässt nichts, der Guard stellt danach den alten Stand her
}
