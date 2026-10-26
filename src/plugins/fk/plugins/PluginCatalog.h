#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace fk::plugins
{

/// Verzeichnis aller eigenen Plugins: die eine Stelle, an der ein neues Plugin
/// eingetragen wird (Open/Closed: der Host ändert sich dafür nicht).
class PluginCatalog final
{
public:
    [[nodiscard]] static std::vector<juce::PluginDescription> getDescriptions();

    /// Erzeugt eine Instanz, oder nullptr, wenn die Beschreibung kein FK-Plugin ist.
    [[nodiscard]] static std::unique_ptr<juce::AudioPluginInstance>
    createInstance (const juce::PluginDescription& description);

    [[nodiscard]] static bool isInternal (const juce::PluginDescription& description);
};

} // namespace fk::plugins
