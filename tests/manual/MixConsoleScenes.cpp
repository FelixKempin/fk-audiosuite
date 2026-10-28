#include "fk/ui/ChannelStrip.h"
#include "fk/ui/InsertSlot.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/LowerZone.h"
#include "fk/ui/MixConsole.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/SendRow.h"
#include "fk/ui/ZoneSplitter.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <vector>

// Bilder für docs/manual/mixconsole.md. Ändern sich MixConsole, Kanalzug oder Insert-Slots, hier die
// Szenen anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

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
/// Breite des Master-Bereichs rechts in der MixConsole (MixConsole.cpp, masterWidth).
constexpr int masterAreaWidth = 120;

/// Platz rechts neben schmalen Ausschnitten für die Nummern kleiner Ziele.
constexpr int calloutMargin = 30;

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

/// Sichtbares direktes Kind-Label mit genau diesem Text, sonst nullptr (Labels haben keinen Tooltip).
juce::Label* directLabel (juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child); label != nullptr && label->isVisible())
            if (label->getText() == text)
                return label;

    return nullptr;
}

juce::Label& requireLabel (juce::Component& parent, const juce::String& text)
{
    auto* found = directLabel (parent, text);
    INFO ("Kein sichtbares Label \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Kanalzug der Spur mit diesem Namen (das Namensfeld ist ein direktes Label des Kanalzugs).
fk::ui::ChannelStrip& stripNamed (juce::Component& root, const juce::String& name)
{
    std::vector<fk::ui::ChannelStrip*> strips;
    collectChildren (root, strips);
    fk::ui::ChannelStrip* found = nullptr;

    for (auto* strip : strips)
        if (found == nullptr && directLabel (*strip, name) != nullptr)
            found = strip;

    INFO ("Kein Kanalzug \"" << name << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Sichtbarer Text-Button (M, S) direkt in parent.
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

/// Sichtbare direkte Kinder vom Typ T in Kind-Reihenfolge (Insert-Slots, Send-Zeilen eines Kanalzugs).
template <typename ComponentType>
std::vector<ComponentType*> directChildren (juce::Component& parent)
{
    std::vector<ComponentType*> found;

    for (auto* child : parent.getChildren())
        if (auto* typed = dynamic_cast<ComponentType*> (child); typed != nullptr && typed->isVisible())
            found.push_back (typed);

    return found;
}

/// Vereinigung der Rechtecke aller sichtbaren direkten Kinder vom Typ T.
template <typename ComponentType>
juce::Rectangle<int> unionOf (juce::Component& root, juce::Component& parent)
{
    juce::Rectangle<int> area;

    for (auto* child : directChildren<ComponentType> (parent))
        area = area.isEmpty() ? boundsIn (root, *child) : area.getUnion (boundsIn (root, *child));

    INFO ("Keine sichtbaren Kinder des gesuchten Typs");
    REQUIRE (! area.isEmpty());
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

juce::String effectId (fk::services::PluginService& plugins, const juce::String& name)
{
    juce::String id;

    for (const auto& effect : plugins.getAvailableEffects())
        if (id.isEmpty() && effect.name == name)
            id = effect.id;

    INFO ("Kein Effekt \"" << name << "\"");
    REQUIRE (id.isNotEmpty());
    return id;
}

juce::String insertNamed (fk::services::PluginService& plugins,
                          fk::services::TrackHandle& track,
                          const juce::String& name)
{
    const auto inserted = plugins.insertPlugin (track, effectId (plugins, name));
    INFO ((inserted.has_value() ? juce::String() : juce::String (inserted.error().message)));
    REQUIRE (inserted.has_value());
    return *inserted;
}

/// Mischt das Demo-Projekt (Kick, Snare, Bass, Synth) zu einem kleinen Mix, mit den Namen, die die DAW
/// selbst vergäbe. Vor dem Öffnen des Fensters aufrufen:
///   Gruppe "Group 5" (Kick und Snare gehen hinein), FX-Spur "FX 1" mit FK Reverb,
///   Kick: Trim +3 dB; Snare: Phase gedreht;
///   Bass: FK Compressor (Sidechain von Kick), FK EQ (umgangen), Send an FX 1 (-12 dB, pre), Fader -4 dB;
///   Synth: FK Delay, Send an FX 1 (-8 dB, post), Pan etwas links.
void buildMix (ProjectWindowScene& scene)
{
    auto& tracks = scene.env.trackService;
    auto& plugins = scene.services.plugins;
    auto& sends = scene.services.sends;
    auto& mixer = scene.services.mixer;

    auto& kick = trackNamed (tracks, "Kick");
    auto& snare = trackNamed (tracks, "Snare");
    auto& bass = trackNamed (tracks, "Bass");
    auto& synth = trackNamed (tracks, "Synth");

    const auto group = tracks.createGroupTrack ("Group 5");
    REQUIRE (group.has_value());
    const auto groupId = tracks.getTrackId (**group);
    REQUIRE (tracks.setOutput (kick, groupId).has_value());
    REQUIRE (tracks.setOutput (snare, groupId).has_value());

    const auto fx = scene.services.sends.createFxTrack ("FX 1");
    REQUIRE (fx.has_value());
    const auto fxId = tracks.getTrackId (**fx);
    (void) insertNamed (plugins, **fx, "FK Reverb");

    REQUIRE (mixer.setTrimDb (kick, 3.0f).has_value());
    REQUIRE (mixer.setPhaseInverted (snare, true).has_value());

    const auto compressorId = insertNamed (plugins, bass, "FK Compressor");
    REQUIRE (plugins.setSidechainSource (compressorId, tracks.getTrackId (kick)).has_value());
    const auto eqId = insertNamed (plugins, bass, "FK EQ");
    REQUIRE (plugins.setInsertEnabled (eqId, false).has_value());
    REQUIRE (sends.setSendLevel (bass, fxId, -12.0f).has_value());
    REQUIRE (sends.setSendPreFader (bass, fxId, true).has_value());
    mixer.setVolumeDb (bass, -4.0f);

    (void) insertNamed (plugins, synth, "FK Delay");
    REQUIRE (sends.setSendLevel (synth, fxId, -8.0f).has_value());
    mixer.setPan (synth, -0.3f);
}

/// Projektfenster mit gezeigter MixConsole; liefert die MixConsole.
fk::ui::MixConsole& openWithMixConsole (ProjectWindowScene& scene, fk::ui::ProjectComponent*& window)
{
    window = &scene.open (1000, 900);
    findDirectChild<fk::ui::LowerZone> (*window).showMixer();

    // Die untere Zone startet beim Öffnen klein (beim ersten Layout ist das Fenster noch niedrig);
    // wie ein Doppelklick auf den Trenner auf die Standardhöhe, dann 160 px höher gezogen, damit
    // Fader und Meter der Kanalzüge im Bild sind.
    auto& splitter = findDirectChild<fk::ui::ZoneSplitter> (*window);
    REQUIRE (static_cast<bool> (splitter.onReset));
    splitter.onReset();
    splitter.onDragStarted();
    splitter.onDragged (-160);
    TestEnvironment::pumpMessages (300);
    return findChild<fk::ui::MixConsole> (*window);
}
} // namespace

TEST_CASE ("Handbuch: MixConsole Überblick mit Spur-, Gruppen-, FX- und Masterkanal", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    buildMix (scene);
    fk::ui::ProjectComponent* window = nullptr;
    auto& console = openWithMixConsole (scene, window);
    REQUIRE (window != nullptr);

    const auto consoleArea = boundsIn (*window, console);
    const auto masterArea = consoleArea.withTrimmedLeft (consoleArea.getWidth() - masterAreaWidth);

    // FIX F-1401 (vier statt acht Slots im Kanalzug)
    // FIX F-1407 (Sends verkleinern den Fader)
    // Nummern wie die Tabelle in mixconsole.md §2 (The MixConsole at a glance).
    saveShot (*window,
              "mixconsole/overview",
              { calloutFor (1, *window, stripNamed (console, "Kick"), Callout::Placement::inside),
                calloutFor (2, *window, stripNamed (console, "Synth"), Callout::Placement::inside),
                calloutFor (3, *window, stripNamed (console, "Group 5"), Callout::Placement::inside),
                calloutFor (4, *window, stripNamed (console, "FX 1"), Callout::Placement::inside),
                Callout { 5, masterArea, Callout::Placement::inside } },
              consoleArea);
}

TEST_CASE ("Handbuch: MixConsole ein Kanalzug mit allen Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    buildMix (scene);
    fk::ui::ProjectComponent* window = nullptr;
    auto& console = openWithMixConsole (scene, window);
    REQUIRE (window != nullptr);

    auto& strip = stripNamed (console, "Bass");
    const auto stripArea = boundsIn (*window, strip);

    const auto trimRow = boundsIn (*window, findByTooltip (strip, "Trim: level before the inserts"))
                             .getUnion (boundsIn (*window, findByTooltip (strip, "Invert phase")));
    // Pan und Trim sind beide Drehregler: Pan ist der ohne Trim-Tooltip. getTooltip ist nicht const.
    auto& pan = directChildWhere<juce::Slider> (strip, [] (juce::Slider& slider)
                                                { return slider.getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag
                                                         && ! slider.getTooltip().startsWith ("Trim"); });
    auto& fader = directChildWhere<juce::Slider> (strip, [] (const juce::Slider& slider)
                                                  { return slider.getSliderStyle() == juce::Slider::LinearVertical; });
    auto& output = directChildWhere<juce::ComboBox> (strip, [] (const juce::ComboBox&) { return true; });
    const auto buttonRow = boundsIn (*window, textButton (strip, "M")).getUnion (boundsIn (*window, textButton (strip, "S")));

    // Der Fader ist schmaler als 80 px: Seine Nummer stünde rechts daneben auf dem Meter. Rahmen deshalb
    // schmaler, die Nummer liegt dann innen rechts im Fader-Bereich.
    const auto faderArea = boundsIn (*window, fader);
    const auto faderCallout = faderArea.withWidth (juce::jmax (10, faderArea.getWidth() - 26));

    // FIX F-1403 (Tooltips für Pan, Fader, M, S und Ausgang; Suche über Typ und Text bleibt gültig)
    // FIX F-1407 (Sends verkleinern den Fader)
    // Nummern wie die Tabelle in mixconsole.md §3 (The channel strip).
    saveShot (*window,
              "mixconsole/channel-strip",
              { calloutFor (1, *window, requireLabel (strip, "Bass"), Callout::Placement::inside),
                Callout { 2, unionOf<fk::ui::InsertSlot> (*window, strip), Callout::Placement::inside },
                Callout { 3, unionOf<fk::ui::SendRow> (*window, strip), Callout::Placement::inside },
                Callout { 4, trimRow, Callout::Placement::inside },
                calloutFor (5, *window, pan, Callout::Placement::inside),
                Callout { 6, faderCallout, Callout::Placement::inside },
                calloutFor (7, *window, findChild<fk::ui::LevelMeter> (strip), Callout::Placement::inside),
                Callout { 8, buttonRow, Callout::Placement::inside },
                calloutFor (9, *window, output, Callout::Placement::inside) },
              stripArea.withWidth (stripArea.getWidth() + calloutMargin));
}

TEST_CASE ("Handbuch: MixConsole Insert-Slots in ihren Zuständen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    buildMix (scene);
    fk::ui::ProjectComponent* window = nullptr;
    auto& console = openWithMixConsole (scene, window);
    REQUIRE (window != nullptr);

    // Bass: Slot 1 FK Compressor (aktiv, SC von Kick), Slot 2 FK EQ (umgangen), Slots 3 und 4 leer.
    auto& strip = stripNamed (console, "Bass");
    const auto slots = directChildren<fk::ui::InsertSlot> (strip);
    REQUIRE (slots.size() == 4);

    const auto slotsArea = unionOf<fk::ui::InsertSlot> (*window, strip);
    const auto shotArea = slotsArea.expanded (4, 4).withWidth (slotsArea.getWidth() + 8 + calloutMargin);

    // FIX F-1401 (acht Slots wie im Inspector)
    // Nummern wie die Tabelle in mixconsole.md §4 (Insert slots).
    saveShot (*window,
              "mixconsole/insert-slots",
              { calloutFor (1, *window, *slots[0], Callout::Placement::inside),
                calloutFor (2, *window, *slots[1], Callout::Placement::inside),
                calloutFor (3, *window, *slots[2], Callout::Placement::inside) },
              shotArea);
}
