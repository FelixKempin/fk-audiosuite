#include "fk/services/ExpressionMapService.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

namespace
{
struct TableEntry
{
    const char* id;
    const char* name;   // englischer Quelltext, wird beim Bauen der Map übersetzt
    const char* symbol; // UTF-8
    int channel;
    int keyswitch;
};

engine::ExpressionMap makeMap (const char* id, const char* name, std::initializer_list<TableEntry> table)
{
    engine::ExpressionMap map;
    map.id = id;
    map.name = juce::translate (name);

    for (const auto& entry : table)
        map.articulations.push_back ({ entry.id,
                                       juce::translate (entry.name),
                                       juce::String::fromUTF8 (entry.symbol),
                                       { .channel = entry.channel, .keyswitch = entry.keyswitch } });

    return map;
}
} // namespace

ExpressionMapService::ExpressionMapService (ProjectService& project, TrackService& tracks, juce::File folder)
    : projectService (project)
    , trackService (tracks)
    , userFolder (std::move (folder))
{
}

engine::ExpressionMap ExpressionMapService::orchestraStrings()
{
    // Standardbelegung aus ORCHESTRA.md §5a.3: Kanal = Strich/Verzierung pro Note.
    return makeMap (orchestraStringsId,
                    NEEDS_TRANS ("FK Orchestra strings (channels)"),
                    {
                        { "normal", NEEDS_TRANS ("Normal"), "", 1, -1 },
                        { "legato", NEEDS_TRANS ("Legato"), "leg", 2, -1 },
                        { "stacc", NEEDS_TRANS ("Staccato"), "\xc2\xb7", 3, -1 },
                        { "staccatissimo", NEEDS_TRANS ("Staccatissimo"), "\xe2\x96\xbe", 4, -1 },
                        { "spicc", NEEDS_TRANS ("Spiccato"), "spic", 5, -1 },
                        { "tenuto", NEEDS_TRANS ("Tenuto"), "\xe2\x80\x93", 6, -1 },
                        { "accent", NEEDS_TRANS ("Accent"), ">", 7, -1 },
                        { "marcato", NEEDS_TRANS ("Marcato"), "^", 8, -1 },
                        { "sfz", NEEDS_TRANS ("Sforzando"), "sfz", 9, -1 },
                        { "trem", NEEDS_TRANS ("Tremolo"), "trem", 10, -1 },
                        { "trillHalf", NEEDS_TRANS ("Trill 1/2"), "tr\xc2\xbd", 11, -1 },
                        { "trillWhole", NEEDS_TRANS ("Trill 1"), "tr", 12, -1 },
                        { "pizz", NEEDS_TRANS ("Pizzicato"), "pizz", 13, -1 },
                        { "bartok", NEEDS_TRANS ("Bartok pizzicato"), "Bart", 14, -1 },
                        { "colLegno", NEEDS_TRANS ("Col legno battuto"), "c.l.", 15, -1 },
                        { "harmonic", NEEDS_TRANS ("Harmonic"), "o", 16, -1 },
                    });
}

engine::ExpressionMap ExpressionMapService::genericKeyswitches()
{
    // Übliche Belegung vieler Sample-Libraries: Keyswitches ab C0 (Note 24, Mitte C = C3).
    return makeMap (keyswitchesId,
                    NEEDS_TRANS ("Keyswitches from C0 (generic)"),
                    {
                        { "sustain", NEEDS_TRANS ("Sustain"), "", 0, 24 },
                        { "legato", NEEDS_TRANS ("Legato"), "leg", 0, 25 },
                        { "stacc", NEEDS_TRANS ("Staccato"), "\xc2\xb7", 0, 26 },
                        { "spicc", NEEDS_TRANS ("Spiccato"), "spic", 0, 27 },
                        { "marcato", NEEDS_TRANS ("Marcato"), "^", 0, 28 },
                        { "pizz", NEEDS_TRANS ("Pizzicato"), "pizz", 0, 29 },
                        { "trem", NEEDS_TRANS ("Tremolo"), "trem", 0, 30 },
                        { "trillHalf", NEEDS_TRANS ("Trill 1/2"), "tr\xc2\xbd", 0, 31 },
                        { "trillWhole", NEEDS_TRANS ("Trill 1"), "tr", 0, 32 },
                    });
}

std::vector<engine::ExpressionMap> ExpressionMapService::getMaps() const
{
    std::vector<engine::ExpressionMap> maps { orchestraStrings(), genericKeyswitches() };

    for (const auto& file : userFolder.findChildFiles (juce::File::findFiles, false, "*.json"))
        if (auto map = engine::ExpressionMap::fromJson (file.loadFileAsString()))
        {
            map->id = "user:" + file.getFileNameWithoutExtension();
            maps.push_back (std::move (*map));
        }

    return maps;
}

std::optional<engine::ExpressionMap> ExpressionMapService::getMap (const juce::String& mapId) const
{
    for (auto& map : getMaps())
        if (map.id == mapId)
            return map;

    return std::nullopt;
}

std::optional<engine::ExpressionMap> ExpressionMapService::getMapForTrack (te::AudioTrack& track) const
{
    return engine::ExpressionMap::fromJson (track.state.getProperty (engine::expressionMapProperty).toString());
}

std::optional<engine::ExpressionMap> ExpressionMapService::getMapForTrack (const juce::String& trackId) const
{
    if (auto* track = trackService.findTrack (trackId))
        return getMapForTrack (*track);

    return std::nullopt;
}

core::Result<void> ExpressionMapService::setMapForTrack (te::AudioTrack& track, const juce::String& mapId)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (mapId.isEmpty())
    {
        projectService.beginUndoStep (TRANS ("Remove expression map"));
        track.state.removeProperty (engine::expressionMapProperty, &edit->getUndoManager());
        return {};
    }

    const auto map = getMap (mapId);

    if (! map)
        return core::fail (core::ErrorCode::notFound, TRANS ("Expression map not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Assign expression map"));
    track.state.setProperty (engine::expressionMapProperty, map->toJson(), &edit->getUndoManager());
    return {};
}

core::Result<void> ExpressionMapService::setMapForTrack (const juce::String& trackId, const juce::String& mapId)
{
    if (auto* track = trackService.findTrack (trackId))
        return setMapForTrack (*track, mapId);

    return core::fail (core::ErrorCode::notFound, TRANS ("Track not found.").toStdString());
}

core::Result<engine::ExpressionMap> ExpressionMapService::importMap (const juce::File& file)
{
    auto map = engine::ExpressionMap::fromJson (file.loadFileAsString());

    if (! map || map->articulations.empty())
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Not a valid articulation list (articulations with id, name and output expected).").toStdString());

    userFolder.createDirectory();
    const auto target =
        userFolder.getNonexistentChildFile (juce::File::createLegalFileName (map->name), ".json", false);

    if (! target.replaceWithText (map->toJson()))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not save the map: {path}"), { { "path", target.getFullPathName() } }).toStdString());

    map->id = "user:" + target.getFileNameWithoutExtension();
    return *map;
}

core::Result<void> ExpressionMapService::exportMap (const juce::String& mapId, const juce::File& file) const
{
    const auto map = getMap (mapId);

    if (! map)
        return core::fail (core::ErrorCode::notFound, TRANS ("Expression map not found.").toStdString());

    if (! file.replaceWithText (map->toJson()))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("File is not writable: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    return {};
}

} // namespace fk::services
