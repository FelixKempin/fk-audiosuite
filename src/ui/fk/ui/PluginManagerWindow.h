#pragma once

#include "fk/services/PluginService.h"
#include "fk/ui/PluginLibraryPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::ui
{

/// Plugin-Manager mit zwei Reitern: "Meine Plugins" (Favoriten, Kategorien, Ausblenden) und
/// "Suchen und Scannen" (Scan nach Formaten, Entfernen). Der zweite nutzt JUCEs fertige PluginListComponent; der Scan selbst läuft in einem Unterprozess
/// (FkEngineBehaviour::canScanPluginsOutOfProcess), damit abstürzende Plugins die DAW nicht mitreißen.
class PluginManagerWindow final : public juce::DocumentWindow
{
public:
    std::function<void()> onClosed;

    explicit PluginManagerWindow (services::PluginService& pluginService);

    void closeButtonPressed() override;

    /// Für Tests: der Reiter "Meine Plugins".
    [[nodiscard]] PluginLibraryPanel& getLibraryPanel() noexcept { return libraryPanel; }

private:
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    PluginLibraryPanel libraryPanel;
    juce::PluginListComponent listComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManagerWindow)
};

} // namespace fk::ui
