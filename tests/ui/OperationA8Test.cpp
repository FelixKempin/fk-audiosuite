#include "fk/services/AutomationService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MixerService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/RackService.h"
#include "fk/services/SendService.h"
#include "fk/ui/Commands.h"
#include "fk/ui/Inspector.h"
#include "fk/services/PluginService.h"
#include "fk/ui/OverviewBar.h"
#include "fk/ui/PluginLibraryPanel.h"
#include "fk/ui/PluginMenu.h"
#include "fk/ui/ShortcutEditor.h"
#include "fk/ui/UndoHistoryPanel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

#include <set>

using fk::tests::TestEnvironment;

namespace
{
juce::MouseEvent localMouseEvent (juce::Component& target, juce::Point<float> position, juce::Point<float> down)
{
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &target,
             &target,
             juce::Time::getCurrentTime(),
             down,
             juce::Time::getCurrentTime(),
             1,
             position != down };
}
} // namespace

TEST_CASE ("A8a: Verlauf listet die Schritte und springt per Klick", "[services][ui][a8]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Verlauf").has_value());
    TestEnvironment::pumpMessages (300);

    for (const auto* name : { "Eins", "Zwei", "Drei" })
        REQUIRE (env.trackService.createAudioTrack (name, 1).has_value());

    const auto history = env.projectService.getUndoHistory();
    REQUIRE (history.done.size() == 3);
    REQUIRE (history.undone.empty());
    REQUIRE (history.done.front() == "Add track");

    fk::ui::UndoHistoryPanel panel { env.projectService };
    REQUIRE (panel.getNumRows() == 4); // Projektanfang + drei Schritte
    REQUIRE (panel.getCurrentRow() == 3);

    // Zurück auf den Stand nach dem ersten Schritt: eine Spur, zwei Schritte zum Wiederholen.
    panel.jumpTo (1);
    REQUIRE (env.trackService.getAudioTracks().size() == 1);
    REQUIRE (panel.getCurrentRow() == 1);
    REQUIRE (env.projectService.getUndoHistory().undone.size() == 2);
    REQUIRE (panel.getNumRows() == 4); // rückgängig gemachte Schritte bleiben sichtbar

    panel.jumpTo (3);
    REQUIRE (env.trackService.getAudioTracks().size() == 3);

    panel.jumpTo (0);
    REQUIRE (env.trackService.getAudioTracks().empty());

    // Außerhalb des Verlaufs wird begrenzt, nicht abgestürzt.
    env.projectService.jumpToUndoStep (99);
    REQUIRE (env.trackService.getAudioTracks().size() == 3);
}

TEST_CASE ("A8a: Übersichtsleiste scrollt und zoomt per Maus", "[ui][a8]")
{
    fk::ui::OverviewBar bar;
    bar.setSize (400, fk::ui::OverviewBar::height);
    bar.setContent ({}, 100.0); // 4 Pixel je Sekunde
    bar.setVisibleRange (0.0, 10.0);

    double from = -1.0, to = -1.0;
    bar.onRangeChanged = [&] (double f, double t)
    {
        from = f;
        to = t;
    };

    // Klick bei 50 s neben den Rahmen: Ausschnitt mittig dorthin.
    bar.mouseDown (localMouseEvent (bar, { 200.0f, 15.0f }, { 200.0f, 15.0f }));
    bar.mouseUp (localMouseEvent (bar, { 200.0f, 15.0f }, { 200.0f, 15.0f }));
    REQUIRE (from == Catch::Approx (45.0));
    REQUIRE (to == Catch::Approx (55.0));

    // Rechten Rand (55 s = 220 px) um 20 px nach rechts ziehen: 5 s breiter.
    bar.mouseDown (localMouseEvent (bar, { 220.0f, 15.0f }, { 220.0f, 15.0f }));
    bar.mouseDrag (localMouseEvent (bar, { 240.0f, 15.0f }, { 220.0f, 15.0f }));
    bar.mouseUp (localMouseEvent (bar, { 240.0f, 15.0f }, { 220.0f, 15.0f }));
    REQUIRE (from == Catch::Approx (45.0));
    REQUIRE (to == Catch::Approx (60.0));

    // Rahmen in der Mitte nach links ziehen: verschieben, Breite bleibt, nie vor 0.
    bar.mouseDown (localMouseEvent (bar, { 210.0f, 15.0f }, { 210.0f, 15.0f }));
    bar.mouseDrag (localMouseEvent (bar, { 0.0f, 15.0f }, { 210.0f, 15.0f }));
    REQUIRE (from == Catch::Approx (0.0));
    REQUIRE (to == Catch::Approx (15.0));
}

TEST_CASE ("A8a: Zoom auf Projekt, Auswahl und Zoom-Presets", "[ui][a8]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Zoom").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    const auto wav = env.createSilentWav ("zehn", 10.0);
    REQUIRE (env.clipService.importAudioFile (*track, wav, 0.0).has_value());
    const auto late = *env.clipService.importAudioFile (*track, wav, 30.0);

    fk::services::MarkerService markers { env.projectService };
    fk::ui::ArrangeView view ({ env.trackService,
                                env.clipService,
                                midiClips,
                                transport,
                                env.timelineService,
                                waveforms,
                                mixer,
                                automation,
                                signatureService,
                                markers });
    view.setSize (1000, 400);
    TestEnvironment::pumpMessages (50);

    view.showTimeRange (5.0, 25.0);
    auto [from, to] = view.getVisibleRange();
    REQUIRE (from == Catch::Approx (5.0));
    REQUIRE (to == Catch::Approx (25.0).margin (0.01));

    view.zoomToProject();
    std::tie (from, to) = view.getVisibleRange();
    REQUIRE (from == Catch::Approx (0.0));
    REQUIRE (to == Catch::Approx (40.8).margin (0.1)); // 40 s Projekt + 2 % Luft

    REQUIRE_FALSE (view.zoomToSelection()); // nichts gewählt
    view.selectClip (late, {});
    REQUIRE (view.zoomToSelection());
    std::tie (from, to) = view.getVisibleRange();
    REQUIRE (from == Catch::Approx (29.5).margin (0.05));
    REQUIRE (to == Catch::Approx (40.5).margin (0.05));

    // Übersichtsleiste ein: der Ausschnitt bleibt derselbe (die Spurbreite ändert sich nicht).
    view.setOverviewShown (true);
    REQUIRE (view.isOverviewShown());
    REQUIRE (view.getVisibleRange().first == Catch::Approx (from));

    // Eigene Presets: speichern, gleicher Name ersetzt, löschen räumt auf.
    for (const auto& preset : env.timelineService.getZoomPresets())
        REQUIRE (env.timelineService.removeZoomPreset (preset.name).has_value());

    REQUIRE (env.timelineService.saveZoomPreset ({ "Strophe", 20.0 }).has_value());
    REQUIRE (env.timelineService.saveZoomPreset ({ "Detail", 2.0 }).has_value());
    REQUIRE (env.timelineService.saveZoomPreset ({ "Strophe", 24.0 }).has_value());
    REQUIRE_FALSE (env.timelineService.saveZoomPreset ({ "  ", 5.0 }).has_value());

    const auto presets = env.timelineService.getZoomPresets();
    REQUIRE (presets.size() == 2);
    REQUIRE (presets[0].name == "Detail"); // von nah nach weit
    REQUIRE (presets[1].visibleSeconds == Catch::Approx (24.0));

    REQUIRE (env.timelineService.removeZoomPreset ("Detail").has_value());
    REQUIRE (env.timelineService.removeZoomPreset ("Strophe").has_value());
    REQUIRE_FALSE (env.timelineService.removeZoomPreset ("Strophe").has_value());
    REQUIRE (env.timelineService.getZoomPresets().empty());
}

TEST_CASE ("A8b: Befehlstabelle ist eindeutig und hat Cubase-Tasten", "[ui][a8][shortcuts]")
{
    std::set<juce::String> keys;
    std::set<juce::String> usedKeys;

    for (const auto& info : fk::ui::allCommands())
    {
        REQUIRE (keys.insert (info.key).second); // stabile Namen eindeutig

        for (const auto* description : info.defaultKeys)
        {
            const auto key = juce::KeyPress::createFromDescription (description);
            REQUIRE (key.isValid());
            REQUIRE (usedKeys.insert (key.getTextDescription().toLowerCase()).second); // keine Taste doppelt
        }
    }

    REQUIRE (fk::ui::describeKey (juce::KeyPress::createFromDescription ("ctrl + Z")) == "Ctrl+Z");
    REQUIRE (fk::ui::describeKey (juce::KeyPress::createFromDescription ("numpad 1")) == "Num 1");
    REQUIRE (fk::ui::describeKey (juce::KeyPress::createFromDescription ("spacebar")) == "Space");
    REQUIRE (fk::ui::describeKey (juce::KeyPress::createFromDescription ("shift + F")) == "Shift+F");
    REQUIRE (fk::ui::describeKey (juce::KeyPress::createFromDescription ("F5")) == "F5");
}

TEST_CASE ("A8b: Tastenbelegung findet Befehle, speichert Änderungen und löst Konflikte", "[ui][a8][shortcuts]")
{
    TestEnvironment env;
    fk::services::ShortcutService store { env.engineContext };
    store.clearAll();
    fk::ui::KeyBindings bindings { store };
    using fk::ui::Command;

    REQUIRE (bindings.find (juce::KeyPress (juce::KeyPress::spaceKey)) == Command::play);
    REQUIRE (bindings.find (juce::KeyPress (juce::KeyPress::numberPad1)) == Command::jumpLeftLocator);
    REQUIRE (bindings.find (juce::KeyPress (juce::KeyPress::numberPad1, juce::ModifierKeys::ctrlModifier, 0)) ==
             Command::setLeftLocator);
    REQUIRE (bindings.find (juce::KeyPress ('1')) == Command::toolSelect);
    REQUIRE (bindings.find (juce::KeyPress ('h')) == Command::zoomIn);
    REQUIRE (bindings.find (juce::KeyPress ('S', juce::ModifierKeys::altModifier, 0)) == Command::zoomSelection);
    REQUIRE (bindings.find (juce::KeyPress (juce::KeyPress::F5Key)) == Command::keyEditor);
    // "*" auf deutscher Tastatur: Shift+Plus, das Zeichen zählt.
    REQUIRE (bindings.find (juce::KeyPress ('+', juce::ModifierKeys::shiftModifier, '*')) == Command::record);
    REQUIRE_FALSE (bindings.find (juce::KeyPress ('q')).has_value());
    REQUIRE (bindings.menuText ("Undo", Command::undo) == juce::String ("Undo\tCtrl+Z"));

    // Strg+D für Vergrößern: Duplizieren verliert die Taste, das wird gemeldet.
    int changes = 0;
    bindings.onChanged = [&changes] { ++changes; };
    const auto robbed = bindings.setKeys (Command::zoomIn, { juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0) });
    REQUIRE (robbed == std::vector<Command> { Command::duplicate });
    REQUIRE (changes == 1);
    REQUIRE (bindings.find (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)) == Command::zoomIn);
    REQUIRE (bindings.keysFor (Command::duplicate).empty());
    REQUIRE_FALSE (bindings.isDefault (Command::zoomIn));

    // Gespeichert: eine neue Belegung aus demselben Speicher sieht dasselbe.
    {
        fk::ui::KeyBindings reloaded { store };
        REQUIRE (reloaded.find (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)) == Command::zoomIn);
        REQUIRE (reloaded.keysFor (Command::duplicate).empty());
        REQUIRE_FALSE (reloaded.find (juce::KeyPress ('h')).has_value());
    }

    bindings.resetToDefault (Command::duplicate);
    REQUIRE (bindings.isDefault (Command::duplicate));
    bindings.resetAll();
    REQUIRE (bindings.find (juce::KeyPress ('h')) == Command::zoomIn);
    REQUIRE (bindings.find (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)) == Command::duplicate);
    REQUIRE (store.getOverrides().empty());
}

TEST_CASE ("A8b: Editor Tastaturbefehle nimmt Tasten auf und sucht", "[ui][a8][shortcuts]")
{
    TestEnvironment env;
    fk::services::ShortcutService store { env.engineContext };
    store.clearAll();
    fk::ui::KeyBindings bindings { store };
    fk::ui::ShortcutEditor editor { bindings };
    using fk::ui::Command;

    const auto all = editor.getNumRows();
    REQUIRE (all == static_cast<int> (fk::ui::allCommands().size()));

    editor.setSearchText ("Zoom");
    REQUIRE (editor.getNumRows() >= 4);
    REQUIRE (editor.getNumRows() < all);
    editor.setSearchText ("Ctrl+D"); // auch nach Tasten suchbar
    REQUIRE (editor.getNumRows() == 1);
    editor.setSearchText ("");

    editor.selectCommand (Command::glue);
    editor.startCapture();
    REQUIRE (editor.isCapturing());
    REQUIRE (editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE_FALSE (editor.isCapturing());
    REQUIRE (bindings.keysFor (Command::glue).size() == 1);

    // Strg+Z gehört Rückgängig: wechselt zu Kleben, der Hinweis nennt den alten Besitzer.
    editor.startCapture();
    REQUIRE (editor.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (bindings.keysFor (Command::glue).size() == 2);
    REQUIRE (bindings.keysFor (Command::undo).empty());
    REQUIRE (editor.getMessage().contains (fk::ui::commandName (Command::undo)));

    editor.removeSelectedKey();
    REQUIRE (bindings.keysFor (Command::glue).size() == 1);

    bindings.resetAll();
    REQUIRE (store.getOverrides().empty());
}

namespace
{
juce::String effectId (const fk::services::PluginService& plugins, const juce::String& name)
{
    for (const auto& plugin : plugins.getAllPlugins())
        if (plugin.name == name)
            return plugin.id;

    return {};
}

/// Plugin-Einstellungen der Tests zurücksetzen (Testordner, nicht die echten Einstellungen).
void resetPluginPrefs (fk::services::PluginService& plugins)
{
    for (const auto& plugin : plugins.getAllPlugins())
    {
        (void) plugins.setFavourite (plugin.id, false);
        (void) plugins.setHidden (plugin.id, false);
        (void) plugins.setGroup (plugin.id, {});
    }
}
} // namespace

TEST_CASE ("A8c: Favoriten, Ausblenden und Kategorien der Plugins", "[services][ui][a8][plugins]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    resetPluginPrefs (plugins);

    const auto groupOf = [&plugins] (const juce::String& name)
    {
        for (const auto& plugin : plugins.getAllPlugins())
            if (plugin.name == name)
                return plugin.group;

        return juce::String();
    };

    REQUIRE (groupOf ("FK Compressor") == "Dynamik");
    REQUIRE (groupOf ("FK EQ") == "EQ");
    REQUIRE (groupOf ("FK Reverb") == "Hall und Delay");
    REQUIRE (groupOf ("FK Delay") == "Hall und Delay");
    REQUIRE (groupOf ("FK Synth") == "Instrument");

    const auto compressor = effectId (plugins, "FK Compressor");
    const auto reverb = effectId (plugins, "FK Reverb");
    REQUIRE (plugins.setFavourite (compressor, true).has_value());
    REQUIRE (plugins.setHidden (reverb, true).has_value());
    REQUIRE (plugins.setGroup (reverb, "Modulation").has_value());
    REQUIRE_FALSE (plugins.setGroup (reverb, "Gibt es nicht").has_value());
    REQUIRE_FALSE (plugins.setFavourite ("unbekannt", true).has_value());

    const auto effects = plugins.getAvailableEffects();
    REQUIRE (std::ranges::none_of (effects, [&] (const auto& p) { return p.id == reverb; })); // ausgeblendet
    REQUIRE (std::ranges::any_of (effects, [&] (const auto& p) { return p.id == compressor && p.favourite; }));
    REQUIRE (groupOf ("FK Reverb") == "Modulation"); // in der Verwaltung weiter sichtbar

    // Menü: Favoriten oben, mit dem Compressor als erstem Eintrag.
    const auto menu = fk::ui::buildPluginMenu (effects);
    juce::PopupMenu::MenuItemIterator items (menu);
    REQUIRE (items.next());
    REQUIRE (items.getItem().isSectionHeader);
    REQUIRE (items.getItem().text == "Favorites");
    REQUIRE (items.next());
    REQUIRE (effects[static_cast<std::size_t> (items.getItem().itemID - 1)].id == compressor);

    // Plugin-Manager-Tabelle: Klick auf den Stern nimmt den Favoriten wieder weg.
    fk::ui::PluginLibraryPanel panel { plugins };
    panel.setSearchText ("FK Compressor");
    REQUIRE (panel.getNumRows() == 1);
    REQUIRE (panel.getRow (0).favourite);
    panel.clickCell (0, fk::ui::PluginLibraryPanel::favouriteColumn);
    REQUIRE_FALSE (panel.getRow (0).favourite);
    panel.clickCell (0, fk::ui::PluginLibraryPanel::hiddenColumn);
    REQUIRE (panel.getRow (0).hidden);

    resetPluginPrefs (plugins);
    REQUIRE (groupOf ("FK Reverb") == "Hall und Delay");
}

TEST_CASE ("A8c: Presets speichern, laden und löschen", "[services][a8][plugins]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Presets").has_value());
    auto* track = *env.trackService.createAudioTrack ("Bass", 1);
    const auto insertId = *plugins.insertPlugin (*track, effectId (plugins, "FK Compressor"));

    // Ein Parameter des Plugins direkt, wie ihn der Editor setzen würde.
    juce::AudioProcessorParameter* parameter = nullptr;

    for (auto* plugin : track->pluginList.getPlugins())
        if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin))
            if (external->itemID.toString() == insertId)
                parameter = external->getAudioPluginInstance()->getParameters()[0];

    REQUIRE (parameter != nullptr);

    for (const auto& name : plugins.getUserPresets (insertId))
        REQUIRE (plugins.deleteUserPreset (insertId, name).has_value());

    parameter->setValueNotifyingHost (0.2f);
    REQUIRE (plugins.saveUserPreset (insertId, "Hart").has_value());
    REQUIRE_FALSE (plugins.saveUserPreset (insertId, "   ").has_value());
    REQUIRE (plugins.getUserPresets (insertId) == std::vector<juce::String> { "Hart" });

    parameter->setValueNotifyingHost (0.8f);
    REQUIRE (plugins.loadUserPreset (insertId, "Hart").has_value());
    REQUIRE (parameter->getValue() == Catch::Approx (0.2f).margin (0.01));
    REQUIRE_FALSE (plugins.loadUserPreset (insertId, "Gibt es nicht").has_value());

    // Ein Preset gehört zu seinem Plugin: Ein zweiter Compressor sieht es auch.
    const auto second = *plugins.insertPlugin (*track, effectId (plugins, "FK Compressor"));
    REQUIRE (plugins.getUserPresets (second).size() == 1);

    REQUIRE (plugins.deleteUserPreset (insertId, "Hart").has_value());
    REQUIRE (plugins.getUserPresets (insertId).empty());

    // Unsere Plugins haben keine Werks-Programme; ein ungültiges Programm wird abgelehnt.
    REQUIRE (plugins.getFactoryPrograms (insertId).empty());
    REQUIRE_FALSE (plugins.loadFactoryProgram (insertId, 5).has_value());
}

TEST_CASE ("B1: Inspector zeigt Rack-Kanal und Ausgänge", "[ui][b1][rack]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::SendService sends { env.projectService, env.trackService };
    fk::services::RackService racks { env.projectService, env.trackService };
    fk::services::TransportService transport { env.projectService };
    fk::services::ExternalMidiService externalMidi { env.projectService, env.trackService, transport };
    REQUIRE (env.projectService.createProject (env.tempFolder, "RackUi").has_value());

    auto* midi = *racks.createRackInstrument (TestEnvironment::synthId (plugins), "Keys");
    auto* output = env.trackService.getAudioTracks().back();

    fk::ui::Inspector inspector { env.trackService, mixer, plugins, sends, racks, externalMidi };
    inspector.setSize (260, 900);
    inspector.setTrack (midi);

    const auto findChild = [&inspector] (const juce::String& tooltipStart) -> juce::Component*
    {
        for (auto* child : inspector.getChildren())
            if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (child))
                if (client->getTooltip().startsWith (tooltipStart))
                    return child;

        return nullptr;
    };

    auto* channelBox = dynamic_cast<juce::ComboBox*> (findChild ("MIDI channel of this track"));
    REQUIRE (channelBox != nullptr);
    REQUIRE (channelBox->isVisible());
    REQUIRE (channelBox->getSelectedId() == 1);

    channelBox->setSelectedId (5, juce::sendNotificationSync);
    REQUIRE (racks.getRole (*midi)->midiChannel == 5);

    auto* addMidi = dynamic_cast<juce::Button*> (findChild ("Another MIDI track"));
    REQUIRE (addMidi != nullptr);
    addMidi->triggerClick();
    TestEnvironment::pumpMessages (50);
    REQUIRE (racks.getRacks().front().numMidiTracks == 2);

    // Ausgangskanal: kein Kanal-Feld, aber der Ausgänge-Button.
    inspector.setTrack (output);
    REQUIRE_FALSE (channelBox->isVisible());
    REQUIRE (findChild ("Enable outputs")->isVisible());
}
