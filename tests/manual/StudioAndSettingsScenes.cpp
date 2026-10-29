#include "fk/services/TrackService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/StudioConnectionsDialog.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

// Bilder für docs/manual/studio-and-settings.md. Ändern sich Studio-Verbindungen, die Studio-Buttons der
// Werkzeugleiste oder der MIDI-Learn-Hinweis, hier die Szenen anpassen und tools/manual.py shots laufen
// lassen (docs/manual/MAINTAINING.md). Die Audio-Einstellungen fotografiert GettingStartedScenes.cpp.

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;

namespace
{

/// Rand rechts neben dem Dialog: Nummern kleiner Ziele am rechten Rand stehen rechts daneben.
constexpr int studioCalloutMargin = 30;

/// Nur Engine und Look-and-Feel, ohne Demo-Projekt (die Studio-Verbindungen brauchen keine Spuren).
struct StudioScene
{
    StudioScene() { juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel); }
    ~StudioScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
};

/// Erstes sichtbares Label (beliebig tief), dessen Text mit textStart beginnt, oder nullptr.
/// Labels haben keinen Tooltip, findByTooltip findet sie nicht.
juce::Label* searchLabel (juce::Component& root, const juce::String& textStart)
{
    for (auto* child : root.getChildren())
    {
        if (auto* label = dynamic_cast<juce::Label*> (child))
            if (label->isVisible() && label->getText().startsWith (textStart))
                return label;

        if (auto* found = searchLabel (*child, textStart))
            return found;
    }

    return nullptr;
}

juce::Label& findLabel (juce::Component& root, const juce::String& textStart)
{
    auto* found = searchLabel (root, textStart);
    INFO ("Kein sichtbares Label mit Text \"" << textStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Alle sichtbaren Labels mit genau diesem Text (Treibernamen der Anschlüsse).
std::optional<juce::Rectangle<int>> unionOfLabels (juce::Component& shotRoot,
                                                   juce::Component& searchRoot,
                                                   const std::vector<juce::String>& texts)
{
    std::optional<juce::Rectangle<int>> area;

    std::function<void (juce::Component&)> visit = [&] (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->isVisible() && std::find (texts.begin(), texts.end(), label->getText()) != texts.end())
                {
                    const auto bounds = calloutFor (0, shotRoot, *label, Callout::Placement::inside).target;
                    area = area.has_value() ? area->getUnion (bounds) : bounds;
                }

            visit (*child);
        }
    };

    visit (searchRoot);
    return area;
}

} // namespace

TEST_CASE ("Handbuch: Studio-Verbindungen mit eigenem Namen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Die Anschlüsse hängen vom Audiogerät des Rechners ab (z. B. Input 1-4, Input 1+2, Output 1+2).
    // Der Dialog liest sie nur mit offenem Projekt. FIX F-107
    StudioScene scene;
    REQUIRE (scene.env.projectService.createProject (scene.env.tempFolder, "Demo Song").has_value());

    const auto ports = scene.env.trackService.getStudioPorts();
    std::optional<fk::services::StudioPort> firstInput;
    std::vector<juce::String> inputNames;

    for (const auto& port : ports)
        if (port.isInput)
        {
            if (! firstInput.has_value())
                firstInput = port;

            inputNames.push_back (port.name);
        }

    // Beispielname wie im Text ("Guitar DI"); die Tests haben eigene Engine-Einstellungen, trotzdem
    // am Ende den vorigen Namen wiederherstellen.
    if (firstInput.has_value())
        REQUIRE (scene.env.trackService.setPortAlias (firstInput->id, "Guitar DI").has_value());

    {
        fk::ui::StudioConnectionsDialog dialog (scene.env.trackService);

        juce::Component holder;
        holder.setSize (dialog.getWidth() + studioCalloutMargin, dialog.getHeight());
        holder.addAndMakeVisible (dialog);
        dialog.setTopLeftPosition (0, 0);
        TestEnvironment::pumpMessages (200);

        std::vector<Callout> callouts;

        if (firstInput.has_value())
        {
            // Nummern wie die Tabelle in studio-and-settings.md, Abschnitt "Studio connections at a glance".
            // FIX F-1902 (MIDI-Anschlüsse fehlen in der Liste)
            const auto nameColumn = unionOfLabels (holder, dialog, inputNames);
            REQUIRE (nameColumn.has_value());

            callouts.push_back (calloutFor (1, holder, findLabel (dialog, "Inputs"), Callout::Placement::inside));
            callouts.push_back ({ 2, *nameColumn, Callout::Placement::inside });
            callouts.push_back (
                calloutFor (3, holder, findByTooltip (dialog, "Own name for this port"), Callout::Placement::inside));

            if (auto* outputs = searchLabel (dialog, "Outputs"))
                callouts.push_back (calloutFor (4, holder, *outputs, Callout::Placement::inside));
            else
                WARN ("Studio-Verbindungen: Überschrift Outputs nicht sichtbar, Nummer 4 fehlt im Bild");
        }
        else
        {
            WARN ("Audiogerät ohne Eingänge: Studio-Verbindungen ohne Nummern im Bild");
        }

        saveShot (holder, "studio-and-settings/studio-connections", callouts);
        holder.removeChildComponent (&dialog);
    }

    if (firstInput.has_value())
        REQUIRE (scene.env.trackService.setPortAlias (firstInput->id, firstInput->alias).has_value());
}

TEST_CASE ("Handbuch: Studio-Verbindungen ohne Audiogerät", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Ohne Projekt liefert der Dialog keine Anschlüsse und zeigt denselben Hinweis wie ohne Audiogerät.
    // FIX F-106 (Hinweistext und Menüpfad im Bild)
    // FIX F-107 (ohne Projekt erschienen nach dem Fix die Anschlüsse; dann ein Gerät ohne Kanäle nachbilden)
    StudioScene scene;
    fk::ui::StudioConnectionsDialog dialog (scene.env.trackService);
    TestEnvironment::pumpMessages (100);

    // Der Hinweis muss sichtbar sein, sonst zeigt das Bild etwas anderes als der Text.
    (void) findLabel (dialog, "No audio device active");

    // Nur der obere Teil: Der Hinweis steht mittig, darunter ist der Dialog leer.
    const auto area = dialog.getLocalBounds().withSizeKeepingCentre (dialog.getWidth(), 160);
    saveShot (dialog, "studio-and-settings/studio-connections-empty", {}, area);
}

TEST_CASE ("Handbuch: Studio-Buttons und MIDI Learn in der Werkzeugleiste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1280, 720);

    // Lernmodus an: Der Button leuchtet, der Hinweis steht vor dem Projektnamen.
    scene.services.midiLearn.setLearnActive (true);
    TestEnvironment::pumpMessages (200);

    auto& audioSettings = findByTooltip (window, "Audio settings:");
    auto& pluginManager = findByTooltip (window, "Plugin manager: scan");
    auto& midiLearn = findByTooltip (window, "MIDI Learn:");
    auto& hint = findLabel (window, "Touch a control in the DAW");

    // Ausschnitt: von den Audio-Einstellungen bis zum rechten Fensterrand, nur die Werkzeugleiste.
    // FIX F-1904 (Button ohne Projekt aktiv; das Bild zeigt ihn mit Projekt und bleibt gleich)
    const auto toolbarArea = boundsIn (window, audioSettings)
                                 .getUnion (boundsIn (window, hint))
                                 .expanded (8, 4)
                                 .withRight (window.getWidth())
                                 .getIntersection (window.getLocalBounds());

    // Nummern wie die Tabelle in studio-and-settings.md, Abschnitt "The Studio buttons in the toolbar".
    saveShot (window,
              "studio-and-settings/toolbar",
              { calloutFor (1, window, audioSettings, Callout::Placement::above),
                calloutFor (2, window, pluginManager, Callout::Placement::above),
                calloutFor (3, window, midiLearn, Callout::Placement::above),
                Callout { 4, boundsIn (window, hint).withWidth (juce::jmin (220, hint.getWidth())), Callout::Placement::above } },
              toolbarArea);

    scene.services.midiLearn.setLearnActive (false);
    TestEnvironment::pumpMessages (100);
}
