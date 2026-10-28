#include "fk/ui/LowerZone.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/TransportBar.h"
#include "fk/ui/ZoneSplitter.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <array>

// Bilder für docs/manual/project-window.md. Ändert sich das Projektfenster (Leisten, Zonen, Reiter),
// hier die Szene anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findChild;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;
using fk::tests::manual::stripCallout;

TEST_CASE ("Handbuch: Projektfenster Überblick mit nummerierten Zonen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    // Hoch genug, dass unter Lineal, Signatur- und Markerspur alle Spuren sichtbar sind.
    auto& window = scene.open (1280, 1060);

    // Übersichtsleiste einschalten, damit sie im Bild eine Nummer bekommt (Standard: aus).
    dynamic_cast<juce::Button&> (findByTooltip (window, "Overview bar on/off")).triggerClick();
    TestEnvironment::pumpMessages (100);
    findDirectChild<fk::ui::LowerZone> (window).showMixer();
    scene.zoomToProject();
    TestEnvironment::pumpMessages (200);

    auto& menuBar = findDirectChild<juce::MenuBarComponent> (window);
    auto& inspector = findDirectChild<juce::Viewport> (window); // einziger Viewport direkt im Fenster
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& tracksViewport = findDirectChild<juce::Viewport> (arrange);
    auto& splitter = findDirectChild<fk::ui::ZoneSplitter> (window);
    auto& lowerZone = findDirectChild<fk::ui::LowerZone> (window);
    auto& transportBar = findDirectChild<fk::ui::TransportBar> (window);

    const auto menuArea = boundsIn (window, menuBar);
    const auto toolbarArea = juce::Rectangle<int>::leftTopRightBottom (
        0, menuArea.getBottom(), window.getWidth(), boundsIn (window, inspector).getY());
    const auto tracksArea = boundsIn (window, tracksViewport);

    // Nummern wie in der Tabelle in project-window.md, Abschnitt "The window at a glance".
    saveShot (window,
              "project-window/overview",
              { calloutFor (1, window, menuBar, Callout::Placement::above),
                Callout { 2, toolbarArea, Callout::Placement::inside },
                calloutFor (3, window, inspector, Callout::Placement::inside),
                stripCallout (4, boundsIn (window, findChild<fk::ui::OverviewBar> (arrange))),
                stripCallout (5, boundsIn (window, findChild<fk::ui::TimeRuler> (arrange))),
                calloutFor (6, window, findChild<fk::ui::SignatureTrack> (arrange), Callout::Placement::inside),
                stripCallout (7, boundsIn (window, findChild<fk::ui::MarkerTrack> (arrange))),
                Callout { 8, tracksArea.withWidth (fk::ui::theme::trackHeaderWidth), Callout::Placement::inside },
                Callout { 9, tracksArea.withTrimmedLeft (fk::ui::theme::trackHeaderWidth), Callout::Placement::inside },
                stripCallout (10, boundsIn (window, splitter)),
                calloutFor (11, window, lowerZone, Callout::Placement::inside),
                calloutFor (12, window, transportBar, Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Projektfenster Werkzeugleiste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    // So breit, dass alle Buttons ihre Beschriftung zeigen und die Leiste in eine Reihe passt.
    auto& window = scene.open (2400, 800);

    // Reihenfolge wie in der Leiste = Nummern der Tabelle in project-window.md, Abschnitt "Toolbar".
    // FIX F-204 (Reihenfolge der Werkzeuge)
    const std::array<const char*, 43> tooltips { "New project:",
                                                 "Open project",
                                                 "Recent projects",
                                                 "Save project",
                                                 "Save as:",
                                                 "Project settings:",
                                                 "Export:",
                                                 "Add audio track",
                                                 "Add instrument track",
                                                 "Add group track",
                                                 "Add FX track",
                                                 "Add folder track",
                                                 "Undo",
                                                 "Redo",
                                                 "History:",
                                                 "Cut selected clips",
                                                 "Copy selected clips",
                                                 "Paste clips",
                                                 "Duplicate selected clips",
                                                 "Glue selected clips",
                                                 "Delete selected clips",
                                                 "Select tool:",
                                                 "Range tool:",
                                                 "Pencil:",
                                                 "Split tool:",
                                                 "Eraser:",
                                                 "Glue: join",
                                                 "Zoom: click",
                                                 "Mute tool:",
                                                 "Move selected clips one grid step left",
                                                 "Move selected clips one grid step right",
                                                 "Grid for snap",
                                                 "Snap:",
                                                 "Zoom: whole project",
                                                 "Overview bar on/off",
                                                 "Tracks smaller",
                                                 "Tracks larger",
                                                 "Show and hide tracks",
                                                 "Inspector on/off",
                                                 "Lower zone on/off",
                                                 "Audio settings:",
                                                 "Plugin manager:",
                                                 "MIDI Learn:" };

    // Drei Ausschnitte: Projekt und Spuren (1-12), Bearbeiten und Werkzeuge (13-29), Ansicht und Studio (30-44).
    const auto top = boundsIn (window, findDirectChild<juce::MenuBarComponent> (window)).getBottom();
    const auto bottom = boundsIn (window, findDirectChild<juce::Viewport> (window)).getY();

    // Markierungen für die Buttons first..last (Index in tooltips) und der Ausschnitt um sie herum.
    const auto part = [&] (std::size_t first, std::size_t last, std::vector<Callout>& callouts)
    {
        for (auto i = first; i <= last; ++i)
            callouts.push_back (calloutFor (static_cast<int> (i) + 1, window, findByTooltip (window, tooltips[i]), Callout::Placement::above));

        const auto left = boundsIn (window, findByTooltip (window, tooltips[first])).getX() - 8;
        const auto right = boundsIn (window, findByTooltip (window, tooltips[last])).getRight() + 8;
        return juce::Rectangle<int>::leftTopRightBottom (juce::jmax (0, left), top, right, bottom);
    };

    std::vector<Callout> projectCallouts;
    const auto projectArea = part (0, 11, projectCallouts);
    saveShot (window, "project-window/toolbar-project", projectCallouts, projectArea);

    std::vector<Callout> editCallouts;
    const auto editArea = part (12, 28, editCallouts);
    saveShot (window, "project-window/toolbar-edit", editCallouts, editArea);

    // Letzter Ausschnitt bis zum rechten Rand: dort steht der Projektname (44).
    std::vector<Callout> viewCallouts;
    const auto viewArea = part (29, tooltips.size() - 1, viewCallouts).withRight (window.getWidth());

    for (auto* child : window.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
            if (label->isVisible() && label->getText() == "Demo Song")
            {
                const auto nameArea = boundsIn (window, *label);
                viewCallouts.push_back ({ 44, nameArea.withTrimmedLeft (nameArea.getWidth() - 120), Callout::Placement::above });
            }

    REQUIRE (viewCallouts.size() == 15);
    saveShot (window, "project-window/toolbar-view", viewCallouts, viewArea);
}

TEST_CASE ("Handbuch: Projektfenster Transportleiste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    // Breit genug für eine Reihe (ab etwa 1620 px); Position in Takt 3, Locatoren Takt 1 bis 5.
    auto& window = scene.open (1700, 900);
    scene.services.transport.setPositionSeconds (2.0 * scene.barSeconds);
    TestEnvironment::pumpMessages (200);

    auto& bar = findDirectChild<fk::ui::TransportBar> (window);
    const auto barArea = boundsIn (window, bar);

    // Nummern wie in der Tabelle in project-window.md, Abschnitt "Transport bar". Suche nur in der
    // Leiste: Die Markerspur hat einen Button mit demselben Tooltip wie "Add marker".
    const std::array<const char*, 25> tooltips { "Go to start",
                                                 "Rewind:",
                                                 "Forward:",
                                                 "Go to previous marker",
                                                 "Go to next marker",
                                                 "Add marker at the cursor",
                                                 "Play (",
                                                 "Stop (",
                                                 "Record on armed tracks",
                                                 "Retrospective record",
                                                 "Loop on/off",
                                                 "Punch in/out",
                                                 "Pre-roll:",
                                                 "Post-roll:",
                                                 "Metronome on/off",
                                                 "Count-in before recording",
                                                 "Automation read",
                                                 "Automation write",
                                                 "Position: bar",
                                                 "Position in time",
                                                 "Left locator",
                                                 "Right locator",
                                                 "Tempo in BPM",
                                                 "Time signature, e.g.",
                                                 "Audio load:" };
    constexpr std::size_t firstDisplay = 18; // ab "Position: bar" zweites Bild

    std::vector<Callout> controls;
    std::vector<Callout> displays;

    for (std::size_t i = 0; i < tooltips.size(); ++i)
        (i < firstDisplay ? controls : displays)
            .push_back (calloutFor (static_cast<int> (i) + 1, window, findByTooltip (bar, tooltips[i]), Callout::Placement::above));

    const auto split = boundsIn (window, findByTooltip (bar, tooltips[firstDisplay])).getX() - 6;
    saveShot (window, "project-window/transport-controls", controls, barArea.withRight (split));
    saveShot (window, "project-window/transport-displays", displays, barArea.withLeft (split));
}

TEST_CASE ("Handbuch: Projektfenster untere Zone mit Reitern", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1000, 900);
    auto& lowerZone = findDirectChild<fk::ui::LowerZone> (window);
    lowerZone.showMixer();
    TestEnvironment::pumpMessages (200);

    auto& splitter = findDirectChild<fk::ui::ZoneSplitter> (window);
    auto& tabBar = findChild<juce::TabbedComponent> (lowerZone).getTabbedButtonBar();
    REQUIRE (tabBar.getNumTabs() == 8);

    // FIX F-202 (Reiternamen)
    // Nummern 1-8 = Reiter, 9 = Trenner; wie die Tabelle in project-window.md, Abschnitt "Lower zone".
    std::vector<Callout> callouts;

    for (int i = 0; i < tabBar.getNumTabs(); ++i)
    {
        auto* tab = tabBar.getTabButton (i);
        REQUIRE (tab != nullptr);
        callouts.push_back (calloutFor (i + 1, window, *tab, Callout::Placement::above));
    }

    const auto splitterArea = boundsIn (window, splitter);
    callouts.push_back ({ 9, splitterArea.withTrimmedLeft (splitterArea.getWidth() - 60), Callout::Placement::above });

    saveShot (window,
              "project-window/lower-zone",
              callouts,
              splitterArea.getUnion (boundsIn (window, lowerZone)));
}
