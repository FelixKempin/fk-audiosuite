#include "fk/services/TimeFormat.h"
#include "fk/services/TransportService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/MarkerTrack.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/TimeRuler.h"
#include "fk/ui/TransportBar.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <array>
#include <cstddef>
#include <vector>

// Bilder für docs/manual/transport.md. Ändert sich die Transportleiste, das Lineal oder die Markerspur,
// hier die Szenen anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findChild;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;

namespace
{
/// Tooltip-Anfänge der Transportleiste in Leistenreihenfolge. Index + 1 = Nummer in transport.md §2
/// (dieselben Nummern wie project-window.md §5).
// FIX F-201 (fest eingetragene Tasten in den Tooltips; die Anfänge hier bleiben gültig)
constexpr std::array<const char*, 25> transportTooltips { "Go to start",
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

/// Markierungen für die Elemente first..last der Leiste (Indizes in transportTooltips). Gesucht wird
/// nur in der Leiste: Die Markerspur hat einen Button mit demselben Tooltip wie "Add marker".
std::vector<Callout> transportCallouts (juce::Component& root, juce::Component& transportBar, std::size_t first, std::size_t last)
{
    std::vector<Callout> callouts;

    for (auto i = first; i <= last; ++i)
        callouts.push_back (calloutFor (static_cast<int> (i) + 1,
                                        root,
                                        findByTooltip (transportBar, transportTooltips[i]),
                                        Callout::Placement::above));

    return callouts;
}

/// Der Vorzähler steht in den Engine-Einstellungen, nicht im Projekt: nach der Szene zurück auf
/// "No count-in", damit andere Szenen (Projektfenster) nicht "1 bar" zeigen.
struct CountInReset
{
    explicit CountInReset (fk::services::TransportService& service)
        : transport (service)
    {
    }

    ~CountInReset() { transport.setCountIn (fk::services::TransportService::CountIn::none); }

    CountInReset (const CountInReset&) = delete;
    CountInReset& operator= (const CountInReset&) = delete;

    fk::services::TransportService& transport;
};
} // namespace

TEST_CASE ("Handbuch: Transport Leiste mit Punch, Pre-/Post-Roll, Metronom und Vorzähler", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Zustand wie in transport.md §2 beschrieben: Punch an (Loop damit aus), Pre-Roll 1 s,
    // Post-Roll 2 s, Metronom an, Vorzähler 1 Takt; Locatoren Takt 1 bis 5 aus dem Demo-Projekt.
    // Alles vor dem Fenster einstellen (spätere Projektänderungen können Rückfragen auslösen).
    ProjectWindowScene scene;
    CountInReset resetCountIn { scene.services.transport };
    scene.services.transport.setPunchEnabled (true);
    scene.services.transport.setPreRollSeconds (1.0);
    scene.services.transport.setPostRollSeconds (2.0);
    scene.services.transport.setCountIn (fk::services::TransportService::CountIn::oneBar);
    scene.env.timelineService.setMetronomeEnabled (true);

    // Breit genug für eine Reihe (ab etwa 1620 px); Position Takt 3, Schlag 2.
    auto& window = scene.open (1700, 900);
    scene.services.transport.setPositionSeconds (2.0 * scene.barSeconds + scene.barSeconds / 4.0);
    TestEnvironment::pumpMessages (200);

    auto& transportBar = findDirectChild<fk::ui::TransportBar> (window);
    const auto barArea = boundsIn (window, transportBar);
    const auto split = boundsIn (window, findByTooltip (transportBar, transportTooltips[firstDisplay])).getX() - 6;

    saveShot (window,
              "transport/controls",
              transportCallouts (window, transportBar, 0, firstDisplay - 1),
              barArea.withRight (split));
    saveShot (window,
              "transport/displays",
              transportCallouts (window, transportBar, firstDisplay, transportTooltips.size() - 1),
              barArea.withLeft (split));
}

TEST_CASE ("Handbuch: Transport Zeitanzeige als Timecode", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Zweite Positionsanzeige auf Timecode (25 fps, Standard), vor dem Fenster eingestellt.
    // Samples zeigt die Szene nicht: Ohne Audiogerät rechnet die Anzeige mit Rate 1.
    // FIX F-407 (Samples ohne Audiogerät)
    ProjectWindowScene scene;
    auto settings = scene.services.projectSettings.getSettings();
    settings.timeDisplay = fk::services::TimeDisplay::timecode;
    REQUIRE (scene.services.projectSettings.setSettings (settings).has_value());

    auto& window = scene.open (1700, 900);
    scene.services.transport.setPositionSeconds (2.0 * scene.barSeconds + scene.barSeconds / 4.0);
    TestEnvironment::pumpMessages (200);

    auto& transportBar = findDirectChild<fk::ui::TransportBar> (window);
    const auto barArea = boundsIn (window, transportBar);
    const auto barsArea = boundsIn (window, findByTooltip (transportBar, "Position: bar"));
    const auto timeArea = boundsIn (window, findByTooltip (transportBar, "Position in time"));

    // Nummern 19 und 20 wie in der Tabelle in transport.md §2.
    saveShot (window,
              "transport/time-timecode",
              { calloutFor (19, window, findByTooltip (transportBar, "Position: bar"), Callout::Placement::above),
                calloutFor (20, window, findByTooltip (transportBar, "Position in time"), Callout::Placement::above) },
              juce::Rectangle<int>::leftTopRightBottom (
                  barsArea.getX() - 8, barArea.getY(), timeArea.getRight() + 8, barArea.getBottom()));
}

TEST_CASE ("Handbuch: Transport Lineal mit Loop-Bereich, Positionszeiger und Markern", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Locatoren Takt 2 bis 4, Loop an, Positionszeiger auf Takt 3; Marker "Intro" (Takt 1) und
    // "Verse" (Takt 5) aus dem Demo-Projekt.
    ProjectWindowScene scene;
    const double bar = scene.barSeconds;
    scene.env.timelineService.setLoopRange ({ 1.0 * bar, 3.0 * bar });
    scene.env.timelineService.setLooping (true);

    auto& window = scene.open (1000, 760);
    scene.zoomToProject();
    scene.services.transport.setPositionSeconds (2.0 * bar);
    TestEnvironment::pumpMessages (200);

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& ruler = findChild<fk::ui::TimeRuler> (arrange);
    auto& markerTrack = findChild<fk::ui::MarkerTrack> (arrange);
    const auto rulerArea = boundsIn (window, ruler);
    const auto markerArea = boundsIn (window, markerTrack);
    const auto arrangeArea = boundsIn (window, arrange);
    const auto mapping = arrange.getTimeMapping();
    const auto xAt = [&mapping, &rulerArea] (double seconds) { return rulerArea.getX() + mapping.secondsToX (seconds); };

    // Lineal, Signatur- und Markerspur und die ersten Spuren darunter (für den Positionszeiger).
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (arrangeArea.getX(),
                                                                    rulerArea.getY(),
                                                                    arrangeArea.getRight(),
                                                                    juce::jmin (arrangeArea.getBottom(), markerArea.getBottom() + 160));
    const int bandHeight = rulerArea.getHeight() / 3; // Loop-Band im oberen Drittel des Lineals

    // Nummern wie in transport.md §6: 1 L, 2 R, 3 Positionszeiger, 4 Marker, 5 Marker-Plus.
    saveShot (window,
              "transport/ruler-loop",
              { Callout { 1, { xAt (1.0 * bar) - 1, rulerArea.getY(), 3, bandHeight }, Callout::Placement::above },
                Callout { 2, { xAt (3.0 * bar) - 1, rulerArea.getY(), 3, bandHeight }, Callout::Placement::above },
                Callout { 3,
                          { xAt (2.0 * bar) - 2, rulerArea.getY(), 5, shotArea.getBottom() - rulerArea.getY() },
                          Callout::Placement::above },
                Callout { 4, { xAt (4.0 * bar), markerArea.getY(), 60, markerArea.getHeight() }, Callout::Placement::inside },
                calloutFor (5, window, findByTooltip (markerTrack, "Add marker at the cursor"), Callout::Placement::inside) },
              shotArea);
}
