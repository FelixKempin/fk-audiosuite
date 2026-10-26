#pragma once

#include "fk/core/Result.h"
#include "fk/engine/ExpressionMap.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <optional>
#include <vector>

namespace fk::services
{

/// Expression Maps (Cubase: Expression Maps): mitgelieferte (FK Orchestra Streicher über Kanäle,
/// Keyswitches ab C0 für fremde Libraries) und eigene als JSON im Ordner userFolder. Einer Spur
/// zugeordnet wird eine Kopie der Map (sie reist mit dem Projekt); die Wiedergabe setzt die
/// Spielweisen der Noten danach um (fk::engine::createSequenceWithArticulations).
class ExpressionMapService final
{
public:
    static constexpr auto orchestraStringsId = "fk-orchestra-strings";
    static constexpr auto keyswitchesId = "keyswitch-c0";

    ExpressionMapService (ProjectService& projectService, TrackService& trackService, juce::File userFolder);

    [[nodiscard]] std::vector<engine::ExpressionMap> getMaps() const;
    [[nodiscard]] std::optional<engine::ExpressionMap> getMap (const juce::String& mapId) const;

    /// Die Map, die in der Spur steht (Kopie), oder keine.
    [[nodiscard]] std::optional<engine::ExpressionMap> getMapForTrack (te::AudioTrack& track) const;
    [[nodiscard]] std::optional<engine::ExpressionMap> getMapForTrack (const juce::String& trackId) const;

    /// Map der Spur zuordnen (Kopie in die Spur); leere Kennung entfernt sie.
    [[nodiscard]] core::Result<void> setMapForTrack (te::AudioTrack& track, const juce::String& mapId);
    [[nodiscard]] core::Result<void> setMapForTrack (const juce::String& trackId, const juce::String& mapId);

    /// Spielweisen-Liste (JSON, Format von ExpressionMap::toJson oder die Liste eines Plugins) übernehmen.
    [[nodiscard]] core::Result<engine::ExpressionMap> importMap (const juce::File& file);
    [[nodiscard]] core::Result<void> exportMap (const juce::String& mapId, const juce::File& file) const;

private:
    [[nodiscard]] static engine::ExpressionMap orchestraStrings();
    [[nodiscard]] static engine::ExpressionMap genericKeyswitches();

    ProjectService& projectService;
    TrackService& trackService;
    juce::File userFolder;
};

} // namespace fk::services
