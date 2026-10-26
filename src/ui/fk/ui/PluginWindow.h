#pragma once

#include "fk/services/PluginService.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace fk::ui
{

/// Eigenes Fenster für die Oberfläche eines Plugins. Besitzt den Editor über EditorPtr
/// (gibt ihn JUCE-konform frei); beim Schließen meldet es sich über onClosed.
class PluginWindow final : public juce::DocumentWindow
{
public:
    std::function<void()> onClosed;

    PluginWindow (const juce::String& title, services::EditorPtr editor);
    ~PluginWindow() override;

    void closeButtonPressed() override;

private:
    services::EditorPtr editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

} // namespace fk::ui
