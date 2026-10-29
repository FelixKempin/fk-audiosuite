#include "fk/plugins/PluginCatalog.h"
#include "fk/plugins/ui/ModulePanel.h"
#include "fk/plugins/ui/PanelEditor.h"
#include "fk/ui/InstrumentBrowser.h"
#include "fk/ui/PluginLibraryPanel.h"
#include "fk/ui/PluginManagerWindow.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <memory>
#include <vector>

// Bilder für docs/manual/instruments-and-effects.md. Ändern sich Plugin-Manager, Instrumenten-Browser
// oder die Oberfläche eines eingebauten Plugins (describePanel), hier die Szenen anpassen und
// tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;

namespace
{
/// Alle Kinder vom Typ T in beliebiger Tiefe, in Kind-Reihenfolge.
template <typename ComponentType>
void collectChildren (juce::Component& parent, std::vector<ComponentType*>& found)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* typed = dynamic_cast<ComponentType*> (child))
            found.push_back (typed);

        collectChildren (*child, found);
    }
}

/// Sichtbares direktes Kind-Label, dessen Text so beginnt (Labels haben keinen Tooltip).
juce::Label& labelStartingWith (juce::Component& parent, const juce::String& textStart)
{
    juce::Label* found = nullptr;

    for (auto* child : parent.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child); label != nullptr && label->isVisible())
            if (found == nullptr && label->getText().startsWith (textStart))
                found = label;

    INFO ("Kein sichtbares Label \"" << textStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Sichtbarer Text-Button mit genau dieser Beschriftung, beliebig tief (JUCEs "Options..." hat keinen Tooltip).
juce::TextButton& textButtonNamed (juce::Component& root, const juce::String& text)
{
    std::vector<juce::TextButton*> buttons;
    collectChildren (root, buttons);
    juce::TextButton* found = nullptr;

    for (auto* button : buttons)
        if (found == nullptr && button->isVisible() && button->getButtonText() == text)
            found = button;

    INFO ("Kein sichtbarer Button \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

juce::String pluginIdNamed (fk::services::PluginService& plugins, const juce::String& name)
{
    juce::String id;

    for (const auto& plugin : plugins.getAllPlugins())
        if (id.isEmpty() && plugin.name == name)
            id = plugin.id;

    INFO ("Kein Plugin \"" << name << "\"");
    REQUIRE (id.isNotEmpty());
    return id;
}

/// Favoriten und Ausblenden gelten für alle Projekte und stehen in den Engine-Einstellungen der Tests:
/// nach der Szene zurücksetzen, auch wenn ein REQUIRE vorher abbricht.
struct PluginPrefsGuard
{
    fk::services::PluginService& plugins;
    std::vector<juce::String> ids;

    ~PluginPrefsGuard()
    {
        for (const auto& id : ids)
        {
            (void) plugins.setFavourite (id, false);
            (void) plugins.setHidden (id, false);
        }
    }
};

/// Ein Eintrag der Sperrliste für das Bild (rote Zeile im Reiter "Search and Scan"); danach wieder weg.
struct BlocklistGuard
{
    juce::KnownPluginList& list;
    juce::String file;

    BlocklistGuard (juce::KnownPluginList& knownPlugins, juce::String path)
        : list (knownPlugins)
        , file (std::move (path))
    {
        list.addToBlacklist (file);
    }

    ~BlocklistGuard()
    {
        list.removeFromBlacklist (file);
        list.sendSynchronousChangeMessage(); // Tracktion speichert die Liste sofort, nicht erst in der Ereignisschleife
    }

    BlocklistGuard (const BlocklistGuard&) = delete;
    BlocklistGuard& operator= (const BlocklistGuard&) = delete;
};

/// Ein eingebautes Plugin mit seiner Oberfläche, erzeugt wie vom Host (createEditorAndMakeActive).
/// Der Prozessor steht vor dem Editor: Er wird zuletzt abgebaut.
struct PluginPanel
{
    explicit PluginPanel (const juce::String& name)
    {
        for (const auto& description : fk::plugins::PluginCatalog::getDescriptions())
            if (processor == nullptr && description.name == name)
                processor = fk::plugins::PluginCatalog::createInstance (description);

        INFO ("Kein eingebautes Plugin \"" << name << "\"");
        REQUIRE (processor != nullptr);

        auto* created = processor->createEditorAndMakeActive();
        editor.reset (dynamic_cast<fk::plugins::ui::PanelEditor*> (created));
        REQUIRE (editor != nullptr);
    }

    ~PluginPanel()
    {
        if (editor != nullptr)
        {
            processor->editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    PluginPanel (const PluginPanel&) = delete;
    PluginPanel& operator= (const PluginPanel&) = delete;

    std::unique_ptr<juce::AudioPluginInstance> processor;
    std::unique_ptr<fk::plugins::ui::PanelEditor> editor;
};

/// Oberfläche eines eingebauten Plugins, bereit zum Fotografieren: Jeder Abschnitt (ModulePanel) bekommt
/// seine Nummer in Reihenfolge, wie die Tabelle des Plugins in instruments-and-effects.md §14.
/// Die JUCE-Umgebung steht vor dem Plugin: Sie wird zuletzt abgebaut.
struct PanelShot
{
    PanelShot (const juce::String& pluginName, std::size_t expectedSections)
        : panel (pluginName)
    {
        TestEnvironment::pumpMessages (200); // Statuszeilen und Anzeigen lesen ihren ersten Wert

        std::vector<fk::plugins::ui::ModulePanel*> sections;
        collectChildren (*panel.editor, sections);
        INFO (pluginName << ": Abschnitte im Editor");
        REQUIRE (sections.size() == expectedSections);

        for (std::size_t i = 0; i < sections.size(); ++i)
            callouts.push_back (
                calloutFor (static_cast<int> (i) + 1, *panel.editor, *sections[i], Callout::Placement::inside));
    }

    juce::Component& editor() { return *panel.editor; }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    PluginPanel panel;
    std::vector<Callout> callouts;
};

void skipWithoutImageFolder()
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");
}
} // namespace

TEST_CASE ("Handbuch: Plugin Manager, Reiter My Plugins", "[manual]")
{
    skipWithoutImageFolder();

    ProjectWindowScene scene;
    auto& plugins = scene.services.plugins;
    const auto compressorId = pluginIdNamed (plugins, "FK Compressor");
    const auto gainId = pluginIdNamed (plugins, "FK Gain");
    PluginPrefsGuard guard { plugins, { compressorId, gainId } };
    REQUIRE (plugins.setFavourite (compressorId, true).has_value());
    REQUIRE (plugins.setHidden (gainId, true).has_value());

    fk::ui::PluginManagerWindow window (plugins);
    auto* tabs = dynamic_cast<juce::TabbedComponent*> (window.getContentComponent());
    REQUIRE (tabs != nullptr);
    tabs->setCurrentTabIndex (0);
    TestEnvironment::pumpMessages (200);

    auto& panel = window.getLibraryPanel();
    REQUIRE (panel.getNumRows() > 0);

    // FIX F-1502 (Suche über die angezeigten Kategorienamen; Bild bleibt gleich)
    // FIX F-1504 (Kategorie-Auswahl zeigt "automatic"; die Zelle sähe anders aus)
    // Nummern wie die Tabelle in instruments-and-effects.md §3.
    saveShot (*tabs,
              "instruments-and-effects/plugin-manager-my-plugins",
              { calloutFor (1, *tabs, tabs->getTabbedButtonBar(), Callout::Placement::inside),
                calloutFor (2, *tabs, findChild<juce::TextEditor> (panel), Callout::Placement::inside),
                calloutFor (3, *tabs, findByTooltip (panel, "Click "), Callout::Placement::inside),
                calloutFor (4, *tabs, findByTooltip (panel, "Category in the plugin menu"), Callout::Placement::inside),
                calloutFor (5, *tabs, labelStartingWith (panel, "Favorites are"), Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Plugin Manager, Reiter Search and Scan", "[manual]")
{
    skipWithoutImageFolder();

    ProjectWindowScene scene;
    auto& plugins = scene.services.plugins;
    BlocklistGuard blocked (plugins.getKnownPluginList(), "/home/user/.vst3/Broken Example.vst3");

    fk::ui::PluginManagerWindow window (plugins);
    auto* tabs = dynamic_cast<juce::TabbedComponent*> (window.getContentComponent());
    REQUIRE (tabs != nullptr);
    tabs->setCurrentTabIndex (1);
    TestEnvironment::pumpMessages (300);

    auto* list = dynamic_cast<juce::PluginListComponent*> (tabs->getTabContentComponent (1));
    REQUIRE (list != nullptr);
    REQUIRE (list->isVisible());

    // FIX F-1501 (FK-Plugins in dieser Liste; nach dem Fix fehlen sie oder sind geschützt)
    // FIX F-1502 (Kategorie-Spalte zeigt interne deutsche Namen und FK Trim)
    // Nummern wie die Tabelle in instruments-and-effects.md §4.
    saveShot (*tabs,
              "instruments-and-effects/plugin-manager-search-scan",
              { calloutFor (1, *tabs, tabs->getTabbedButtonBar(), Callout::Placement::inside),
                calloutFor (2, *tabs, findChild<juce::TableListBox> (*list), Callout::Placement::inside),
                calloutFor (3, *tabs, textButtonNamed (*list, "Options..."), Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Instrumenten-Browser mit nummerierten Elementen", "[manual]")
{
    skipWithoutImageFolder();

    ProjectWindowScene scene;
    auto& plugins = scene.services.plugins;
    auto& library = scene.services.library;
    const auto synthId = pluginIdNamed (plugins, "FK Synth");
    PluginPrefsGuard guard { plugins, { synthId } };
    REQUIRE (library.setFavourite ("plugin:" + synthId, true).has_value());
    REQUIRE (library.addTag ("plugin:" + synthId, "Pads").has_value());

    fk::ui::InstrumentBrowser browser (library);
    browser.setSize (900, 240);
    browser.refresh();
    TestEnvironment::pumpMessages (200);
    REQUIRE (! browser.getItems().empty());

    auto& list = findChild<juce::ListBox> (browser);
    list.selectRow (0);
    TestEnvironment::pumpMessages (100);

    // FIX F-1508 (Platzhalter der Suche)
    // Nummern wie die Tabelle in instruments-and-effects.md §6.
    saveShot (browser,
              "instruments-and-effects/instrument-browser",
              { calloutFor (1, browser, findChild<juce::TextEditor> (browser), Callout::Placement::above),
                calloutFor (2, browser, findByTooltip (browser, "Filter by tag"), Callout::Placement::above),
                calloutFor (3, browser, findByTooltip (browser, "Show favorites only"), Callout::Placement::above),
                calloutFor (4, browser, findByTooltip (browser, "New track with the selected entry"), Callout::Placement::above),
                calloutFor (5, browser, findByTooltip (browser, "Content Manager: download"), Callout::Placement::above),
                calloutFor (6, browser, list, Callout::Placement::inside) });
}

// Eingebaute Plugins: je ein Bild der Oberfläche, Abschnitte nummeriert (instruments-and-effects.md §14).

TEST_CASE ("Handbuch: Oberfläche FK Synth", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Synth", 5);
    saveShot (shot.editor(), "instruments-and-effects/fk-synth", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Sampler", "[manual]")
{
    skipWithoutImageFolder();
    // FIX F-1509 (Fehler beim Laden sichtbar; die Statuszeile ohne Datei bleibt gleich)
    PanelShot shot ("FK Sampler", 2);
    saveShot (shot.editor(), "instruments-and-effects/fk-sampler", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK EQ", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK EQ", 6);
    saveShot (shot.editor(), "instruments-and-effects/fk-eq", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Compressor", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Compressor", 4);
    saveShot (shot.editor(), "instruments-and-effects/fk-compressor", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Limiter", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Limiter", 1);
    saveShot (shot.editor(), "instruments-and-effects/fk-limiter", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Gate", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Gate", 2);
    saveShot (shot.editor(), "instruments-and-effects/fk-gate", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Distortion", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Distortion", 3);
    saveShot (shot.editor(), "instruments-and-effects/fk-distortion", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Delay", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Delay", 3);
    saveShot (shot.editor(), "instruments-and-effects/fk-delay", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Reverb", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Reverb", 3);
    saveShot (shot.editor(), "instruments-and-effects/fk-reverb", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Gain", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Gain", 1);
    saveShot (shot.editor(), "instruments-and-effects/fk-gain", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Transpose", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Transpose", 1);
    saveShot (shot.editor(), "instruments-and-effects/fk-transpose", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Chord", "[manual]")
{
    skipWithoutImageFolder();
    PanelShot shot ("FK Chord", 1);
    saveShot (shot.editor(), "instruments-and-effects/fk-chord", shot.callouts);
}

TEST_CASE ("Handbuch: Oberfläche FK Arpeggiator", "[manual]")
{
    skipWithoutImageFolder();
    // FIX F-1510 (Reihenfolge der Notenwerte im Menü; das Bild zeigt nur den gewählten Wert)
    PanelShot shot ("FK Arpeggiator", 1);
    saveShot (shot.editor(), "instruments-and-effects/fk-arpeggiator", shot.callouts);
}
