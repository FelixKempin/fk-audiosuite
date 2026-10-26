#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <optional>
#include <vector>

namespace fk::services
{

/// Ein Instrument einer Drum-Map: Note, Name, Gruppe (Teil des Kits, für Trennlinien im Editor).
struct DrumMapEntry
{
    int note = 36;
    juce::String name;
    juce::String group;
};

/// Drum-Map (Cubase: Drum-Map): welche Note welches Instrument ist, in der Reihenfolge des Editors.
struct DrumMap
{
    juce::String id;
    juce::String name;
    std::vector<DrumMapEntry> entries;
    bool builtIn = false; ///< mitgeliefert (GM, FK Drums), nicht löschbar

    /// Name der Note in dieser Map; leer, wenn die Note nicht vorkommt.
    [[nodiscard]] juce::String nameFor (int note) const;
};

/// Drum-Maps: mitgelieferte (GM-Schlagzeug, FK Drums mit allen Schlagzonen), eigene als JSON-Dateien
/// im Ordner userFolder, Zuordnung zu Spuren. Ohne eigene Zuordnung wählt die Spur automatisch nach
/// ihrem Instrument: FK Drums → FK-Drums-Map, andere Schlagzeug-Instrumente → GM, sonst keine.
class DrumMapService final
{
public:
    static constexpr auto gmMapId = "gm";
    static constexpr auto fkDrumsMapId = "fk-drums";
    static constexpr auto noMapId = "-"; ///< bewusst keine Map (Key Editor statt Drum-Editor)

    DrumMapService (ProjectService& projectService, TrackService& trackService, juce::File userFolder);

    [[nodiscard]] std::vector<DrumMap> getMaps() const;
    [[nodiscard]] std::optional<DrumMap> getMap (const juce::String& mapId) const;

    /// Die Map der Spur: eigene Zuordnung, sonst automatisch nach dem Instrument; leer = keine.
    [[nodiscard]] std::optional<DrumMap> getMapForTrack (te::AudioTrack& track) const;
    [[nodiscard]] std::optional<DrumMap> getMapForTrack (const juce::String& trackId) const;

    /// Gespeicherte Zuordnung der Spur: Map-Kennung, noMapId oder leer (= automatisch).
    [[nodiscard]] juce::String getAssignedMapId (const juce::String& trackId) const;

    /// Zuordnung setzen: Map-Kennung, noMapId (keine) oder leer (wieder automatisch).
    [[nodiscard]] core::Result<void> setMapForTrack (te::AudioTrack& track, const juce::String& mapId);

    /// Eigene Map aus einer JSON-Datei übernehmen (wird in den Map-Ordner kopiert) und als JSON sichern.
    [[nodiscard]] core::Result<DrumMap> importMap (const juce::File& file);
    [[nodiscard]] core::Result<void> exportMap (const juce::String& mapId, const juce::File& file) const;
    [[nodiscard]] core::Result<void> deleteMap (const juce::String& mapId);

    [[nodiscard]] static juce::String toJson (const DrumMap& map);
    [[nodiscard]] static core::Result<DrumMap> fromJson (const juce::String& json);

private:
    [[nodiscard]] static DrumMap gmMap();
    [[nodiscard]] static DrumMap fkDrumsMap();

    ProjectService& projectService;
    TrackService& trackService;
    juce::File userFolder;
};

} // namespace fk::services
