#include "fk/services/ClipService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/ClipStretchPanel.h"
#include "fk/ui/ClipView.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/Theme.h"
#include "fk/ui/TrackLane.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// Bilder für docs/manual/arranging.md. Ändern sich Werkzeuge, Clip-Darstellung, Bereichsauswahl, Abwurf-
// Vorschau oder die Sprechblase "Tempo and Pitch", hier die Szenen anpassen und tools/manual.py shots
// laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::mouseEvent;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;

namespace
{
/// Spur des Demo-Projekts über ihren Namen (nullptr, wenn es sie nicht gibt).
fk::services::TrackHandle* trackNamed (ProjectWindowScene& scene, const juce::String& name)
{
    for (auto* track : scene.env.trackService.getAudioTracks())
        if (scene.env.trackService.getTrackInfo (*track).name == name)
            return track;

    return nullptr;
}

/// Kennung des Clips einer Spur, der bei atSeconds beginnt (auf 0,1 s genau), sonst leer.
juce::String clipStartingAt (ProjectWindowScene& scene, fk::services::TrackHandle& track, double atSeconds)
{
    for (const auto& clip : scene.env.trackService.getClips (track))
        if (std::abs (clip.startSeconds - atSeconds) < 0.1)
            return clip.id;

    return {};
}

/// Die Clips, die die Bilder zeigen.
struct DemoClips
{
    juce::String kick;
    juce::String snareMuted;
    juce::String bass;
    juce::String synth;
};

/// Bearbeitet das Demo-Projekt vor dem Öffnen des Fensters (ProjectWindowScene: Änderungen danach
/// lösen die Abtastraten-Rückfrage aus): Snare ab Takt 5 geteilt und stumm, Bass mit Fades, +3 dB
/// Clip-Gain und +2 Halbtönen.
DemoClips prepareClips (ProjectWindowScene& scene)
{
    auto& clips = scene.env.clipService;
    const double bar = scene.barSeconds;
    auto* kick = trackNamed (scene, "Kick");
    auto* snare = trackNamed (scene, "Snare");
    auto* bass = trackNamed (scene, "Bass");
    REQUIRE (kick != nullptr);
    REQUIRE (snare != nullptr);
    REQUIRE (bass != nullptr);

    DemoClips ids;
    ids.kick = clipStartingAt (scene, *kick, 0.0);
    ids.synth = scene.synthClipId;

    const auto snareWhole = clipStartingAt (scene, *snare, 0.0);
    REQUIRE (snareWhole.isNotEmpty());
    REQUIRE (clips.splitClip (snareWhole, 4.0 * bar).has_value());
    ids.snareMuted = clipStartingAt (scene, *snare, 4.0 * bar);
    REQUIRE (ids.snareMuted.isNotEmpty());
    REQUIRE (clips.setClipsMuted ({ ids.snareMuted }, true).has_value());

    ids.bass = clipStartingAt (scene, *bass, 4.0 * bar);
    REQUIRE (ids.bass.isNotEmpty());
    REQUIRE (clips.setFades (ids.bass, 0.5 * bar, 1.5 * bar).has_value());
    REQUIRE (clips.setGainDb (ids.bass, 3.0f).has_value());
    REQUIRE (clips.setPitch ({ ids.bass }, 2.0f).has_value());
    REQUIRE (ids.kick.isNotEmpty());
    return ids;
}

juce::Component* findWithId (juce::Component& parent, const juce::String& id)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getComponentID() == id)
            return child;

        if (auto* found = findWithId (*child, id))
            return found;
    }

    return nullptr;
}

/// Zeitachse der n-ten Spurzeile (ArrangeView vergibt "lane0", "lane1", ...; Ordner zählen nicht).
fk::ui::TrackLane& laneAt (juce::Component& root, int index)
{
    auto* lane = dynamic_cast<fk::ui::TrackLane*> (findWithId (root, "lane" + juce::String (index)));
    INFO ("Keine Spurzeile lane" << index);
    REQUIRE (lane != nullptr);
    return *lane;
}

fk::ui::ClipView& clipViewFor (fk::ui::TrackLane& lane, const juce::String& clipId)
{
    fk::ui::ClipView* found = nullptr;

    for (auto* child : lane.getChildren())
        if (auto* view = dynamic_cast<fk::ui::ClipView*> (child); view != nullptr && view->getClipInfo().id == clipId)
            found = view;

    INFO ("Keine Clip-Ansicht für " << clipId);
    REQUIRE (found != nullptr);
    return *found;
}

/// Inspector und untere Zone ausblenden, damit das Arrangement die ganze Breite und Höhe bekommt.
void showArrangementOnly (juce::Component& window)
{
    dynamic_cast<juce::Button&> (findByTooltip (window, "Inspector on/off")).triggerClick();
    dynamic_cast<juce::Button&> (findByTooltip (window, "Lower zone on/off")).triggerClick();
    TestEnvironment::pumpMessages (200);
}

/// Rechteck in Koordinaten einer Clip-Ansicht, umgerechnet ins Fenster.
juce::Rectangle<int> inWindow (juce::Component& window, juce::Component& view, juce::Rectangle<int> local)
{
    return window.getLocalArea (&view, local);
}
} // namespace

TEST_CASE ("Handbuch: Arrangieren, Bearbeiten- und Werkzeug-Buttons", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    const auto ids = prepareClips (scene);
    // So breit, dass die Werkzeugleiste in eine Reihe passt (wie project-window/toolbar-edit).
    auto& window = scene.open (2400, 800);
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);

    // Ein Clip ausgewählt und kopiert: Dann sind die Clip-Buttons aktiv statt grau.
    arrange.selectClip (ids.kick, juce::ModifierKeys());
    arrange.performClipAction (fk::ui::ClipAction::copy);
    TestEnvironment::pumpMessages (100);

    // Reihenfolge wie in der Leiste = Nummern der Tabelle in arranging.md §2.
    // FIX F-204 (Reihenfolge der Werkzeuge 7-14)
    constexpr std::array<const char*, 22> tooltips { "Cut selected clips",
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
                                                     "Tracks larger" };

    std::vector<Callout> callouts;

    for (std::size_t i = 0; i < tooltips.size(); ++i)
        callouts.push_back (calloutFor (static_cast<int> (i) + 1,
                                        window,
                                        findByTooltip (window, tooltips[i]),
                                        Callout::Placement::above));

    const auto top = boundsIn (window, findDirectChild<juce::MenuBarComponent> (window)).getBottom();
    const auto bottom = boundsIn (window, findDirectChild<juce::Viewport> (window)).getY(); // Inspector
    const auto left = boundsIn (window, findByTooltip (window, tooltips.front())).getX() - 8;
    const auto right = boundsIn (window, findByTooltip (window, tooltips.back())).getRight() + 8;

    saveShot (window,
              "arranging/tools",
              callouts,
              juce::Rectangle<int>::leftTopRightBottom (juce::jmax (0, left), top, right, bottom));
}

TEST_CASE ("Handbuch: Arrangieren, Clips im Arrangement", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    const auto ids = prepareClips (scene);
    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);
    scene.zoomToProject();

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    arrange.selectClip (ids.kick, juce::ModifierKeys());
    TestEnvironment::pumpMessages (200);

    const auto area = boundsIn (window, findDirectChild<juce::Viewport> (arrange));

    // Nummern wie in arranging.md §1.
    saveShot (window,
              "arranging/clips",
              { calloutFor (1, window, clipViewFor (laneAt (arrange, 0), ids.kick), Callout::Placement::inside),
                calloutFor (2, window, clipViewFor (laneAt (arrange, 1), ids.snareMuted), Callout::Placement::inside),
                calloutFor (3, window, clipViewFor (laneAt (arrange, 2), ids.bass), Callout::Placement::inside),
                calloutFor (4, window, clipViewFor (laneAt (arrange, 3), ids.synth), Callout::Placement::inside) },
              area);
}

TEST_CASE ("Handbuch: Arrangieren, Griffe eines Audio-Clips", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    const auto ids = prepareClips (scene);
    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);

    // Spuren höher (72 + 3 x 16 = 120 px), damit Griffe und Ränder gut zu sehen sind.
    auto& larger = dynamic_cast<juce::Button&> (findByTooltip (window, "Tracks larger"));

    for (int i = 0; i < 3; ++i)
        larger.triggerClick();

    TestEnvironment::pumpMessages (100);

    // Bass-Clip (Takt 5 bis 9) mit einem halben Takt Rand links und rechts.
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    arrange.showTimeRange (3.5 * scene.barSeconds, 8.5 * scene.barSeconds);
    TestEnvironment::pumpMessages (200);

    auto& lane = laneAt (arrange, 2);
    auto& view = clipViewFor (lane, ids.bass);
    const int width = view.getWidth();
    const int height = view.getHeight();
    constexpr int titleHeight = 16; // ClipView::paint
    constexpr int edgeZone = 6;     // TrackLane::edgeZone
    const int pad = fk::ui::theme::spacingSmall;

    // Nummern wie in arranging.md §12. Spurkopf links im Bild, oben Platz für die Nummern der Griffe.
    saveShot (window,
              "arranging/clip-handles",
              { Callout { 1, inWindow (window, view, { pad, 0, 110, titleHeight }), Callout::Placement::inside },
                Callout { 2, inWindow (window, view, view.getFadeInHandle()), Callout::Placement::inside },
                Callout { 3, inWindow (window, view, view.getFadeOutHandle()), Callout::Placement::inside },
                Callout { 4, inWindow (window, view, { 0, titleHeight, edgeZone, height - titleHeight }), Callout::Placement::inside },
                Callout { 5,
                          inWindow (window, view, { width - edgeZone, titleHeight, edgeZone, height - titleHeight }),
                          Callout::Placement::inside },
                Callout { 6, inWindow (window, view, { width - pad - 48, 0, 48, titleHeight }), Callout::Placement::inside } },
              boundsIn (window, lane).withLeft (0).expanded (0, 12));
}

TEST_CASE ("Handbuch: Arrangieren, Bereich über zwei Spuren", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);
    scene.zoomToProject();

    dynamic_cast<juce::Button&> (findByTooltip (window, "Range tool:")).triggerClick();
    TestEnvironment::pumpMessages (100);

    // Bereich Takt 3 bis 5 von der Kick- in die Snare-Spur ziehen (am Raster "Beat").
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& kickLane = laneAt (arrange, 0);
    auto& snareLane = laneAt (arrange, 1);
    const auto mapping = arrange.getTimeMapping();
    const juce::Point<float> from (static_cast<float> (mapping.secondsToX (2.0 * scene.barSeconds)),
                                   static_cast<float> (kickLane.getHeight() / 2));
    const auto to = kickLane
                        .getLocalPoint (&snareLane,
                                        juce::Point<int> (mapping.secondsToX (4.0 * scene.barSeconds),
                                                          snareLane.getHeight() / 2))
                        .toFloat();

    kickLane.mouseDown (mouseEvent (kickLane, from, from));
    kickLane.mouseDrag (mouseEvent (kickLane, to, from));
    kickLane.mouseUp (mouseEvent (kickLane, to, from));
    TestEnvironment::pumpMessages (100);

    const auto& selection = arrange.getRangeSelection();
    REQUIRE (selection.has_value());
    REQUIRE (selection->tracks.size() == 2);

    const auto kickArea = boundsIn (window, kickLane);
    const auto snareArea = boundsIn (window, snareLane);
    const auto rangeArea = juce::Rectangle<int>::leftTopRightBottom (kickArea.getX() + mapping.secondsToX (selection->fromSeconds),
                                                                      kickArea.getY(),
                                                                      kickArea.getX() + mapping.secondsToX (selection->toSeconds),
                                                                      snareArea.getBottom());

    // Nummer wie in arranging.md §10.
    saveShot (window,
              "arranging/range",
              { Callout { 1, rangeArea, Callout::Placement::inside } },
              boundsIn (window, arrange));
}

TEST_CASE ("Handbuch: Arrangieren, Abwurf-Vorschau beim Import", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);
    scene.zoomToProject();

    // Eine Audiodatei über die Bass-Spur bei Takt 3 ziehen (dort ist sie noch leer). Die Datei muss
    // nicht existieren: Die Vorschau prüft nur die Endung.
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& bassLane = laneAt (arrange, 2);
    const auto laneInArrange = arrange.getLocalArea (&bassLane, bassLane.getLocalBounds());
    const auto mapping = arrange.getTimeMapping();
    const auto dropX = fk::ui::theme::trackHeaderWidth + mapping.secondsToX (2.0 * scene.barSeconds);
    juce::StringArray files;
    files.add (scene.env.tempFolder.getChildFile ("Loop.wav").getFullPathName());
    arrange.fileDragMove (files, dropX, laneInArrange.getCentreY());
    TestEnvironment::pumpMessages (100);

    const auto laneArea = boundsIn (window, bassLane);
    const auto lineX = boundsIn (window, arrange).getX() + dropX;

    // Nummern wie in arranging.md §19.
    saveShot (window,
              "arranging/drop",
              { Callout { 1, laneArea, Callout::Placement::inside },
                Callout { 2, { lineX - 1, laneArea.getY(), 3, laneArea.getHeight() }, Callout::Placement::inside } },
              boundsIn (window, arrange));
}

TEST_CASE ("Handbuch: Arrangieren, Sprechblase Tempo and Pitch", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    const auto ids = prepareClips (scene);
    auto* bass = trackNamed (scene, "Bass");
    REQUIRE (bass != nullptr);

    // Die Sprechblase allein, in einem etwas breiteren Rahmen, damit die Nummern rechts Platz haben.
    juce::Component holder;
    fk::ui::ClipStretchPanel panel (scene.env.clipService, scene.env.trackService, *bass, ids.bass);
    holder.setSize (panel.getWidth() + 40, panel.getHeight());
    holder.addAndMakeVisible (panel);
    panel.setTopLeftPosition (0, 0);
    TestEnvironment::pumpMessages (100);

    const auto pitchArea = boundsIn (holder, findByTooltip (holder, "Pitch in semitones"));
    const juce::Rectangle<int> speedArea (fk::ui::theme::spacingMedium,
                                          pitchArea.getBottom() + fk::ui::theme::spacingSmall,
                                          panel.getWidth() - 2 * fk::ui::theme::spacingMedium,
                                          26);

    // Nummern wie in arranging.md §15.
    saveShot (holder,
              "arranging/stretch-panel",
              { calloutFor (1, holder, findByTooltip (holder, "Music mode:"), Callout::Placement::inside),
                calloutFor (2, holder, findByTooltip (holder, "Tempo in BPM"), Callout::Placement::inside),
                calloutFor (3, holder, findByTooltip (holder, "Estimate the tempo"), Callout::Placement::inside),
                Callout { 4, pitchArea, Callout::Placement::inside },
                Callout { 5, speedArea, Callout::Placement::inside } });
}
