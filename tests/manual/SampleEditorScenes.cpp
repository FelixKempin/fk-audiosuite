#include "fk/services/ClipService.h"
#include "fk/services/TrackService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/SampleEditor.h"
#include "fk/ui/Theme.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <array>
#include <cstddef>
#include <vector>

// Bilder für docs/manual/sample-editor.md. Ändern sich Leiste, Tooltips, Infozeile oder Aufteilung des
// Sample Editors, hier die Szenen anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::mouseEvent;
using fk::tests::manual::saveShot;
using fk::tests::manual::stripCallout;
using fk::tests::manual::writeHitsWav;

namespace
{
constexpr double clipSeconds = 4.0;

/// Wellenformfläche wie SampleEditor::waveformArea() (Leiste 34 px oben, Infozeile 22 px unten, Rand 4 px).
/// Die Fläche ist kein eigenes Kind, deshalb hier nachgebaut; ändert sich die Aufteilung, hier mitziehen.
juce::Rectangle<int> waveformAreaOf (const juce::Component& editor)
{
    constexpr int toolbarHeight = 34;
    constexpr int infoHeight = 22;
    return editor.getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedBottom (infoHeight).reduced (fk::ui::theme::spacingSmall);
}

/// Die Infozeile: ein Label ohne Tooltip, daher über den Anfang ihres Textes gesucht.
juce::Label& findLabelStartingWith (juce::Component& root, const juce::String& start)
{
    juce::Label* found = nullptr;

    for (auto* child : root.getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child); label != nullptr && label->isVisible() && label->getText().startsWith (start))
        {
            found = label;
            break;
        }

    INFO ("Kein sichtbares Label mit Text \"" << start << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Abschnitt per Maus wählen (Ziehen von fromSeconds bis toSeconds), bei ganz sichtbarem Clip.
void dragSelect (fk::ui::SampleEditor& editor, double fromSeconds, double toSeconds)
{
    const auto area = waveformAreaOf (editor);
    const auto xAt = [&area] (double seconds)
    { return static_cast<float> (area.getX() + static_cast<double> (area.getWidth()) * seconds / clipSeconds); };
    const auto y = static_cast<float> (area.getCentreY());
    const juce::Point<float> down { xAt (fromSeconds), y };
    const juce::Point<float> up { xAt (toSeconds), y };

    editor.mouseDown (mouseEvent (editor, down, down));
    editor.mouseDrag (mouseEvent (editor, up, down));
    editor.mouseUp (mouseEvent (editor, up, down));
}

/// Ein Projekt mit einer Audiospur "Guitar" und einem vier Sekunden langen Clip mit abklingenden
/// Schlägen alle halbe Sekunde, damit die Wellenform Form hat.
struct SampleEditorScene
{
    SampleEditorScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        REQUIRE (env.projectService.createProject (env.tempFolder, "Manual").has_value());

        auto created = env.trackService.createAudioTrack ("Guitar", 1);
        REQUIRE (created.has_value());
        track = *created;

        const auto file = writeHitsWav (env.tempFolder.getChildFile ("Sources").getChildFile ("Guitar.wav"), clipSeconds, 0.5, 0.0, 110.0, 0.18);
        auto imported = env.clipService.importAudioFile (*track, file, 0.0);
        REQUIRE (imported.has_value());
        clipId = *imported;
    }

    ~SampleEditorScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    void open (fk::ui::SampleEditor& editor)
    {
        editor.setSize (900, 260);
        editor.setClip (env.trackService.getTrackId (*track), clipId);
        TestEnvironment::pumpMessages (600); // Wellenform rechnet im Hintergrund
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};
} // namespace

TEST_CASE ("Handbuch: Sample Editor ohne Clip", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    SampleEditorScene scene;
    fk::ui::SampleEditor editor (scene.env.clipService, scene.env.trackService, scene.waveforms);
    editor.setSize (900, 260);
    TestEnvironment::pumpMessages (100);

    saveShot (editor, "sample-editor/empty");
}

TEST_CASE ("Handbuch: Sample Editor Überblick mit nummerierten Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    SampleEditorScene scene;
    fk::ui::SampleEditor editor (scene.env.clipService, scene.env.trackService, scene.waveforms);
    scene.open (editor);

    // Nummern wie in der Tabelle in sample-editor.md, Abschnitt "The editor at a glance".
    // FIX F-1106: Tooltips können sich ändern (Anfänge hier mitziehen)
    const std::array<const char*, 10> toolbar { "Silence:", "Amplify or attenuate", "Normalize:", "Fade in over",
                                                "Fade out over", "Reverse the section", "Invert phase", "Zoom in",
                                                "Zoom out", "Show the whole clip" };
    std::vector<Callout> callouts;

    for (std::size_t i = 0; i < toolbar.size(); ++i)
        callouts.push_back (calloutFor (static_cast<int> (i) + 1, editor, findByTooltip (editor, toolbar[i]), Callout::Placement::above));

    const auto area = waveformAreaOf (editor);
    callouts.push_back ({ 11, area.withTrimmedBottom (18), Callout::Placement::inside });
    callouts.push_back (stripCallout (12, area.withTrimmedTop (area.getHeight() - 16)));
    callouts.push_back (stripCallout (13, boundsIn (editor, findLabelStartingWith (editor, "Guitar"))));

    saveShot (editor, "sample-editor/overview", callouts);
}

TEST_CASE ("Handbuch: Sample Editor Auswahl", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    SampleEditorScene scene;
    fk::ui::SampleEditor editor (scene.env.clipService, scene.env.trackService, scene.waveforms);
    scene.open (editor);
    dragSelect (editor, 1.0, 2.0);
    TestEnvironment::pumpMessages (100);

    // 1 = gewählter Abschnitt (1 s bis 2 s), 2 = Infozeile mit "Selection ... to ...".
    const auto area = waveformAreaOf (editor);
    const auto x1 = area.getX() + area.getWidth() / 4;
    const auto x2 = area.getX() + area.getWidth() / 2;
    saveShot (editor,
              "sample-editor/selection",
              { { 1, juce::Rectangle<int> (x1, area.getY(), x2 - x1, area.getHeight()), Callout::Placement::inside },
                stripCallout (2, boundsIn (editor, findLabelStartingWith (editor, "Guitar"))) });
}

TEST_CASE ("Handbuch: Sample Editor nach Silence auf dem Abschnitt", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    SampleEditorScene scene;
    fk::ui::SampleEditor editor (scene.env.clipService, scene.env.trackService, scene.waveforms);
    scene.open (editor);
    dragSelect (editor, 1.0, 2.0);

    // Wie ein Klick auf den Button, aber synchron (triggerClick liefe über die Nachrichtenschleife).
    auto* silence = dynamic_cast<juce::Button*> (&findByTooltip (editor, "Silence:"));
    REQUIRE (silence != nullptr);
    REQUIRE (silence->onClick != nullptr);
    silence->onClick();
    TestEnvironment::pumpMessages (600); // neue Datei: Wellenform neu rechnen

    // Die Infozeile zeigt den Namen der neuen Datei ("Guitar Stille.wav").
    // FIX F-306
    saveShot (editor, "sample-editor/processed");
}

TEST_CASE ("Handbuch: Sample Editor bei rückwärts laufendem Clip", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    SampleEditorScene scene;
    REQUIRE (scene.env.clipService.setClipsReversed ({ scene.clipId }, true).has_value());
    fk::ui::SampleEditor editor (scene.env.clipService, scene.env.trackService, scene.waveforms);
    scene.open (editor);

    // FIX F-1107: ein rückwärts laufender Clip wird vorwärts gezeichnet
    saveShot (editor, "sample-editor/locked");
}
