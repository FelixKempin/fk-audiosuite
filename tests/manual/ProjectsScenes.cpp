#include "fk/ui/ProjectComponent.h"
#include "fk/ui/ProjectSettingsDialog.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <array>
#include <chrono>
#include <functional>

// Bilder für docs/manual/projects.md. Ändern sich Projekt-Buttons oder der Dialog Projekteinstellungen,
// hier die Szene anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

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
/// Erstes sichtbares Label (beliebig tief), dessen Text mit einem der Anfänge beginnt. Labels haben keinen
/// Tooltip, findByTooltip findet sie darum nicht.
juce::Label& findLabel (juce::Component& root, const juce::StringArray& textStarts)
{
    std::function<juce::Label*(juce::Component&)> search = [&] (juce::Component& parent) -> juce::Label*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->isVisible())
                    for (const auto& textStart : textStarts)
                        if (label->getText().startsWith (textStart))
                            return label;

            if (auto* found = search (*child))
                return found;
        }

        return nullptr;
    };

    auto* found = search (root);
    INFO ("Kein sichtbares Label mit Text \"" << textStarts.joinIntoString ("\" / \"") << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}
} // namespace

TEST_CASE ("Handbuch: Projekte, Buttons der Werkzeugleiste", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    ProjectWindowScene scene;
    // Breit wie im Kapitel Projektfenster, damit New, Open und Save ihre Beschriftung zeigen.
    auto& window = scene.open (2400, 800);

    // Nummern wie die Tabelle in projects.md, Abschnitt "Where to find the project functions".
    const std::array<const char*, 6> tooltips { "New project:",  "Open project", "Recent projects",
                                                "Save project",  "Save as:",     "Project settings:" };

    std::vector<Callout> callouts;

    for (std::size_t i = 0; i < tooltips.size(); ++i)
        callouts.push_back (calloutFor (static_cast<int> (i) + 1, window, findByTooltip (window, tooltips[i]), Callout::Placement::above));

    const auto top = boundsIn (window, findDirectChild<juce::MenuBarComponent> (window)).getBottom();
    const auto bottom = boundsIn (window, findDirectChild<juce::Viewport> (window)).getY(); // Inspector
    const auto left = boundsIn (window, findByTooltip (window, tooltips.front())).getX() - 8;
    const auto right = boundsIn (window, findByTooltip (window, tooltips.back())).getRight() + 8;

    saveShot (window,
              "projects/toolbar-buttons",
              callouts,
              juce::Rectangle<int>::leftTopRightBottom (juce::jmax (0, left), top, right, bottom));
}

TEST_CASE ("Handbuch: Projekte, Dialog Projekteinstellungen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Demo-Projekt mit Standardeinstellungen (48000 Hz, WAV, 24 Bit, Keep takes, Merge, 25 fps).
    // Zeile 2 hängt vom Rechner ab: ohne Audiogerät "No audio device active" (Switch device versteckt),
    // mit Gerät "Device: <rate> Hz, matches/differs". Der Text in projects.md beschreibt beide Fälle.
    // FIX F-301 (Crossfade-Haken zeigt immer den Standard)
    ProjectWindowScene scene;
    scene.services.autosave.setInterval (std::chrono::minutes (5)); // Vorgabe, unabhängig von früheren Testläufen

    fk::ui::ProjectSettingsDialog dialog (scene.services.projectSettings, scene.services.autosave);
    TestEnvironment::pumpMessages (100);

    auto& toggle = findByTooltip (dialog, "When two audio clips overlap");

    // Nummern wie die Tabelle in projects.md, Abschnitt "Project settings".
    saveShot (dialog,
              "projects/project-settings",
              { calloutFor (1, dialog, findByTooltip (dialog, "Sample rate of the project"), Callout::Placement::inside),
                calloutFor (2, dialog, findLabel (dialog, { "No audio device active", "Device:" }), Callout::Placement::inside),
                calloutFor (3, dialog, findByTooltip (dialog, "File format of new recordings"), Callout::Placement::inside),
                calloutFor (4, dialog, findByTooltip (dialog, "24 bit is enough"), Callout::Placement::inside),
                calloutFor (5, dialog, findByTooltip (dialog, "Keep takes:"), Callout::Placement::inside),
                calloutFor (6, dialog, findByTooltip (dialog, "New clip:"), Callout::Placement::inside),
                calloutFor (7, dialog, findByTooltip (dialog, "Frames per second"), Callout::Placement::inside),
                // Haken über die ganze Breite: Rahmen kürzer, damit die Nummer rechts daneben im Bild bleibt.
                Callout { 8, boundsIn (dialog, toggle).withWidth (360), Callout::Placement::inside },
                calloutFor (9, dialog, findByTooltip (dialog, "Automatic backup to Backups/"), Callout::Placement::inside),
                calloutFor (10, dialog, findLabel (dialog, { "Format and bit depth apply" }), Callout::Placement::inside) });
}
