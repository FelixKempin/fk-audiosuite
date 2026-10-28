#include "fk/services/MidiService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/ClipView.h"
#include "fk/ui/Inspector.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/LowerZone.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/Theme.h"
#include "fk/ui/TrackLane.h"
#include "fk/ui/VirtualKeyboard.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <functional>

// Bilder für docs/manual/recording.md. Ändern sich Eingangswahl, Aufnahmebereitschaft, Take-Lanes oder das
// Bildschirm-Keyboard, hier die Szenen anpassen und tools/manual.py shots laufen lassen
// (docs/manual/MAINTAINING.md). Echte Aufnahmen laufen im Test nicht: Die Szenen bauen den Endzustand
// über die Services (Eingang, Scharfschaltung, Takes wie nach einer Loop-Aufnahme).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findChild;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;
using fk::tests::manual::writeHitsWav;

namespace
{

/// Erstes sichtbares Kind vom Typ T (beliebig tief), auf das matches zutrifft; nullptr, wenn keines.
/// Für Elemente ohne Tooltip (Eingangsfeld, R-Button, Pegel), siehe FIX F-609.
template <typename ComponentType>
ComponentType* findVisible (juce::Component& root, const std::function<bool (ComponentType&)>& matches)
{
    for (auto* child : root.getChildren())
    {
        if (! child->isVisible())
            continue;

        if (auto* typed = dynamic_cast<ComponentType*> (child); typed != nullptr && matches (*typed))
            return typed;

        if (auto* nested = findVisible<ComponentType> (*child, matches))
            return nested;
    }

    return nullptr;
}

/// Wie findVisible, die Szene schlägt aber fehl, wenn nichts passt (dann ist das Kapitel zu prüfen).
template <typename ComponentType>
ComponentType& requireVisible (juce::Component& root,
                               const std::function<bool (ComponentType&)>& matches,
                               const char* description)
{
    auto* found = findVisible<ComponentType> (root, matches);
    INFO ("Nicht gefunden: " << description);
    REQUIRE (found != nullptr);
    return *found;
}

template <typename Value>
Value valueOf (fk::core::Result<Value> result)
{
    INFO ((result.has_value() ? juce::String() : juce::String (result.error().message)));
    REQUIRE (result.has_value());
    return *result;
}

/// Kennung des Bildschirm-Keyboards als MIDI-Eingang. Das virtuelle Gerät entsteht hier nach dem
/// Projekt; Tracktion baut die Geräteliste asynchron neu, deshalb kurz warten.
juce::String waitForKeyboardInput (ProjectWindowScene& scene)
{
    scene.services.midi.ensureVirtualKeyboardExists();
    juce::String keyboardId;

    for (int waited = 0; keyboardId.isEmpty() && waited < 3000; waited += 100)
    {
        TestEnvironment::pumpMessages (100);

        for (const auto& input : scene.env.trackService.getAvailableMidiInputs())
            if (input.name == fk::services::MidiService::getVirtualKeyboardName())
                keyboardId = input.id;
    }

    INFO ("Bildschirm-Keyboard erscheint nicht als MIDI-Eingang");
    REQUIRE (keyboardId.isNotEmpty());
    return keyboardId;
}

} // namespace

TEST_CASE ("Handbuch: Aufnahme Eingang, Aufnahmebereitschaft und Pegel", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Instrumentenspur "Synth" des Demo-Projekts: Eingang Bildschirm-Keyboard (einziger MIDI-Eingang,
    // den jede Testumgebung hat), scharf geschaltet. Alles vor dem Fenster einstellen.
    // FIX F-103 (das Eingangsfeld zeigt "Bildschirm-Keyboard")
    // FIX F-603 (das Eingangsfeld im Spurkopf kann erscheinen, dann ändert sich der Spurkopf im Bild)
    ProjectWindowScene scene;
    const auto keyboardId = waitForKeyboardInput (scene);
    REQUIRE (scene.env.trackService.setAudioInput (*scene.synthTrack, keyboardId).has_value());
    scene.env.trackService.setArmed (*scene.synthTrack, true);
    REQUIRE (scene.env.trackService.isArmed (*scene.synthTrack));

    auto& window = scene.open (1280, 900);
    auto& inspector = findChild<fk::ui::Inspector> (window);
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    const auto keyboardName = fk::services::MidiService::getVirtualKeyboardName();

    auto& inputBox = requireVisible<juce::ComboBox> (
        inspector, [&keyboardName] (juce::ComboBox& box) { return box.getText() == keyboardName; }, "Eingang im Inspector");
    auto& inspectorArm = requireVisible<juce::TextButton> (
        inspector, [] (juce::TextButton& button) { return button.getButtonText() == "R"; }, "R im Inspector");
    auto& inspectorMeter =
        requireVisible<fk::ui::LevelMeter> (inspector, [] (fk::ui::LevelMeter&) { return true; }, "Pegel im Inspector");

    // Im Arrangement ist nur die Synth-Spur scharf: ihr R ist der einzige eingeschaltete R-Button.
    auto& headerArm = requireVisible<juce::TextButton> (
        arrange,
        [] (juce::TextButton& button) { return button.getButtonText() == "R" && button.getToggleState(); },
        "R im Spurkopf");
    auto* header = headerArm.getParentComponent();
    REQUIRE (header != nullptr);
    auto& headerMeter =
        requireVisible<fk::ui::LevelMeter> (*header, [] (fk::ui::LevelMeter&) { return true; }, "Pegel im Spurkopf");

    // Ausschnitt: Inspector, Spurköpfe und ein Stück der Spuren (unter 1000 px breit).
    const auto inspectorArea = boundsIn (window, inspector);
    const auto headerArea = boundsIn (window, *header);
    const juce::Rectangle<int> area (0, inspectorArea.getY(), headerArea.getRight() + 260, inspectorArea.getHeight());

    // Nummern wie die Tabelle in recording.md, Abschnitt "Arming tracks".
    saveShot (window,
              "recording/arm-and-input",
              { calloutFor (1, window, inputBox, Callout::Placement::inside),
                calloutFor (2, window, inspectorArm, Callout::Placement::inside),
                calloutFor (3, window, inspectorMeter, Callout::Placement::inside),
                calloutFor (4, window, headerArm, Callout::Placement::inside),
                calloutFor (5, window, headerMeter, Callout::Placement::inside) },
              area);
}

TEST_CASE ("Handbuch: Aufnahme Take-Lanes mit Comping", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Zustand wie nach einer Loop-Aufnahme über vier Takte mit drei Durchgängen: Audiospur "Vocals",
    // ein Clip mit drei Takes (Dateinamen wie bei der Loop-Aufnahme), Take-Lanes aufgeklappt, und
    // Takt 2 per Comping auf Take 3 geschaltet. Ordner "Drums" zugeklappt, damit die Spur ins Bild passt.
    ProjectWindowScene scene;
    auto& trackService = scene.env.trackService;
    auto& clipService = scene.env.clipService;

    const auto folders = trackService.getFolders();
    REQUIRE (! folders.empty());
    trackService.setFolderCollapsed (*folders.front(), true);

    auto* vocals = valueOf (trackService.createAudioTrack ("Vocals", 1));
    const auto takeFolder = scene.env.tempFolder.getChildFile ("RecordingTakes");
    const auto length = 4.0 * scene.barSeconds;
    const auto beat = scene.barSeconds / 4.0;
    const auto take1 = writeHitsWav (takeFolder.getChildFile ("Vocals.wav"), length, 2.0 * beat, 0.0, 220.0, 0.35);
    const auto take2 = writeHitsWav (takeFolder.getChildFile ("Vocals_take_2.wav"), length, beat, 0.1, 247.0, 0.15);
    const auto take3 = writeHitsWav (takeFolder.getChildFile ("Vocals_take_3.wav"), length, 1.5 * beat, 0.05, 196.0, 0.25);

    const auto clipId = valueOf (clipService.importAudioFile (*vocals, take1, 0.0));
    REQUIRE (clipService.addTakeFromFile (clipId, take2).has_value());
    REQUIRE (clipService.addTakeFromFile (clipId, take3).has_value());
    REQUIRE (clipService.setShowingTakes (clipId, true).has_value());
    const auto middleId = valueOf (clipService.useTakeForRange (clipId, 2, scene.barSeconds, 2.0 * scene.barSeconds));

    auto& window = scene.open (1280, 1000);
    scene.zoomToProject();
    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);

    auto& firstPiece = requireVisible<fk::ui::ClipView> (
        arrange, [&clipId] (fk::ui::ClipView& view) { return view.getClipInfo().id == clipId; }, "erstes Clip-Stück");
    auto& middlePiece = requireVisible<fk::ui::ClipView> (
        arrange, [&middleId] (fk::ui::ClipView& view) { return view.getClipInfo().id == middleId; }, "Comping-Stück");
    auto* lane = firstPiece.getParentComponent();
    REQUIRE (lane != nullptr);

    // Fläche der Take-Lanes: unter allen Clip-Stücken der Spur, eine Lane je Take.
    juce::Rectangle<int> clipsArea;

    for (auto* child : lane->getChildren())
        if (auto* view = dynamic_cast<fk::ui::ClipView*> (child))
            clipsArea = clipsArea.isEmpty() ? boundsIn (window, *view) : clipsArea.getUnion (boundsIn (window, *view));

    const juce::Rectangle<int> takeLanes (clipsArea.getX(),
                                          clipsArea.getBottom() + fk::ui::theme::spacingSmall,
                                          clipsArea.getWidth(),
                                          3 * fk::ui::TrackLane::takeLaneHeight);

    const auto laneArea = boundsIn (window, *lane);
    const auto arrangeArea = boundsIn (window, arrange);
    const juce::Rectangle<int> area (arrangeArea.getX(),
                                     laneArea.getY() - fk::ui::theme::spacingSmall,
                                     juce::jmin (1000, window.getWidth() - arrangeArea.getX()),
                                     laneArea.getHeight() + 2 * fk::ui::theme::spacingSmall);

    // Nummern wie die Tabelle in recording.md, Abschnitt "Take lanes and comping".
    saveShot (window,
              "recording/take-lanes",
              { calloutFor (1, window, firstPiece, Callout::Placement::inside),
                Callout { 2, takeLanes, Callout::Placement::inside },
                calloutFor (3, window, middlePiece, Callout::Placement::inside) },
              area);
}

TEST_CASE ("Handbuch: Aufnahme Bildschirm-Keyboard", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // FIX F-001 (Oktavbezeichnung der Bereichsanzeige)
    // FIX F-608 (Bereichsanzeige folgt dem Rollen; im Bild ändert sich nichts, solange nicht gerollt wird)
    ProjectWindowScene scene;
    auto& window = scene.open (1280, 900);
    auto& lowerZone = findDirectChild<fk::ui::LowerZone> (window);
    lowerZone.showKeyboard();
    TestEnvironment::pumpMessages (200);

    auto& keyboard = findChild<fk::ui::VirtualKeyboard> (lowerZone);
    keyboard.scrollToMiddleC(); // wie beim ersten Öffnen; andere Szenen können das schon verbraucht haben
    TestEnvironment::pumpMessages (100);

    auto& rangeLabel =
        requireVisible<juce::Label> (keyboard, [] (juce::Label&) { return true; }, "Bereichsanzeige");
    auto& keys = requireVisible<juce::MidiKeyboardComponent> (
        keyboard, [] (juce::MidiKeyboardComponent&) { return true; }, "Tasten");

    auto area = boundsIn (window, keyboard);
    area = area.withWidth (juce::jmin (1000, area.getWidth()));

    // Nummern wie die Tabelle in recording.md, Abschnitt "The on-screen keyboard".
    saveShot (window,
              "recording/keyboard",
              { calloutFor (1, window, findByTooltip (keyboard, "Shift up one octave"), Callout::Placement::inside),
                calloutFor (2, window, findByTooltip (keyboard, "Shift down one octave"), Callout::Placement::inside),
                calloutFor (3, window, rangeLabel, Callout::Placement::inside),
                calloutFor (4, window, keys, Callout::Placement::inside) },
              area);
}
