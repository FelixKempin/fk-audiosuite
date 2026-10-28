#include "fk/services/AutomationService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/AutomationHeader.h"
#include "fk/ui/AutomationLane.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/Theme.h"
#include "fk/ui/TimeMapping.h"
#include "fk/ui/TrackHeader.h"
#include "fk/ui/TrackLane.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <vector>

// Bilder für docs/manual/automation.md. Ändern sich Automationslage, Lagenkopf oder der Automation-
// Button im Spurkopf, hier die Szenen anpassen und tools/manual.py shots laufen lassen
// (docs/manual/MAINTAINING.md).
// FIX F-1306 (Tooltips sagen "automation track", die Suche hier nutzt diese Anfänge)

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
/// Alle sichtbaren Kinder vom Typ T in beliebiger Tiefe, in Kind-Reihenfolge.
template <typename ComponentType>
void collectVisible (juce::Component& parent, std::vector<ComponentType*>& found)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* typed = dynamic_cast<ComponentType*> (child); typed != nullptr && typed->isVisible())
            found.push_back (typed);

        collectVisible (*child, found);
    }
}

template <typename ComponentType>
std::vector<ComponentType*> visibleChildren (juce::Component& parent)
{
    std::vector<ComponentType*> found;
    collectVisible (parent, found);
    return found;
}

/// Spurkopf der Spur mit diesem Namen (das Namensfeld ist ein direktes Label des Kopfs; Labels haben
/// keinen Tooltip, deshalb Suche über den Text).
fk::ui::TrackHeader& trackHeader (juce::Component& root, const juce::String& name)
{
    fk::ui::TrackHeader* found = nullptr;

    for (auto* header : visibleChildren<fk::ui::TrackHeader> (root))
        for (auto* child : header->getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (found == nullptr && label->isVisible() && label->getText() == name)
                    found = header;

    INFO ("Kein Spurkopf \"" << name << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

fk::services::TrackHandle& trackNamed (ProjectWindowScene& scene, const juce::String& name)
{
    fk::services::TrackHandle* found = nullptr;

    for (auto* track : scene.env.trackService.getAudioTracks())
        if (found == nullptr && scene.env.trackService.getTrackInfo (*track).name == name)
            found = track;

    INFO ("Keine Spur \"" << name << "\"");
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

/// Wert an einer Stelle des Bereichs (0 = unten, 1 = oben), damit die Szene für jeden Parameter passt.
float valueAt (const fk::services::AutomationParameterInfo& parameter, float fraction)
{
    return parameter.minValue + fraction * (parameter.maxValue - parameter.minValue);
}

/// Die drei Parameter der Synth-Spur, die die Bilder zeigen: Lautstärke, Pan und ein Synth-Parameter
/// (Cutoff, sonst der erste Parameter von FK Synth).
struct SynthParameters
{
    fk::services::AutomationParameterInfo volume;
    fk::services::AutomationParameterInfo pan;
    fk::services::AutomationParameterInfo plugin;
};

SynthParameters synthParameters (fk::services::AutomationService& automation, fk::services::TrackHandle& track)
{
    const auto parameters = automation.getParameters (track);
    REQUIRE (parameters.size() > 2);

    SynthParameters result { parameters[0], parameters[1], {} };
    REQUIRE (result.volume.name == "Volume");
    REQUIRE (result.pan.name == "Pan");

    bool foundCutoff = false;
    bool foundAny = false;

    for (const auto& parameter : parameters)
    {
        if (parameter.ownerName.isEmpty())
            continue;

        if (! foundCutoff && parameter.name == "Cutoff")
        {
            result.plugin = parameter;
            foundCutoff = true;
            foundAny = true;
        }
        else if (! foundAny)
        {
            result.plugin = parameter;
            foundAny = true;
        }
    }

    REQUIRE (foundAny);

    if (! foundCutoff)
        WARN ("FK Synth hat keinen Parameter \"Cutoff\"; Bild zeigt " << result.plugin.ownerName << ": "
                                                                      << result.plugin.name);

    return result;
}

/// Synth-Spur mit drei Automationslagen (Volume, Pan, Synth-Parameter) und Punkten darauf.
/// Vor dem Öffnen des Fensters aufrufen (ProjectWindowScene: spätere Änderungen lösen die
/// Abtastraten-Rückfrage aus). Pan: Takt 2 links, Takt 4 rechts, Takt 6 links, Mitte bei 6,5 Takten.
SynthParameters prepareSynthAutomation (ProjectWindowScene& scene)
{
    auto& automation = scene.services.automation;
    auto& track = *scene.synthTrack;
    const double bar = scene.barSeconds;
    const auto parameters = synthParameters (automation, track);

    automation.addLane (track, parameters.volume.id);
    automation.addLane (track, parameters.pan.id);
    automation.addLane (track, parameters.plugin.id);
    REQUIRE (automation.getNumLanes (track) == 3);

    const auto& volume = parameters.volume;
    REQUIRE (automation.addPoint (track, volume.id, 0.0, valueAt (volume, 0.6f)).has_value());
    REQUIRE (automation.addPoint (track, volume.id, 2.0 * bar, valueAt (volume, 0.6f)).has_value());
    REQUIRE (automation.addPoint (track, volume.id, 4.0 * bar, valueAt (volume, 0.85f)).has_value());
    REQUIRE (automation.addPoint (track, volume.id, 6.0 * bar, valueAt (volume, 0.85f)).has_value());
    REQUIRE (automation.addPoint (track, volume.id, 7.0 * bar, valueAt (volume, 0.4f)).has_value());

    const auto& pan = parameters.pan;
    REQUIRE (automation.addPoint (track, pan.id, 1.0 * bar, valueAt (pan, 0.2f)).has_value());
    REQUIRE (automation.addPoint (track, pan.id, 3.0 * bar, valueAt (pan, 0.8f)).has_value());
    REQUIRE (automation.addPoint (track, pan.id, 5.0 * bar, valueAt (pan, 0.2f)).has_value());
    REQUIRE (automation.addPoint (track, pan.id, 6.5 * bar, valueAt (pan, 0.5f)).has_value());

    const auto& plugin = parameters.plugin;
    REQUIRE (automation.addPoint (track, plugin.id, 0.0, valueAt (plugin, 0.2f)).has_value());
    REQUIRE (automation.addPoint (track, plugin.id, 4.0 * bar, valueAt (plugin, 0.9f)).has_value());
    REQUIRE (automation.addPoint (track, plugin.id, 8.0 * bar, valueAt (plugin, 0.3f)).has_value());

    return parameters;
}

/// Mittelpunkt eines Punkts in Koordinaten der Lage, wie AutomationLane::paint ihn zeichnet
/// (verticalMargin = 6 px oben und unten).
juce::Point<int> pointCentre (const fk::ui::TimeMapping& mapping,
                              const fk::services::AutomationParameterInfo& parameter,
                              int laneHeight,
                              double seconds,
                              float value)
{
    constexpr int verticalMargin = 6;
    const auto proportion = (value - parameter.minValue) / (parameter.maxValue - parameter.minValue);
    const auto usable = static_cast<float> (laneHeight - verticalMargin * 2);
    const auto y = static_cast<float> (laneHeight - verticalMargin) - proportion * usable;
    return { mapping.secondsToX (seconds), juce::roundToInt (y) };
}
} // namespace

TEST_CASE ("Handbuch: Automation, Automationslagen einer Spur", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    prepareSynthAutomation (scene);
    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);
    scene.zoomToProject();

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    arrange.setAutomationShown (scene.synthTrack, true);
    TestEnvironment::pumpMessages (200);

    auto& header = trackHeader (arrange, "Synth");
    const auto headers = visibleChildren<fk::ui::AutomationHeader> (arrange);
    const auto lanes = visibleChildren<fk::ui::AutomationLane> (arrange);
    INFO ("Erwartet: drei sichtbare Automationslagen der Synth-Spur");
    REQUIRE (headers.size() == 3);
    REQUIRE (lanes.size() == 3);

    // Rahmen der Lagen rechts kürzer, damit die Nummer den Parameternamen oben rechts nicht verdeckt.
    auto volumeLane = calloutFor (6, window, *lanes[0], Callout::Placement::inside);
    volumeLane.target = volumeLane.target.withTrimmedRight (70);
    auto pluginLane = calloutFor (7, window, *lanes[2], Callout::Placement::inside);
    pluginLane.target = pluginLane.target.withTrimmedRight (130);

    const auto top = boundsIn (window, header).getY();
    const auto bottom = boundsIn (window, *lanes[2]).getBottom();
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (0, top, window.getWidth(), bottom);

    // FIX F-1305 (Modus auch außerhalb der ersten Lage sichtbar: Bild würde sich ändern)
    // Nummern wie die Tabelle in automation.md §2 (At a glance).
    saveShot (window,
              "automation/overview",
              { calloutFor (1, window, findByTooltip (header, "Automation track on/off"), Callout::Placement::above),
                calloutFor (2, window, findByTooltip (*headers[0], "Parameter whose curve"), Callout::Placement::inside),
                calloutFor (3, window, findByTooltip (*headers[0], "Read: play back only"), Callout::Placement::inside),
                calloutFor (4, window, findByTooltip (*headers[1], "Another automation track"), Callout::Placement::inside),
                calloutFor (5, window, findByTooltip (*headers[0], "Remove this automation track"), Callout::Placement::inside),
                volumeLane,
                pluginLane },
              shotArea);
}

TEST_CASE ("Handbuch: Automation, Punkte und Kurve", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    const auto parameters = prepareSynthAutomation (scene);
    auto& track = *scene.synthTrack;
    const double bar = scene.barSeconds;

    // Eine echte Lage (Pan, zweite Lage der Spur) mit fester Zeitskala: 40 px je Sekunde ab 0, so
    // lassen sich die Punkte genau markieren. Höhe wie im Arrangement.
    constexpr int laneHeight = fk::ui::theme::automationLaneHeight;
    constexpr int headerWidth = fk::ui::theme::trackHeaderWidth;
    constexpr int laneWidth = 680;
    constexpr int rightMargin = 40; // Platz für die Nummer neben dem Parameternamen

    fk::ui::LaneSettings settings;
    settings.mapping.pixelsPerSecond = 40.0;
    settings.mapping.firstVisibleSecond = 0.0;

    juce::Component stage;
    fk::ui::AutomationHeader laneHeader (scene.services.automation, track, 1);
    fk::ui::AutomationLane lane (scene.services.automation,
                                 scene.env.timelineService,
                                 track,
                                 1,
                                 scene.env.trackService.getTrackInfo (track).colour);
    stage.addAndMakeVisible (laneHeader);
    stage.addAndMakeVisible (lane);
    stage.setSize (headerWidth + laneWidth + rightMargin, laneHeight);
    laneHeader.setBounds (0, 0, headerWidth, laneHeight);
    lane.setBounds (headerWidth, 0, laneWidth, laneHeight);
    lane.setSettings (settings);
    lane.refresh();
    TestEnvironment::pumpMessages (100);

    const auto& pan = parameters.pan;
    const auto first = pointCentre (settings.mapping, pan, laneHeight, 1.0 * bar, valueAt (pan, 0.2f));
    const auto second = pointCentre (settings.mapping, pan, laneHeight, 3.0 * bar, valueAt (pan, 0.8f));
    const auto third = pointCentre (settings.mapping, pan, laneHeight, 5.0 * bar, valueAt (pan, 0.2f));
    const auto last = pointCentre (settings.mapping, pan, laneHeight, 6.5 * bar, valueAt (pan, 0.5f));

    // Letzten Punkt anfassen (Maustaste unten, nicht loslassen): Er wird hell und zeigt seinen Wert.
    lane.mouseDown (mouseEvent (lane, last.toFloat(), last.toFloat()));
    TestEnvironment::pumpMessages (100);

    const auto inStage = [&stage, &lane] (juce::Rectangle<int> local) { return stage.getLocalArea (&lane, local); };
    constexpr int radius = 5; // AutomationLane::pointRadius

    // Nummern wie in automation.md §5 (Reading the lane).
    saveShot (stage,
              "automation/points",
              { Callout { 1, inStage (juce::Rectangle<int> (radius * 2, radius * 2).withCentre (second)), Callout::Placement::inside },
                Callout { 2,
                          inStage (juce::Rectangle<int>::leftTopRightBottom (second.x + radius + 2, 0, third.x - radius - 2, laneHeight)),
                          Callout::Placement::inside },
                Callout { 3, inStage ({ last.x + radius + 2, last.y - 16, 60, 14 }), Callout::Placement::inside },
                Callout { 4, inStage ({ 0, 0, juce::jmax (80, first.x - radius - 2), laneHeight }), Callout::Placement::inside },
                Callout { 5, inStage ({ laneWidth - 4 - 36, 2, 36, 14 }), Callout::Placement::inside } });

    lane.mouseUp (mouseEvent (lane, last.toFloat(), last.toFloat()));
}

TEST_CASE ("Handbuch: Automation, neue leere Lage", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& bass = trackNamed (scene, "Bass");

    // Wie ein erster Klick auf den Automation-Button: eine Lage ohne Parameter. Vor dem Öffnen
    // anlegen, dann ändert das Aufklappen das Projekt nicht mehr.
    scene.services.automation.addLane (bass);
    REQUIRE (scene.services.automation.getNumLanes (bass) == 1);

    auto& window = scene.open (1000, 820);
    showArrangementOnly (window);
    scene.zoomToProject();

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    arrange.setAutomationShown (&bass, true);
    TestEnvironment::pumpMessages (200);

    auto& header = trackHeader (arrange, "Bass");
    const auto headers = visibleChildren<fk::ui::AutomationHeader> (arrange);
    const auto lanes = visibleChildren<fk::ui::AutomationLane> (arrange);
    INFO ("Erwartet: eine sichtbare Automationslage der Bass-Spur");
    REQUIRE (headers.size() == 1);
    REQUIRE (lanes.size() == 1);

    const auto top = boundsIn (window, header).getY();
    const auto bottom = boundsIn (window, *lanes[0]).getBottom();

    // Nummern wie in automation.md §3 (Showing automation lanes).
    saveShot (window,
              "automation/new-lane",
              { calloutFor (1, window, findByTooltip (*headers[0], "Parameter whose curve"), Callout::Placement::inside),
                calloutFor (2, window, *lanes[0], Callout::Placement::inside) },
              juce::Rectangle<int>::leftTopRightBottom (0, top, window.getWidth(), bottom));
}
