#include "fk/ui/ArrangeView.h"
#include "fk/ui/FolderHeader.h"
#include "fk/ui/InsertSlot.h"
#include "fk/ui/Inspector.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/SendRow.h"
#include "fk/ui/TrackHeader.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <vector>

// Bilder für docs/manual/tracks.md. Ändern sich Spurkopf, Ordnerkopf oder Inspector, hier die Szenen
// anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

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

/// Sichtbares direktes Kind-Label mit genau diesem Text (oder diesem Anfang), sonst nullptr.
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

/// Sichtbarer Text-Button (R, M, S) direkt in parent.
juce::Component& textButton (juce::Component& parent, const juce::String& text)
{
    juce::Component* found = nullptr;

    for (auto* child : parent.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child))
            if (found == nullptr && button->isVisible() && button->getButtonText() == text)
                found = button;

    INFO ("Kein sichtbarer Button \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Erstes sichtbares direktes Kind vom Typ T, das die Bedingung erfüllt.
template <typename ComponentType, typename Predicate>
ComponentType& directChildWhere (juce::Component& parent, Predicate predicate)
{
    ComponentType* found = nullptr;

    for (auto* child : parent.getChildren())
        if (auto* typed = dynamic_cast<ComponentType*> (child))
            if (found == nullptr && typed->isVisible() && predicate (*typed))
                found = typed;

    INFO ("Kein passendes sichtbares Kind");
    REQUIRE (found != nullptr);
    return *found;
}

/// Überschrift im Inspector ("Input", "Output") und das Auswahlfeld direkt darunter, als ein Rechteck.
juce::Rectangle<int> labelledBox (juce::Component& root, juce::Component& inspector, const juce::String& labelText)
{
    auto& label = requireLabel (inspector, labelText);
    auto& box = directChildWhere<juce::ComboBox> (inspector,
                                                  [&label] (const juce::ComboBox& candidate)
                                                  { return candidate.getY() == label.getBottom(); });
    return boundsIn (root, label).getUnion (boundsIn (root, box));
}

/// Vereinigung der Rechtecke aller sichtbaren direkten Kinder vom Typ T (Insert-Slots, Send-Zeilen).
template <typename ComponentType>
juce::Rectangle<int> unionOf (juce::Component& root, juce::Component& parent, juce::Rectangle<int> area)
{
    for (auto* child : parent.getChildren())
        if (auto* typed = dynamic_cast<ComponentType*> (child); typed != nullptr && typed->isVisible())
            area = area.getUnion (boundsIn (root, *typed));

    return area;
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

/// Ergänzt das Demo-Projekt um die übrigen Spurarten, mit den Namen, die die DAW selbst vergäbe:
/// Gruppe "Group 5" (Kick und Snare gehen hinein), FX-Spur "FX 1", MIDI-Spur an einem Gerät "MIDI 7",
/// Rack-Instrument "FK Synth" (MIDI-Spur "FK Synth channel 1", Ausgangskanal "FK Synth 1+2").
/// Vor dem Öffnen des Fensters aufrufen.
void addOtherTrackTypes (ProjectWindowScene& scene)
{
    auto& tracks = scene.env.trackService;

    const auto group = tracks.createGroupTrack ("Group 5");
    REQUIRE (group.has_value());
    const auto groupId = tracks.getTrackId (**group);
    REQUIRE (tracks.setOutput (trackNamed (tracks, "Kick"), groupId).has_value());
    REQUIRE (tracks.setOutput (trackNamed (tracks, "Snare"), groupId).has_value());

    REQUIRE (scene.services.sends.createFxTrack ("FX 1").has_value());
    REQUIRE (scene.services.externalMidi.createMidiTrack ("MIDI 7").has_value());

    const auto synthId = TestEnvironment::synthId (scene.services.plugins);
    REQUIRE (synthId.isNotEmpty());
    REQUIRE (scene.services.racks.createRackInstrument (synthId, "FK Synth").has_value());
}

/// Spur auswählen wie mit einem Klick in ihren Kopf; der Inspector zeigt sie dann.
void selectTrack (juce::Component& root, const juce::String& name)
{
    auto& header = trackHeader (root, name);
    REQUIRE (static_cast<bool> (header.onSelected));
    header.onSelected();
    TestEnvironment::pumpMessages (100);
}

/// Bildausschnitt um den Inspector, rechts etwas breiter für die Nummern neben schmalen Elementen.
juce::Rectangle<int> inspectorShotArea (juce::Component& window, juce::Component& inspector, int bottom)
{
    const auto area = boundsIn (window, inspector);
    return juce::Rectangle<int>::leftTopRightBottom (area.getX(), area.getY(), area.getRight() + 30, bottom);
}
} // namespace

TEST_CASE ("Handbuch: Spuren alle Spurarten in der Spurliste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    addOtherTrackTypes (scene);
    auto& window = scene.open (1280, 1060);

    // Untere Zone aus, damit alle neun Spuren und der Ordner untereinander Platz haben.
    dynamic_cast<juce::Button&> (findByTooltip (window, "Lower zone on/off")).triggerClick();
    TestEnvironment::pumpMessages (100);
    scene.zoomToProject();

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& folder = findChild<fk::ui::FolderHeader> (arrange);
    auto& rackOutput = trackHeader (arrange, "FK Synth 1+2");
    const auto folderArea = boundsIn (window, folder);
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (
        folderArea.getX(), folderArea.getY(), folderArea.getX() + fk::ui::theme::trackHeaderWidth + 300,
        boundsIn (window, rackOutput).getBottom());

    // Nummern wie die Tabelle in tracks.md §2 (Track types at a glance).
    saveShot (window,
              "tracks/track-types",
              { calloutFor (1, window, folder, Callout::Placement::inside),
                calloutFor (2, window, trackHeader (arrange, "Kick"), Callout::Placement::inside),
                calloutFor (3, window, trackHeader (arrange, "Synth"), Callout::Placement::inside),
                calloutFor (4, window, trackHeader (arrange, "Group 5"), Callout::Placement::inside),
                calloutFor (5, window, trackHeader (arrange, "FX 1"), Callout::Placement::inside),
                calloutFor (6, window, trackHeader (arrange, "MIDI 7"), Callout::Placement::inside),
                calloutFor (7, window, trackHeader (arrange, "FK Synth channel 1"), Callout::Placement::inside),
                calloutFor (8, window, rackOutput, Callout::Placement::inside) },
              shotArea);
}

TEST_CASE ("Handbuch: Spuren Spurkopf einer Instrumentenspur", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1280, 1060);
    scene.zoomToProject();
    selectTrack (window, "Synth");

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& header = trackHeader (arrange, "Synth");
    const auto headerArea = boundsIn (window, header);

    // FIX F-105 (Tooltips für R, M, S; die Suche über den Button-Text bleibt gültig)
    // FIX F-603 (Eingangsfeld im Spurkopf: würde rechts in der Knopfreihe erscheinen)
    // Nummern wie die Tabelle in tracks.md §4 (The track header).
    saveShot (window,
              "tracks/header",
              { calloutFor (1, window, requireLabel (header, "Synth"), Callout::Placement::inside),
                calloutFor (2, window, textButton (header, "R"), Callout::Placement::above),
                calloutFor (3, window, textButton (header, "M"), Callout::Placement::above),
                calloutFor (4, window, textButton (header, "S"), Callout::Placement::above),
                calloutFor (5, window, findByTooltip (header, "Automation track on/off"), Callout::Placement::above),
                calloutFor (6, window, findByTooltip (header, "Open instrument:"), Callout::Placement::above),
                calloutFor (7, window, findByTooltip (header, "Bake:"), Callout::Placement::above),
                calloutFor (8, window, findChild<fk::ui::LevelMeter> (header), Callout::Placement::above) },
              headerArea.withWidth (headerArea.getWidth() + 40));
}

TEST_CASE ("Handbuch: Spuren Ordnerspur auf- und zugeklappt", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    auto& window = scene.open (1280, 1060);
    scene.zoomToProject();

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    constexpr int laneWidth = 420;

    {
        auto& folder = findChild<fk::ui::FolderHeader> (arrange);
        auto& folderLane = findChild<fk::ui::FolderLane> (arrange);
        auto& kick = trackHeader (arrange, "Kick");
        const auto folderArea = boundsIn (window, folder);
        const auto laneArea = boundsIn (window, folderLane);
        const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (
            folderArea.getX(), folderArea.getY(), laneArea.getX() + laneWidth,
            boundsIn (window, trackHeader (arrange, "Snare")).getBottom());

        // Nummern wie die Tabelle in tracks.md §11.1 (The folder header).
        saveShot (window,
                  "tracks/folder",
                  { calloutFor (1, window, findByTooltip (folder, "Collapse folder"), Callout::Placement::above),
                    calloutFor (2, window, findByTooltip (folder, "Folder. Double-click"), Callout::Placement::above),
                    calloutFor (3, window, findByTooltip (folder, "Mute all tracks"), Callout::Placement::above),
                    calloutFor (4, window, findByTooltip (folder, "Solo all tracks"), Callout::Placement::above),
                    stripCallout (5, laneArea.withWidth (laneWidth)),
                    calloutFor (6, window, kick, Callout::Placement::inside) },
                  shotArea);

        dynamic_cast<juce::Button&> (findByTooltip (folder, "Collapse folder")).triggerClick();
    }

    // Zuklappen baut die Zeilen neu: Ordnerkopf und Spuren danach neu suchen.
    TestEnvironment::pumpMessages (200);

    auto& folder = findChild<fk::ui::FolderHeader> (arrange);
    auto& folderLane = findChild<fk::ui::FolderLane> (arrange);
    const auto folderArea = boundsIn (window, folder);
    const auto laneArea = boundsIn (window, folderLane);
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (
        folderArea.getX(), folderArea.getY(), laneArea.getX() + laneWidth,
        boundsIn (window, trackHeader (arrange, "Bass")).getBottom());

    saveShot (window,
              "tracks/folder-collapsed",
              { calloutFor (1, window, findByTooltip (folder, "Expand folder"), Callout::Placement::above),
                stripCallout (5, laneArea.withWidth (laneWidth)) },
              shotArea);
}

TEST_CASE ("Handbuch: Spuren Inspector einer Audiospur", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Kick: Mono-Audiospur, Ausgang auf "Group 5", ein Send an "FX 1" (noch aus).
    ProjectWindowScene scene;
    addOtherTrackTypes (scene);
    auto& window = scene.open (1280, 1060);
    selectTrack (window, "Kick");

    auto& inspector = findChild<fk::ui::Inspector> (window);
    auto& bakeLabel = requireLabel (inspector, "Bake");
    const auto bakeArea = boundsIn (window, bakeLabel).getUnion (boundsIn (window, findByTooltip (inspector, "Unbake:")));
    // FIX F-510: die Überschrift "Sends" fehlt im Inspector; nach dem Fix das Label wieder einbeziehen
    // (unionOf<fk::ui::SendRow> (window, inspector, boundsIn (window, requireLabel (inspector, "Sends")))).
    const auto sendsArea = unionOf<fk::ui::SendRow> (window, inspector, {});
    const auto insertsArea =
        unionOf<fk::ui::InsertSlot> (window, inspector, boundsIn (window, requireLabel (inspector, "Inserts")));

    auto& pan = directChildWhere<juce::Slider> (inspector, [] (const juce::Slider& slider)
                                                { return slider.getSliderStyle() == juce::Slider::LinearHorizontal; });
    auto& fader = directChildWhere<juce::Slider> (inspector, [] (const juce::Slider& slider)
                                                  { return slider.getSliderStyle() == juce::Slider::LinearVertical; });
    const auto panRow = boundsIn (window, findByTooltip (inspector, "Invert phase")).getUnion (boundsIn (window, pan));
    const auto buttonRow = boundsIn (window, textButton (inspector, "R")).getUnion (boundsIn (window, textButton (inspector, "S")));

    // FIX F-602 (R ohne Eingang)
    // Nummern wie die Tabelle in tracks.md §5 (The track in the Inspector).
    saveShot (window,
              "tracks/inspector-audio",
              { calloutFor (1, window, requireLabel (inspector, "Kick"), Callout::Placement::inside),
                calloutFor (2, window, requireLabel (inspector, "Audio track, mono"), Callout::Placement::inside),
                Callout { 3, labelledBox (window, inspector, "Input"), Callout::Placement::inside },
                Callout { 4, labelledBox (window, inspector, "Output"), Callout::Placement::inside },
                Callout { 5, insertsArea, Callout::Placement::inside },
                Callout { 6, bakeArea, Callout::Placement::inside },
                Callout { 7, sendsArea, Callout::Placement::inside },
                Callout { 8, panRow, Callout::Placement::inside },
                calloutFor (9, window, fader, Callout::Placement::inside),
                calloutFor (10, window, findChild<fk::ui::LevelMeter> (inspector), Callout::Placement::inside),
                Callout { 11, buttonRow, Callout::Placement::inside } },
              inspectorShotArea (window, inspector, boundsIn (window, inspector).getBottom()));
}

TEST_CASE ("Handbuch: Spuren Inspector von Instrument-, Rack- und MIDI-Spur", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    addOtherTrackTypes (scene);
    auto& window = scene.open (1280, 1060);
    auto& inspector = findChild<fk::ui::Inspector> (window);

    // Oberer Teil des Inspectors bis unter das Ausgangsfeld; der Rest gleicht der Audiospur.
    const auto topPart = [&window, &inspector]
    {
        const auto output = labelledBox (window, inspector, "Output");
        return inspectorShotArea (window, inspector, output.getBottom() + 8);
    };

    // Instrumentenspur "Synth". Nummern wie tracks.md §5.1.
    selectTrack (window, "Synth");
    saveShot (window,
              "tracks/inspector-instrument",
              { calloutFor (1, window, requireLabel (inspector, "Synth"), Callout::Placement::inside),
                calloutFor (2, window, requireLabel (inspector, "Instrument track"), Callout::Placement::inside),
                calloutFor (3, window, findByTooltip (inspector, "Double-click opens the plugin window"), Callout::Placement::inside),
                calloutFor (4, window, findByTooltip (inspector, "Open instrument: plugin window"), Callout::Placement::above),
                calloutFor (5, window, findByTooltip (inspector, "Replace instrument"), Callout::Placement::above),
                Callout { 6, labelledBox (window, inspector, "Input"), Callout::Placement::inside },
                Callout { 7, labelledBox (window, inspector, "Output"), Callout::Placement::inside } },
              topPart());

    // MIDI-Spur eines Rack-Instruments. Nummern wie tracks.md §5.2.
    selectTrack (window, "FK Synth channel 1");
    saveShot (window,
              "tracks/inspector-rack",
              { calloutFor (1, window, requireLabel (inspector, "Rack instrument:", true), Callout::Placement::inside),
                calloutFor (2, window, findByTooltip (inspector, "MIDI channel of this track"), Callout::Placement::above),
                calloutFor (3, window, findByTooltip (inspector, "Another MIDI track for this rack"), Callout::Placement::above),
                calloutFor (4, window, findByTooltip (inspector, "Enable outputs:"), Callout::Placement::above) },
              topPart());

    // MIDI-Spur an einem externen Gerät. Nummern wie tracks.md §5.3.
    // FIX F-502 (Ausgangsliste ohne MIDI-Ausgang leer, "MIDI to: Master")
    selectTrack (window, "MIDI 7");
    saveShot (window,
              "tracks/inspector-midi-device",
              { calloutFor (1, window, requireLabel (inspector, "MIDI to:", true), Callout::Placement::inside),
                calloutFor (2, window, findByTooltip (inspector, "MIDI channel of this track"), Callout::Placement::above),
                calloutFor (3, window, findByTooltip (inspector, "Program change to the device"), Callout::Placement::above),
                calloutFor (4, window, findByTooltip (inspector, "Bank (CC 0"), Callout::Placement::inside),
                Callout { 5, labelledBox (window, inspector, "Output"), Callout::Placement::inside } },
              topPart());
}
