#include "fk/ui/ArrangeView.h"
#include "fk/ui/FolderHeader.h"
#include "fk/ui/InsertSlot.h"
#include "fk/ui/Inspector.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/TrackHeader.h"
#include "fk/ui/TrackLane.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <cstdlib>
#include <vector>

// Bilder für docs/manual/track-bake.md. Ändern sich Bake-Buttons, Bake-Markierungen der Insert-Slots,
// Spurkopf oder Spurzeile einer gebackenen Spur, hier die Szenen anpassen und tools/manual.py shots
// laufen lassen (docs/manual/MAINTAINING.md). Backen rendert wirklich (wie BakeServiceTest), ohne
// Fortschrittsfenster: Das schaltet nur die Anwendung ein.

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
/// Breite des Spurzeilen-Ausschnitts rechts vom Spurkopf.
constexpr int laneWidth = 420;

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

/// Sichtbares direktes Kind-Label mit genau diesem Text (oder diesem Anfang), sonst nullptr.
/// Labels haben keinen Tooltip, deshalb die Suche über den Text.
juce::Label* directLabel (juce::Component& parent, const juce::String& text, bool startOnly = false)
{
    for (auto* child : parent.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child); label != nullptr && label->isVisible())
            if (startOnly ? label->getText().startsWith (text) : label->getText() == text)
                return label;

    return nullptr;
}

juce::Label& requireLabel (juce::Component& parent, const juce::String& text, bool startOnly = false)
{
    auto* found = directLabel (parent, text, startOnly);
    INFO ("Kein sichtbares Label \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Spurkopf der Spur mit diesem Namen (das Namensfeld ist ein direktes Label des Kopfs).
fk::ui::TrackHeader& trackHeader (juce::Component& root, const juce::String& name)
{
    std::vector<fk::ui::TrackHeader*> headers;
    collectChildren (root, headers);
    fk::ui::TrackHeader* found = nullptr;

    for (auto* header : headers)
        if (found == nullptr && directLabel (*header, name) != nullptr)
            found = header;

    INFO ("Kein Spurkopf \"" << name << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Spurzeile neben dem Spurkopf: gleiche Höhe im Fenster.
fk::ui::TrackLane& laneBeside (juce::Component& root, fk::ui::TrackHeader& header)
{
    std::vector<fk::ui::TrackLane*> lanes;
    collectChildren (root, lanes);
    const auto headerTop = boundsIn (root, header).getY();
    fk::ui::TrackLane* found = nullptr;

    for (auto* lane : lanes)
        if (found == nullptr && lane->isVisible() && std::abs (boundsIn (root, *lane).getY() - headerTop) <= 1)
            found = lane;

    INFO ("Keine Spurzeile neben dem Spurkopf");
    REQUIRE (found != nullptr);
    return *found;
}

fk::services::TrackHandle& trackNamed (fk::services::TrackService& tracks, const juce::String& name)
{
    fk::services::TrackHandle* found = nullptr;

    for (auto* track : tracks.getAudioTracks())
        if (found == nullptr && tracks.getTrackInfo (*track).name == name)
            found = track;

    INFO ("Keine Spur \"" << name << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

void insertNamed (fk::services::PluginService& plugins, fk::services::TrackHandle& track, const juce::String& name)
{
    juce::String id;

    for (const auto& effect : plugins.getAvailableEffects())
        if (id.isEmpty() && effect.name == name)
            id = effect.id;

    INFO ("Kein Effekt \"" << name << "\"");
    REQUIRE (id.isNotEmpty());
    const auto inserted = plugins.insertPlugin (track, id);
    INFO ((inserted.has_value() ? juce::String() : juce::String (inserted.error().message)));
    REQUIRE (inserted.has_value());
}

/// Synth bekommt drei Inserts: FK EQ, FK Compressor, FK Delay. Vor dem Öffnen des Fensters aufrufen.
void addSynthInserts (ProjectWindowScene& scene)
{
    REQUIRE (scene.synthTrack != nullptr);
    insertNamed (scene.services.plugins, *scene.synthTrack, "FK EQ");
    insertNamed (scene.services.plugins, *scene.synthTrack, "FK Compressor");
    insertNamed (scene.services.plugins, *scene.synthTrack, "FK Delay");
}

/// Spur auswählen wie mit einem Klick in ihren Kopf; der Inspector zeigt sie dann.
void selectTrack (juce::Component& root, const juce::String& name)
{
    auto& header = trackHeader (root, name);
    REQUIRE (static_cast<bool> (header.onSelected));
    header.onSelected();
    TestEnvironment::pumpMessages (100);
}

/// Die acht Insert-Slots des Inspectors, von oben nach unten.
std::vector<fk::ui::InsertSlot*> inspectorSlots (fk::ui::Inspector& inspector)
{
    std::vector<fk::ui::InsertSlot*> slots;

    for (auto* child : inspector.getChildren())
        if (auto* slot = dynamic_cast<fk::ui::InsertSlot*> (child))
            slots.push_back (slot);

    REQUIRE (slots.size() == static_cast<std::size_t> (fk::ui::Inspector::numInsertSlots));
    return slots;
}

/// Markierungen und Bildausschnitt für die Bake-Zeile des Inspectors.
struct InspectorBakeShot
{
    std::vector<Callout> callouts;
    juce::Rectangle<int> area;
};

/// Nummern wie die Tabellen in track-bake.md §2 (vorher) und §8 (nachher):
/// 1 Slots 1 und 2, 2 Slot 3, 3 Bake-Zeile (Beschriftung), 4 die drei Bake-Buttons.
InspectorBakeShot inspectorBakeShot (juce::Component& window, fk::ui::Inspector& inspector)
{
    const auto slots = inspectorSlots (inspector);
    auto& insertsLabel = requireLabel (inspector, "Inserts");
    auto& bakeLabel = requireLabel (inspector, "Bake", true);
    const auto buttons =
        boundsIn (window, findByTooltip (inspector, "Bake Track:")).getUnion (boundsIn (window, findByTooltip (inspector, "Unbake:")));

    const auto inspectorArea = boundsIn (window, inspector);
    const auto area = juce::Rectangle<int>::leftTopRightBottom (inspectorArea.getX(),
                                                                boundsIn (window, insertsLabel).getY() - 4,
                                                                inspectorArea.getRight() + 30,
                                                                buttons.getBottom() + 6);

    // FIX F-1711 (Bake-Feld ohne Tooltip; die Suche läuft über die Slots, bleibt gültig)
    return { { Callout { 1, boundsIn (window, *slots[0]).getUnion (boundsIn (window, *slots[1])), Callout::Placement::inside },
               calloutFor (2, window, *slots[2], Callout::Placement::inside),
               calloutFor (3, window, bakeLabel, Callout::Placement::inside),
               Callout { 4, buttons, Callout::Placement::inside } },
             area };
}

/// Backen über die Oberfläche hat nicht gegriffen: melden und über den Service nachholen, damit das
/// Bild trotzdem entsteht. Ein solcher Hinweis im Testlauf ist ein Fund für FINDINGS.md.
void ensureBaked (ProjectWindowScene& scene, fk::services::TrackHandle& track, int insertCount)
{
    if (scene.env.trackService.getTrackInfo (track).baked)
        return;

    WARN ("Backen über die Oberfläche ohne Wirkung; Szene backt über den BakeService");
    const auto baked = scene.services.bake.bakeInserts (track, insertCount);
    INFO ((baked.has_value() ? juce::String() : juce::String (baked.error().message)));
    REQUIRE (baked.has_value());
    TestEnvironment::pumpMessages (300);
}
} // namespace

TEST_CASE ("Handbuch: Track Bake Markierungen und Bake-Zeile im Inspector", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Synth mit FK EQ, FK Compressor, FK Delay; Bake-Feld von Slot 2 angeklickt (Slots 1 und 2 markiert).
    ProjectWindowScene scene;
    addSynthInserts (scene);
    auto& window = scene.open (1280, 1060);
    selectTrack (window, "Synth");

    auto& inspector = findChild<fk::ui::Inspector> (window);
    const auto slots = inspectorSlots (inspector);
    REQUIRE (static_cast<bool> (slots[1]->onBakeMarkClicked));

    // Kopie aufrufen: Der Klick baut die Rückrufe der Slots neu auf.
    const auto clickMark = slots[1]->onBakeMarkClicked;
    clickMark();
    TestEnvironment::pumpMessages (100);

    (void) requireLabel (inspector, "Bake: inserts 1 to 2 marked");
    const auto shot = inspectorBakeShot (window, inspector);
    saveShot (window, "track-bake/inspector-marks", shot.callouts, shot.area);
}

TEST_CASE ("Handbuch: Track Bake gebackene Spur im Inspector und in der Spurliste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Wie oben, dann "Inserts" geklickt: Instrument und Slots 1 bis 2 gebacken, Slot 3 bleibt live.
    ProjectWindowScene scene;
    addSynthInserts (scene);
    auto& window = scene.open (1280, 1060);
    scene.zoomToProject();
    selectTrack (window, "Synth");

    auto& inspector = findChild<fk::ui::Inspector> (window);
    {
        const auto slots = inspectorSlots (inspector);
        REQUIRE (static_cast<bool> (slots[1]->onBakeMarkClicked));
        const auto clickMark = slots[1]->onBakeMarkClicked;
        clickMark();
        TestEnvironment::pumpMessages (100);
    }

    dynamic_cast<juce::Button&> (findByTooltip (inspector, "Bake Inserts:")).triggerClick();
    TestEnvironment::pumpMessages (500);
    ensureBaked (scene, *scene.synthTrack, 2);

    (void) requireLabel (inspector, "Bake: baked");
    {
        const auto shot = inspectorBakeShot (window, inspector);
        saveShot (window, "track-bake/inspector-baked", shot.callouts, shot.area);
    }

    // Spurkopf und Spurzeile. Nummern wie die Tabelle in track-bake.md §8.
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& header = trackHeader (arrange, "Synth");
    const auto headerArea = boundsIn (window, header);
    const auto laneArea = boundsIn (window, laneBeside (window, header));

    // Schloss: TrackHeader::paint zeichnet es 34 px vor dem rechten Rand, 6 px unter der Oberkante.
    const juce::Rectangle<int> lock (headerArea.getRight() - 34, headerArea.getY() + 6, 14, 14);

    // FIX F-1710 (Tooltip "Click: unbake"; die Suche über "Baked: instrument" bleibt gültig, solange der Anfang bleibt)
    saveShot (window,
              "track-bake/baked-track",
              { calloutFor (1, window, findByTooltip (header, "Baked: instrument"), Callout::Placement::inside),
                Callout { 2, lock, Callout::Placement::inside },
                Callout { 3, laneArea.withWidth (laneWidth), Callout::Placement::inside } },
              juce::Rectangle<int>::leftTopRightBottom (headerArea.getX(), headerArea.getY(), laneArea.getX() + laneWidth, headerArea.getBottom()));
}

TEST_CASE ("Handbuch: Track Bake gebackene Gruppe mit zuführenden Spuren", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Gruppe "Group 5" (Name, den die DAW vergäbe); Kick und Snare gehen hinein.
    ProjectWindowScene scene;
    auto& tracks = scene.env.trackService;
    const auto group = tracks.createGroupTrack ("Group 5");
    REQUIRE (group.has_value());
    const auto groupId = tracks.getTrackId (**group);
    REQUIRE (tracks.setOutput (trackNamed (tracks, "Kick"), groupId).has_value());
    REQUIRE (tracks.setOutput (trackNamed (tracks, "Snare"), groupId).has_value());

    auto& window = scene.open (1280, 1060);

    // Untere Zone aus, damit Ordner, vier Spuren und die Gruppe untereinander Platz haben.
    dynamic_cast<juce::Button&> (findByTooltip (window, "Lower zone on/off")).triggerClick();
    TestEnvironment::pumpMessages (100);
    scene.zoomToProject();

    // Wie die Wahl "Bake group (with all feeding tracks)" im Bake-Menü des Gruppenkopfs.
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    {
        auto& groupHeader = trackHeader (arrange, "Group 5");
        REQUIRE (static_cast<bool> (groupHeader.onBakeRequested));
        const auto request = groupHeader.onBakeRequested;
        request (fk::ui::BakeAction::bakeInserts);
        TestEnvironment::pumpMessages (500);
    }

    ensureBaked (scene, **group, -1);
    REQUIRE (tracks.getTrackInfo (trackNamed (tracks, "Kick")).bakedIntoGroup == "Group 5");

    // Köpfe nach dem Backen neu suchen (die Spurliste kann neu aufgebaut sein).
    auto& folder = findChild<fk::ui::FolderHeader> (arrange);
    auto& groupHeader = trackHeader (arrange, "Group 5");
    auto& kickHeader = trackHeader (arrange, "Kick");
    const auto folderArea = boundsIn (window, folder);
    const auto kickLane = boundsIn (window, laneBeside (window, kickHeader));
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (
        folderArea.getX(), folderArea.getY(), kickLane.getX() + laneWidth, boundsIn (window, groupHeader).getBottom());

    // Nummern wie die Tabelle in track-bake.md §6.
    saveShot (window,
              "track-bake/group-baked",
              { calloutFor (1, window, findByTooltip (groupHeader, "Baked: instrument"), Callout::Placement::inside),
                calloutFor (2, window, findByTooltip (kickHeader, "Baked into group"), Callout::Placement::inside),
                Callout { 3, kickLane.withWidth (laneWidth), Callout::Placement::inside } },
              shotArea);
}
