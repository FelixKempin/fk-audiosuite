#pragma once

#include "fk/ui/ProjectComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Das Hauptfenster. Besitzt den Inhalt als Member-Wert (→ LEARNING.md §4).
class ProjectWindow final : public juce::DocumentWindow
{
public:
    ProjectWindow (const juce::String& title, UiServices services);

    void closeButtonPressed() override;

    /// Fragt bei ungespeicherten Änderungen nach, schließt das Projekt und ruft dann action (z. B. Beenden).
    void closeProjectThen (std::function<void()> action);

private:
    ProjectComponent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectWindow)
};

} // namespace fk::ui
