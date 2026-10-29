#include "fk/ui/ExportDialog.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <functional>
#include <type_traits>
#include <vector>

// Bilder für docs/manual/exporting.md. Ändern sich Felder, Texte oder Tooltips des Export-Dialogs,
// hier die Szene anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).
// Es wird nichts exportiert: Die Szenen zeigen nur den Dialog in seinen drei Zuständen.

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;

namespace
{
/// Platz rechts neben dem Dialog: Nummern kleiner Ziele stehen rechts neben ihnen.
constexpr int exportCalloutMargin = 40;

/// Breite, um die große Flächen (Spurliste, Hinweis) nach rechts in den Rand verlängert werden, damit ihre
/// Nummer im Rand steht und keinen Text verdeckt.
constexpr int exportBadgeReach = 26;

/// Beispielordner statt des Temp-Ordners des Testlaufs (enthält eine UUID): jedes Bild gleich.
const juce::String examplePath { "/home/anna/Music" };

/// Erstes sichtbares Kind (beliebig tief) vom Typ T, auf das die Bedingung passt.
template <typename ComponentType>
ComponentType* findExportChild (juce::Component& root,
                                const std::type_identity_t<std::function<bool (ComponentType&)>>& matches)
{
    for (auto* child : root.getChildren())
    {
        if (auto* typed = dynamic_cast<ComponentType*> (child))
            if (child->isVisible() && matches (*typed))
                return typed;

        if (auto* found = findExportChild<ComponentType> (*child, matches))
            return found;
    }

    return nullptr;
}

/// Label über den Anfang seines Texts (Labels haben keinen Tooltip).
juce::Label& exportLabelStartingWith (juce::Component& root, const juce::String& textStart)
{
    auto* found = findExportChild<juce::Label> (root, [&] (juce::Label& label) { return label.getText().startsWith (textStart); });
    INFO ("Kein sichtbares Label mit Text \"" << textStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Auswahlfeld über den angezeigten Eintrag (Range und Bit depth haben keinen Tooltip). FIX F-1805
juce::ComboBox& exportComboShowing (juce::Component& root, const juce::String& text)
{
    auto* found = findExportChild<juce::ComboBox> (root, [&] (juce::ComboBox& box) { return box.getText() == text; });
    INFO ("Kein sichtbares Auswahlfeld mit \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Häkchen über seine Beschriftung (Spuren in der Stem-Liste haben keinen Tooltip).
juce::ToggleButton& exportToggleNamed (juce::Component& root, const juce::String& text)
{
    auto* found = findExportChild<juce::ToggleButton> (root, [&] (juce::ToggleButton& toggle) { return toggle.getButtonText() == text; });
    INFO ("Kein sichtbares Häkchen \"" << text << "\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Wählt im Auswahlfeld den Eintrag mit diesem Text, mit Benachrichtigung wie ein Klick.
void selectExportItem (juce::ComboBox& box, const juce::String& itemText)
{
    for (int i = 0; i < box.getNumItems(); ++i)
    {
        if (box.getItemText (i) == itemText)
        {
            box.setSelectedId (box.getItemId (i), juce::sendNotificationSync);
            return;
        }
    }

    INFO ("Kein Eintrag \"" << itemText << "\"");
    REQUIRE (false);
}

/// Ersetzt im Zielpfad den Temp-Ordner durch den Beispielordner. Nach jedem Wechsel von Ziel oder Format
/// nötig, weil der Dialog den Pfad dann neu schreibt.
juce::Label& showExamplePath (juce::Component& dialog, const TestEnvironment& env)
{
    const auto tempPath = env.tempFolder.getFullPathName();
    auto& label = exportLabelStartingWith (dialog, tempPath);
    label.setText (examplePath + label.getText().substring (tempPath.length()), juce::dontSendNotification);
    return label;
}

/// Ziel nach rechts in den Rand verlängert (siehe exportBadgeReach).
juce::Rectangle<int> reachingIntoMargin (juce::Component& root, juce::Component& target)
{
    const auto area = boundsIn (root, target);
    return area.withWidth (area.getWidth() + exportBadgeReach);
}

/// Dialog mit Hülle, die rechts Platz für Nummern lässt.
struct ExportDialogShot
{
    explicit ExportDialogShot (ProjectWindowScene& projectScene)
        : scene (projectScene)
        , dialog (fk::ui::ExportDialog::Services { scene.services.exporter,
                                                   scene.env.projectService,
                                                   scene.services.projectSettings,
                                                   scene.env.trackService })
    {
        holder.setSize (dialog.getWidth() + exportCalloutMargin, dialog.getHeight());
        holder.addAndMakeVisible (dialog);
        dialog.setTopLeftPosition (0, 0);
        TestEnvironment::pumpMessages (100);
    }

    ~ExportDialogShot() { holder.removeChildComponent (&dialog); }

    ProjectWindowScene& scene;
    juce::Component holder;
    fk::ui::ExportDialog dialog;
};
} // namespace

TEST_CASE ("Handbuch: Exportieren, Dialog Mixdown", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Demo-Projekt mit Standardwerten: Mixdown, WAV, Entire project, 24 bit, Same as project (48000 Hz),
    // Normalize Off, Dither und Let tails ring out an. Der Hinweis unten ist leer, außer libmp3lame
    // fehlt auf dem Rechner (dann "MP3 is off: ..."); exporting.md beschreibt beide Fälle.
    // FIX F-1805
    ProjectWindowScene scene;
    ExportDialogShot shot (scene);
    auto& dialog = shot.dialog;
    auto& holder = shot.holder;

    auto& path = showExamplePath (dialog, scene.env);
    auto& dither = findByTooltip (dialog, "Fine noise when reducing");

    // Nummern wie die Tabelle in exporting.md, Abschnitt "The export dialog at a glance".
    saveShot (holder,
              "exporting/mixdown",
              { calloutFor (1, holder, findByTooltip (dialog, "Mixdown: the sum as you hear it"), Callout::Placement::inside),
                calloutFor (2, holder, path, Callout::Placement::inside),
                calloutFor (3, holder, findByTooltip (dialog, "Choose file or folder"), Callout::Placement::inside),
                calloutFor (4, holder, findByTooltip (dialog, "WAV/AIFF/FLAC are lossless"), Callout::Placement::inside),
                calloutFor (5, holder, exportComboShowing (dialog, "Entire project"), Callout::Placement::inside),
                calloutFor (6, holder, exportComboShowing (dialog, "24 bit"), Callout::Placement::inside),
                calloutFor (7, holder, findByTooltip (dialog, "44100 Hz for CD"), Callout::Placement::inside),
                calloutFor (8, holder, findByTooltip (dialog, "Peak: loudest point"), Callout::Placement::inside),
                // Dither ist 120 px breit und grenzt an das nächste Häkchen: Rahmen kürzer, Nummer bleibt davor.
                Callout { 9, boundsIn (holder, dither).withWidth (juce::jmax (10, dither.getWidth() - 30)), Callout::Placement::inside },
                calloutFor (10, holder, findByTooltip (dialog, "Let reverb and echo keep running"), Callout::Placement::inside),
                calloutFor (11, holder, findByTooltip (dialog, "Start export"), Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Exportieren, Dialog mit OGG Vorbis", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // OGG statt MP3: OGG ist immer verfügbar, das Bild hängt nicht von libmp3lame ab. Statt Bit depth und
    // Dither erscheint Quality (Vorgabe 192 kbit/s).
    ProjectWindowScene scene;
    ExportDialogShot shot (scene);
    auto& dialog = shot.dialog;
    auto& holder = shot.holder;

    auto* formatBox = dynamic_cast<juce::ComboBox*> (&findByTooltip (dialog, "WAV/AIFF/FLAC are lossless"));
    REQUIRE (formatBox != nullptr);
    selectExportItem (*formatBox, "OGG Vorbis");
    TestEnvironment::pumpMessages (50);
    showExamplePath (dialog, scene.env);

    // Nummern wie die Tabelle in exporting.md, Abschnitt "Lossy formats: OGG and MP3".
    saveShot (holder,
              "exporting/lossy",
              { calloutFor (1, holder, findByTooltip (dialog, "WAV/AIFF/FLAC are lossless"), Callout::Placement::inside),
                calloutFor (2, holder, findByTooltip (dialog, "Higher bitrate = better sound"), Callout::Placement::inside) });
}

TEST_CASE ("Handbuch: Exportieren, Dialog Stems", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Zum Demo-Projekt (Kick, Snare, Bass, Synth; der Ordner Drums ist keine Spur der Liste) eine Gruppe
    // und eine FX-Spur, damit die Liste beide Zusätze zeigt. Bass ist abgewählt.
    // FIX F-1805 FIX F-1807
    ProjectWindowScene scene;
    REQUIRE (scene.env.trackService.createGroupTrack ("Drum Bus").has_value());
    REQUIRE (scene.services.sends.createFxTrack ("Reverb").has_value());

    ExportDialogShot shot (scene);
    auto& dialog = shot.dialog;
    auto& holder = shot.holder;

    dialog.setTarget (fk::ui::ExportDialog::Target::stems);
    TestEnvironment::pumpMessages (50);
    exportToggleNamed (dialog, "Bass").setToggleState (false, juce::dontSendNotification);
    auto& path = showExamplePath (dialog, scene.env);

    // Prüft nebenbei die Beschriftung der Zusätze, die exporting.md nennt.
    (void) exportToggleNamed (dialog, "Drum Bus (group)");
    (void) exportToggleNamed (dialog, "Reverb (FX)");

    auto& allButton = findByTooltip (dialog, "Select all tracks");
    auto& noneButton = findByTooltip (dialog, "Select no tracks");
    const auto buttons = boundsIn (holder, allButton).getUnion (boundsIn (holder, noneButton));

    // Nummern wie die Tabelle in exporting.md, Abschnitt "Exporting stems".
    saveShot (holder,
              "exporting/stems",
              { calloutFor (1, holder, findByTooltip (dialog, "Mixdown: the sum as you hear it"), Callout::Placement::inside),
                calloutFor (2, holder, path, Callout::Placement::inside),
                Callout { 3, buttons, Callout::Placement::inside },
                Callout { 4, reachingIntoMargin (holder, findDirectChild<juce::Viewport> (dialog)), Callout::Placement::inside },
                Callout { 5,
                          reachingIntoMargin (holder, exportLabelStartingWith (dialog, "Each selected track is rendered")),
                          Callout::Placement::inside } });
}

TEST_CASE ("Handbuch: Exportieren, Dialog MIDI-Datei", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    ExportDialogShot shot (scene);
    auto& dialog = shot.dialog;
    auto& holder = shot.holder;

    dialog.setTarget (fk::ui::ExportDialog::Target::midi);
    TestEnvironment::pumpMessages (50);
    auto& path = showExamplePath (dialog, scene.env);

    // Nummern wie die Tabelle in exporting.md, Abschnitt "Exporting a MIDI file".
    saveShot (holder,
              "exporting/midi",
              { calloutFor (1, holder, path, Callout::Placement::inside),
                Callout { 2,
                          reachingIntoMargin (holder, exportLabelStartingWith (dialog, "Type 1 MIDI file")),
                          Callout::Placement::inside } });
}
