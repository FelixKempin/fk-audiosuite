#pragma once

#include "fk/core/Result.h"
#include "fk/services/ContentService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/TrackService.h"

#include <vector>

namespace fk::services
{

/// Ein Eintrag im Instrumenten-Browser.
struct LibraryItem
{
    enum class Kind
    {
        plugin, ///< Instrument-Plugin (FK oder VST3/LV2)
        sfz,    ///< SFZ-Instrument aus einer Bibliothek (FK Sampler)
        preset  ///< eigenes Preset eines Instrument-Plugins
    };

    juce::String key;      ///< stabil: "plugin:<id>", "sfz:<Pfad>", "preset:<Plugin-ID>|<Name>"
    Kind kind = Kind::plugin;
    juce::String name;
    juce::String source;   ///< Hersteller, Bibliothek oder Plugin
    juce::StringArray tags; ///< automatische und eigene, ohne Doppelte
    bool favourite = false;
};

/// Instrumenten- und Preset-Browser (Cubase: MediaBay für Instrumente): ein Index über Plugin-Instrumente,
/// SFZ-Instrumente der Bibliotheken und eigene Presets, mit Suche, Tags und Favoriten. Eigene Tags und
/// Favoriten stehen in einer JSON-Datei (settingsFile); Plugin-Favoriten teilt er mit dem Plugin-Manager.
class LibraryService final
{
public:
    LibraryService (PluginService& pluginService,
                    ContentService& contentService,
                    SamplerService& samplerService,
                    TrackService& trackService,
                    juce::File settingsFile);

    [[nodiscard]] std::vector<LibraryItem> getItems() const;

    /// Suche in Name, Quelle und Tags (alle Wörter müssen passen), optional ein Tag und nur Favoriten.
    [[nodiscard]] std::vector<LibraryItem> search (const juce::String& text, const juce::String& tag, bool favouritesOnly) const;

    /// Alle vorkommenden Tags, alphabetisch.
    [[nodiscard]] juce::StringArray getAllTags() const;

    [[nodiscard]] core::Result<void> setFavourite (const juce::String& key, bool shouldBeFavourite);
    [[nodiscard]] core::Result<void> addTag (const juce::String& key, const juce::String& tag);
    [[nodiscard]] core::Result<void> removeTag (const juce::String& key, const juce::String& tag);

    /// Neue Instrumentenspur für den Eintrag (Plugin, SFZ im FK Sampler oder Plugin mit Preset).
    [[nodiscard]] core::Result<te::AudioTrack*> createTrack (const juce::String& key);

private:
    [[nodiscard]] juce::var loadSettings() const;
    void storeSettings (const juce::var& settings);
    [[nodiscard]] bool exists (const juce::String& key) const;

    PluginService& plugins;
    ContentService& content;
    SamplerService& sampler;
    TrackService& tracks;
    juce::File settingsFile;
};

} // namespace fk::services
