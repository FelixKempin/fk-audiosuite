#include "fk/ui/ArrangeView.h"
#include "fk/ui/AudioSettingsComponent.h"
#include "fk/ui/ClipView.h"
#include "fk/ui/KeyEditor.h"
#include "fk/ui/LowerZone.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/Theme.h"
#include "fk/ui/TrackHeader.h"
#include "fk/ui/TransportBar.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <memory>
#include <optional>
#include <vector>

// Bilder für docs/manual/getting-started.md. Ändern sich Startfenster, Audio-Einstellungen oder die
// Schritte des ersten Projekts, hier die Szenen anpassen und tools/manual.py shots laufen lassen
// (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findChild;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::FullServices;
using fk::tests::manual::saveShot;

namespace
{

/// Projektfenster wie nach dem Programmstart: ohne Demo-Projekt (ProjectWindowScene baut immer eines).
/// Ein Projekt entsteht, wenn gewünscht, vor dem Fenster (sonst Abtastraten-Rückfrage, siehe ProjectWindowScene).
struct GettingStartedScene
{
    GettingStartedScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        env.engineContext.registerInternalPlugins();
    }

    ~GettingStartedScene()
    {
        window.reset(); // Fenster vor den Services abbauen (es hängt als Listener daran)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void createProject (const juce::String& name)
    {
        REQUIRE (env.projectService.createProject (env.tempFolder, name).has_value());
    }

    fk::ui::ProjectComponent& open (int width, int height)
    {
        window = std::make_unique<fk::ui::ProjectComponent> (services.ui());
        window->setSize (width, height);
        TestEnvironment::pumpMessages (300);
        return *window;
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    FullServices services { env };
    std::unique_ptr<fk::ui::ProjectComponent> window;
};

/// Sichtbares Label mit genau diesem Text (beliebig tief).
juce::Label* findLabel (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* label = dynamic_cast<juce::Label*> (child))
            if (label->isVisible() && label->getText() == text)
                return label;

        if (auto* found = findLabel (*child, text))
            return found;
    }

    return nullptr;
}

/// Das Feld, an dem ein Label mit diesem Text hängt (JUCE-Geräteauswahl: "Sample rate:" usw.).
juce::Component* findLabelled (juce::Component& root, const juce::String& labelText)
{
    if (auto* label = findLabel (root, labelText))
        return label->getAttachedComponent();

    return nullptr;
}

/// Wie findByTooltip, aber ohne REQUIRE: Felder der Geräteauswahl gibt es nur mit geöffnetem Audiogerät.
juce::Component* searchTooltip (juce::Component& root, const juce::String& tooltipStart)
{
    for (auto* child : root.getChildren())
    {
        if (auto* client = dynamic_cast<juce::TooltipClient*> (child))
            if (child->isVisible() && client->getTooltip().startsWith (tooltipStart))
                return child;

        if (auto* found = searchTooltip (*child, tooltipStart))
            return found;
    }

    return nullptr;
}

juce::Button* findButtonByText (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*> (child))
            if (button->isVisible() && button->getButtonText() == text)
                return button;

        if (auto* found = findButtonByText (*child, text))
            return found;
    }

    return nullptr;
}

/// Sichtbares Label mit dem Projektnamen bzw. "No project: New or Open" rechts in der Werkzeugleiste.
juce::Label& projectNameLabel (fk::ui::ProjectComponent& window, const juce::String& text)
{
    juce::Label* found = nullptr;

    for (auto* child : window.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
            if (found == nullptr && label->isVisible() && label->getText() == text)
                found = label;

    INFO ("Kein Projektname \"" << text << "\" in der Werkzeugleiste");
    REQUIRE (found != nullptr);
    return *found;
}

/// Der Name ist breit (Rest der Leiste): Markierung nur über seinem rechten Ende, wo der Text steht.
Callout projectNameCallout (int number, fk::ui::ProjectComponent& window, const juce::String& text)
{
    const auto area = boundsIn (window, projectNameLabel (window, text));
    return { number, area.withTrimmedLeft (juce::jmax (0, area.getWidth() - 160)), Callout::Placement::above };
}

/// Hintergrund mit Rand rechts, damit die Nummern neben Feldern am rechten Rand noch im Bild liegen.
struct ShotFrame final : juce::Component
{
    void paint (juce::Graphics& g) override { g.fillAll (fk::ui::theme::backgroundBase); }
};

} // namespace

TEST_CASE ("Handbuch: Erste Schritte Fenster nach dem Start", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    GettingStartedScene scene;
    // Größe wie beim Programmstart (ProjectWindow: 1280 × 720), noch kein Projekt offen.
    auto& window = scene.open (1280, 720);

    // Nummern wie die Tabelle in getting-started.md, Abschnitt "Starting FK Audio Suite".
    saveShot (window,
              "getting-started/first-start",
              { calloutFor (1, window, findByTooltip (window, "New project:"), Callout::Placement::above),
                calloutFor (2, window, findByTooltip (window, "Open project"), Callout::Placement::above),
                calloutFor (3, window, findByTooltip (window, "Recent projects"), Callout::Placement::above),
                calloutFor (4, window, findByTooltip (window, "Audio settings:"), Callout::Placement::above),
                projectNameCallout (5, window, "No project: New or Open") });
}

TEST_CASE ("Handbuch: Erste Schritte Audio- und MIDI-Einstellungen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    GettingStartedScene scene;
    // Wie beim Programmstart: Das Bildschirm-Keyboard steht dann als virtueller MIDI-Eingang in der Liste.
    // FIX F-103 (Gerätename "Bildschirm-Keyboard")
    scene.services.midi.ensureVirtualKeyboardExists();
    TestEnvironment::pumpMessages (300);

    ShotFrame frame;
    fk::ui::AudioSettingsComponent settings (scene.env.engineContext, scene.services.midi);
    frame.setSize (settings.getWidth() + 40, settings.getHeight());
    frame.addAndMakeVisible (settings);
    settings.setTopLeftPosition (0, 0);
    TestEnvironment::pumpMessages (300);

    // Nummern wie die Tabelle in getting-started.md, Abschnitt "Audio and MIDI setup".
    // Die Felder 1-7 baut JUCEs Geräteauswahl; welche es gibt, hängt vom Audiogerät des Rechners ab
    // (ohne Gerät fehlen Kanäle, Samplerate und Puffer). Fehlende Felder bekommen keine Nummer.
    std::vector<Callout> callouts;
    const auto add = [&] (int number, std::optional<juce::Rectangle<int>> area, const char* what)
    {
        if (area.has_value())
            callouts.push_back ({ number, *area, Callout::Placement::inside });
        else
            WARN ("Audio-Einstellungen ohne " << what << " (kein Audiogerät?): Nummer " << number << " fehlt im Bild");
    };
    const auto areaOf = [&] (juce::Component* component) -> std::optional<juce::Rectangle<int>>
    {
        if (component == nullptr)
            return std::nullopt;

        return boundsIn (frame, *component);
    };

    // 2: Ausgang mit JUCEs Test-Button; Geräte ohne getrennte Ein- und Ausgänge heißen "Device:".
    // FIX F-104 (zwei Testton-Bedienelemente)
    auto output = areaOf (findLabelled (frame, "Output:"));
    if (! output.has_value())
        output = areaOf (findLabelled (frame, "Device:"));
    if (const auto test = areaOf (searchTooltip (frame, "Plays a test tone")); output.has_value() && test.has_value())
        output = output->getUnion (*test);

    // 3: Eingang; rechts daneben der Pegelmesser, so breit wie der Test-Button darüber.
    auto input = areaOf (findLabelled (frame, "Input:"));
    if (input.has_value() && output.has_value())
        input = input->withRight (output->getRight());

    add (1, areaOf (findLabelled (frame, "Audio device type:")), "Gerätetyp");
    add (2, output, "Ausgang");
    add (3, input, "Eingang");
    add (4, areaOf (findLabelled (frame, "Active output channels:")), "Ausgangskanäle");
    add (5, areaOf (findLabelled (frame, "Active input channels:")), "Eingangskanäle");
    add (6, areaOf (findLabelled (frame, "Sample rate:")), "Samplerate");
    add (7, areaOf (findLabelled (frame, "Audio buffer size:")), "Puffergröße");

    // 8: Liste der MIDI-Eingänge (Schalter direkt in der Komponente), ohne Gerät der Hinweis.
    std::optional<juce::Rectangle<int>> midiList;
    for (auto* child : settings.getChildren())
        if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child); toggle != nullptr && toggle->isVisible())
            midiList = midiList.has_value() ? midiList->getUnion (boundsIn (frame, *toggle)) : boundsIn (frame, *toggle);
    REQUIRE (midiList.has_value());
    add (8, midiList, "MIDI-Eingänge");

    callouts.push_back (calloutFor (9, frame, findByTooltip (frame, "Rescan hardware"), Callout::Placement::inside));

    // 10: Testton-Schalter und Frequenz als eine Zeile.
    auto* toneButton = findButtonByText (frame, "Test tone");
    auto* frequency = findLabelled (frame, "Frequency");
    REQUIRE (toneButton != nullptr);
    REQUIRE (frequency != nullptr);
    callouts.push_back (
        { 10, boundsIn (frame, *toneButton).getUnion (boundsIn (frame, *frequency)), Callout::Placement::inside });

    saveShot (frame, "getting-started/audio-settings", callouts);
}

TEST_CASE ("Handbuch: Erste Schritte leeres Projekt", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    GettingStartedScene scene;
    scene.createProject ("My First Song");
    auto& window = scene.open (1280, 720);

    // Nummern wie die Tabelle in getting-started.md, Abschnitt "Your first project", Schritt 1.
    saveShot (window,
              "getting-started/empty-project",
              { calloutFor (1, window, findByTooltip (window, "Add instrument track"), Callout::Placement::above),
                calloutFor (2, window, findByTooltip (window, "Save project"), Callout::Placement::above),
                projectNameCallout (3, window, "My First Song"),
                calloutFor (4, window, findDirectChild<fk::ui::ArrangeView> (window), Callout::Placement::inside),
                calloutFor (5, window, findDirectChild<fk::ui::LowerZone> (window), Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Erste Schritte erstes Projekt fertig", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    GettingStartedScene scene;
    scene.createProject ("My First Song");

    // Wie nach den Schritten im Kapitel: Instrumentenspur "FK Synth 1" (Name wie über den Button
    // Instrument), ein eintaktiger Clip am Projektanfang (Track › Insert empty MIDI clip at playhead)
    // mit vier Viertelnoten (Anschlag 100 wie mit dem Stift).
    const auto synthId = TestEnvironment::synthId (scene.services.plugins);
    REQUIRE (synthId.isNotEmpty());
    const auto track = scene.env.trackService.createInstrumentTrack ("FK Synth 1", synthId);
    REQUIRE (track.has_value());

    const auto barSeconds = scene.env.timelineService.barLengthSeconds (0.0);
    const auto clipId = scene.services.midiClips.createEmptyClip (**track, 0.0, barSeconds);
    REQUIRE (clipId.has_value());

    std::vector<fk::services::EditableNote> notes;
    const std::vector<int> melody { 60, 64, 67, 72 };

    for (std::size_t i = 0; i < melody.size(); ++i)
        notes.push_back ({ -1, melody[i], static_cast<double> (i), 1.0, 100, {} });

    REQUIRE (scene.services.midiClips.addNotes (*clipId, notes, "Notes").has_value());

    auto& window = scene.open (1280, 900);
    auto& lowerZone = findDirectChild<fk::ui::LowerZone> (window);
    lowerZone.showKeyEditor();
    TestEnvironment::pumpMessages (100);
    findChild<fk::ui::KeyEditor> (lowerZone).setClip (*clipId);
    TestEnvironment::pumpMessages (200);

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& transportBar = findDirectChild<fk::ui::TransportBar> (window);

    // Nummern wie die Tabelle in getting-started.md, Abschnitt "Your first project" (Ergebnis).
    saveShot (window,
              "getting-started/first-project",
              { calloutFor (1, window, findChild<fk::ui::TrackHeader> (arrange), Callout::Placement::inside),
                calloutFor (2, window, findChild<fk::ui::ClipView> (arrange), Callout::Placement::inside),
                calloutFor (3, window, findChild<fk::ui::KeyEditor> (lowerZone), Callout::Placement::inside),
                calloutFor (4, window, findByTooltip (transportBar, "Play ("), Callout::Placement::inside),
                calloutFor (5, window, findByTooltip (window, "Save project"), Callout::Placement::above) });
}
