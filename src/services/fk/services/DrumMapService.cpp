#include "fk/services/DrumMapService.h"

#include "fk/services/TrackProperties.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

namespace
{
struct TableEntry
{
    int note;
    const char* name;  // englischer Quelltext, wird beim Bauen der Map übersetzt
    const char* group;
};

DrumMap makeMap (const char* id, const char* name, std::initializer_list<TableEntry> table)
{
    DrumMap map;
    map.id = id;
    map.name = juce::translate (name);
    map.builtIn = true;

    for (const auto& entry : table)
        map.entries.push_back ({ entry.note, juce::translate (entry.name), juce::translate (entry.group) });

    return map;
}

/// Instrumente, deren Name nach einem Schlagzeug klingt, bekommen automatisch die GM-Map.
bool soundsLikeDrums (const juce::String& instrumentName)
{
    for (const auto* word : { "drum", "kit", "percussion", "schlagzeug" })
        if (instrumentName.containsIgnoreCase (word))
            return true;

    return false;
}
} // namespace

juce::String DrumMap::nameFor (int note) const
{
    for (const auto& entry : entries)
        if (entry.note == note)
            return entry.name;

    return {};
}

DrumMapService::DrumMapService (ProjectService& project, TrackService& tracks, juce::File folder)
    : projectService (project)
    , trackService (tracks)
    , userFolder (std::move (folder))
{
}

DrumMap DrumMapService::gmMap()
{
    // General MIDI Schlagzeug (Kanal 10), sortiert wie ein Kit von unten nach oben.
    return makeMap (gmMapId,
                    NEEDS_TRANS ("GM drum kit"),
                    {
                        { 35, NEEDS_TRANS ("Bass drum 2"), NEEDS_TRANS ("Kick") },
                        { 36, NEEDS_TRANS ("Bass drum"), NEEDS_TRANS ("Kick") },
                        { 37, NEEDS_TRANS ("Cross stick"), NEEDS_TRANS ("Snare") },
                        { 38, NEEDS_TRANS ("Snare"), NEEDS_TRANS ("Snare") },
                        { 39, NEEDS_TRANS ("Hand clap"), NEEDS_TRANS ("Snare") },
                        { 40, NEEDS_TRANS ("Snare 2"), NEEDS_TRANS ("Snare") },
                        { 41, NEEDS_TRANS ("Low tom 2"), NEEDS_TRANS ("Toms") },
                        { 43, NEEDS_TRANS ("Low tom 1"), NEEDS_TRANS ("Toms") },
                        { 45, NEEDS_TRANS ("Mid tom 2"), NEEDS_TRANS ("Toms") },
                        { 47, NEEDS_TRANS ("Mid tom 1"), NEEDS_TRANS ("Toms") },
                        { 48, NEEDS_TRANS ("High tom 2"), NEEDS_TRANS ("Toms") },
                        { 50, NEEDS_TRANS ("High tom 1"), NEEDS_TRANS ("Toms") },
                        { 42, NEEDS_TRANS ("Closed hi-hat"), NEEDS_TRANS ("Hi-hat") },
                        { 44, NEEDS_TRANS ("Hi-hat pedal"), NEEDS_TRANS ("Hi-hat") },
                        { 46, NEEDS_TRANS ("Open hi-hat"), NEEDS_TRANS ("Hi-hat") },
                        { 49, NEEDS_TRANS ("Crash 1"), NEEDS_TRANS ("Cymbals") },
                        { 57, NEEDS_TRANS ("Crash 2"), NEEDS_TRANS ("Cymbals") },
                        { 51, NEEDS_TRANS ("Ride"), NEEDS_TRANS ("Cymbals") },
                        { 59, NEEDS_TRANS ("Ride 2"), NEEDS_TRANS ("Cymbals") },
                        { 53, NEEDS_TRANS ("Ride bell"), NEEDS_TRANS ("Cymbals") },
                        { 52, NEEDS_TRANS ("China"), NEEDS_TRANS ("Cymbals") },
                        { 55, NEEDS_TRANS ("Splash"), NEEDS_TRANS ("Cymbals") },
                        { 54, NEEDS_TRANS ("Tambourine"), NEEDS_TRANS ("Percussion") },
                        { 56, NEEDS_TRANS ("Cowbell"), NEEDS_TRANS ("Percussion") },
                        { 58, NEEDS_TRANS ("Vibraslap"), NEEDS_TRANS ("Percussion") },
                        { 60, NEEDS_TRANS ("High bongo"), NEEDS_TRANS ("Percussion") },
                        { 61, NEEDS_TRANS ("Low bongo"), NEEDS_TRANS ("Percussion") },
                        { 62, NEEDS_TRANS ("Muted high conga"), NEEDS_TRANS ("Percussion") },
                        { 63, NEEDS_TRANS ("Open high conga"), NEEDS_TRANS ("Percussion") },
                        { 64, NEEDS_TRANS ("Low conga"), NEEDS_TRANS ("Percussion") },
                        { 65, NEEDS_TRANS ("High timbale"), NEEDS_TRANS ("Percussion") },
                        { 66, NEEDS_TRANS ("Low timbale"), NEEDS_TRANS ("Percussion") },
                        { 67, NEEDS_TRANS ("High agogo"), NEEDS_TRANS ("Percussion") },
                        { 68, NEEDS_TRANS ("Low agogo"), NEEDS_TRANS ("Percussion") },
                        { 69, NEEDS_TRANS ("Cabasa"), NEEDS_TRANS ("Percussion") },
                        { 70, NEEDS_TRANS ("Maracas"), NEEDS_TRANS ("Percussion") },
                        { 75, NEEDS_TRANS ("Claves"), NEEDS_TRANS ("Percussion") },
                        { 76, NEEDS_TRANS ("High wood block"), NEEDS_TRANS ("Percussion") },
                        { 77, NEEDS_TRANS ("Low wood block"), NEEDS_TRANS ("Percussion") },
                        { 80, NEEDS_TRANS ("Muted triangle"), NEEDS_TRANS ("Percussion") },
                        { 81, NEEDS_TRANS ("Open triangle"), NEEDS_TRANS ("Percussion") },
                    });
}

DrumMap DrumMapService::fkDrumsMap()
{
    // Schlagzonen von FK Drums (Belegung wie im Plugin).
    return makeMap (fkDrumsMapId,
                    "FK Drums",
                    {
                        { 36, NEEDS_TRANS ("Bass drum"), NEEDS_TRANS ("Kick") },
                        { 35, NEEDS_TRANS ("Bass drum 2"), NEEDS_TRANS ("Kick") },
                        { 34, NEEDS_TRANS ("Bass drum beater on head"), NEEDS_TRANS ("Kick") },
                        { 38, NEEDS_TRANS ("Snare center"), NEEDS_TRANS ("Snare") },
                        { 39, NEEDS_TRANS ("Snare off-center"), NEEDS_TRANS ("Snare") },
                        { 33, NEEDS_TRANS ("Snare edge"), NEEDS_TRANS ("Snare") },
                        { 40, NEEDS_TRANS ("Snare rimshot"), NEEDS_TRANS ("Snare") },
                        { 37, NEEDS_TRANS ("Snare cross stick"), NEEDS_TRANS ("Snare") },
                        { 31, NEEDS_TRANS ("Snare rim"), NEEDS_TRANS ("Snare") },
                        { 50, NEEDS_TRANS ("Tom 1 center"), NEEDS_TRANS ("Toms") },
                        { 48, NEEDS_TRANS ("Tom 1 edge"), NEEDS_TRANS ("Toms") },
                        { 71, NEEDS_TRANS ("Tom 1 rimshot"), NEEDS_TRANS ("Toms") },
                        { 47, NEEDS_TRANS ("Tom 2 center"), NEEDS_TRANS ("Toms") },
                        { 45, NEEDS_TRANS ("Tom 2 edge"), NEEDS_TRANS ("Toms") },
                        { 72, NEEDS_TRANS ("Tom 2 rimshot"), NEEDS_TRANS ("Toms") },
                        { 43, NEEDS_TRANS ("Tom 3 center"), NEEDS_TRANS ("Toms") },
                        { 65, NEEDS_TRANS ("Tom 3 edge"), NEEDS_TRANS ("Toms") },
                        { 73, NEEDS_TRANS ("Tom 3 rimshot"), NEEDS_TRANS ("Toms") },
                        { 41, NEEDS_TRANS ("Tom 4 center"), NEEDS_TRANS ("Toms") },
                        { 66, NEEDS_TRANS ("Tom 4 edge"), NEEDS_TRANS ("Toms") },
                        { 74, NEEDS_TRANS ("Tom 4 rimshot"), NEEDS_TRANS ("Toms") },
                        { 42, NEEDS_TRANS ("Hi-hat closed tip"), NEEDS_TRANS ("Hi-hat") },
                        { 22, NEEDS_TRANS ("Hi-hat closed edge"), NEEDS_TRANS ("Hi-hat") },
                        { 23, NEEDS_TRANS ("Hi-hat slightly open"), NEEDS_TRANS ("Hi-hat") },
                        { 24, NEEDS_TRANS ("Hi-hat half open"), NEEDS_TRANS ("Hi-hat") },
                        { 25, NEEDS_TRANS ("Open hi-hat"), NEEDS_TRANS ("Hi-hat") },
                        { 46, NEEDS_TRANS ("Hi-hat fully open"), NEEDS_TRANS ("Hi-hat") },
                        { 44, NEEDS_TRANS ("Hi-hat pedal"), NEEDS_TRANS ("Hi-hat") },
                        { 21, NEEDS_TRANS ("Hi-hat foot splash"), NEEDS_TRANS ("Hi-hat") },
                        { 49, NEEDS_TRANS ("Crash 1 edge"), NEEDS_TRANS ("Crash") },
                        { 76, NEEDS_TRANS ("Crash 1 bow"), NEEDS_TRANS ("Crash") },
                        { 77, NEEDS_TRANS ("Crash 1 bell"), NEEDS_TRANS ("Crash") },
                        { 78, NEEDS_TRANS ("Crash 1 choke"), NEEDS_TRANS ("Crash") },
                        { 57, NEEDS_TRANS ("Crash 2 edge"), NEEDS_TRANS ("Crash") },
                        { 79, NEEDS_TRANS ("Crash 2 bow"), NEEDS_TRANS ("Crash") },
                        { 80, NEEDS_TRANS ("Crash 2 bell"), NEEDS_TRANS ("Crash") },
                        { 81, NEEDS_TRANS ("Crash 2 choke"), NEEDS_TRANS ("Crash") },
                        { 51, NEEDS_TRANS ("Ride bow"), NEEDS_TRANS ("Ride") },
                        { 82, NEEDS_TRANS ("Ride bow inner"), NEEDS_TRANS ("Ride") },
                        { 59, NEEDS_TRANS ("Ride edge"), NEEDS_TRANS ("Ride") },
                        { 53, NEEDS_TRANS ("Ride bell"), NEEDS_TRANS ("Ride") },
                        { 83, NEEDS_TRANS ("Ride choke"), NEEDS_TRANS ("Ride") },
                        { 52, NEEDS_TRANS ("FX edge"), NEEDS_TRANS ("FX cymbals") },
                        { 55, NEEDS_TRANS ("FX bow"), NEEDS_TRANS ("FX cymbals") },
                        { 84, NEEDS_TRANS ("FX bell"), NEEDS_TRANS ("FX cymbals") },
                        { 85, NEEDS_TRANS ("FX choke"), NEEDS_TRANS ("FX cymbals") },
                        { 56, NEEDS_TRANS ("Perc open"), NEEDS_TRANS ("Percussion") },
                        { 54, NEEDS_TRANS ("Perc muted"), NEEDS_TRANS ("Percussion") },
                    });
}

std::vector<DrumMap> DrumMapService::getMaps() const
{
    std::vector<DrumMap> maps { gmMap(), fkDrumsMap() };

    for (const auto& file : userFolder.findChildFiles (juce::File::findFiles, false, "*.json"))
        if (auto map = fromJson (file.loadFileAsString()))
        {
            map->id = "user:" + file.getFileNameWithoutExtension();
            map->builtIn = false;
            maps.push_back (std::move (*map));
        }

    return maps;
}

std::optional<DrumMap> DrumMapService::getMap (const juce::String& mapId) const
{
    for (auto& map : getMaps())
        if (map.id == mapId)
            return map;

    return std::nullopt;
}

std::optional<DrumMap> DrumMapService::getMapForTrack (te::AudioTrack& track) const
{
    const auto assigned = track.state.getProperty (trackProperties::drumMap).toString();

    if (assigned == noMapId)
        return std::nullopt;

    if (assigned.isNotEmpty())
        if (auto map = getMap (assigned))
            return map;

    // Automatisch nach dem Instrument.
    const auto info = trackService.getTrackInfo (track);

    if (! playsMidi (info.kind))
        return std::nullopt;

    if (info.instrumentName.containsIgnoreCase ("FK Drums"))
        return fkDrumsMap();

    if (soundsLikeDrums (info.instrumentName))
        return gmMap();

    return std::nullopt;
}

std::optional<DrumMap> DrumMapService::getMapForTrack (const juce::String& trackId) const
{
    if (auto* track = trackService.findTrack (trackId))
        return getMapForTrack (*track);

    return std::nullopt;
}

juce::String DrumMapService::getAssignedMapId (const juce::String& trackId) const
{
    if (auto* track = trackService.findTrack (trackId))
        return track->state.getProperty (trackProperties::drumMap).toString();

    return {};
}

core::Result<void> DrumMapService::setMapForTrack (te::AudioTrack& track, const juce::String& mapId)
{
    if (mapId.isNotEmpty() && mapId != noMapId && ! getMap (mapId))
        return core::fail (core::ErrorCode::notFound, TRANS ("Drum map not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Assign drum map"));
    track.state.setProperty (trackProperties::drumMap, mapId, &projectService.getEdit()->getUndoManager());
    return {};
}

juce::String DrumMapService::toJson (const DrumMap& map)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("name", map.name);
    juce::Array<juce::var> entries;

    for (const auto& entry : map.entries)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("note", entry.note);
        object->setProperty ("name", entry.name);
        object->setProperty ("group", entry.group);
        entries.add (juce::var (object.release()));
    }

    root->setProperty ("entries", entries);
    return juce::JSON::toString (juce::var (root.release()));
}

core::Result<DrumMap> DrumMapService::fromJson (const juce::String& json)
{
    const auto parsed = juce::JSON::parse (json);
    const auto* entries = parsed.getProperty ("entries", {}).getArray();

    if (! parsed.isObject() || entries == nullptr || entries->isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Not a valid drum map (name and entries expected).").toStdString());

    DrumMap map;
    map.name = parsed.getProperty ("name", "Drum-Map").toString();

    for (const auto& entry : *entries)
    {
        const int note = entry.getProperty ("note", -1);

        if (note < 0 || note > 127)
            return core::fail (core::ErrorCode::invalidArgument,
                               plugins::i18n::format (TRANS ("Drum map: invalid note {note}"), { { "note", juce::String (note) } }).toStdString());

        map.entries.push_back ({ note, entry.getProperty ("name", {}).toString(), entry.getProperty ("group", {}).toString() });
    }

    return map;
}

core::Result<DrumMap> DrumMapService::importMap (const juce::File& file)
{
    auto map = fromJson (file.loadFileAsString());

    if (! map)
        return map;

    userFolder.createDirectory();
    const auto target = userFolder.getNonexistentChildFile (juce::File::createLegalFileName (map->name), ".json", false);

    if (! target.replaceWithText (toJson (*map)))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not save the drum map: {path}"), { { "path", target.getFullPathName() } }).toStdString());

    map->id = "user:" + target.getFileNameWithoutExtension();
    return map;
}

core::Result<void> DrumMapService::exportMap (const juce::String& mapId, const juce::File& file) const
{
    const auto map = getMap (mapId);

    if (! map)
        return core::fail (core::ErrorCode::notFound, TRANS ("Drum map not found.").toStdString());

    if (! file.replaceWithText (toJson (*map)))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("File is not writable: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    return {};
}

core::Result<void> DrumMapService::deleteMap (const juce::String& mapId)
{
    if (! mapId.startsWith ("user:"))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Built-in drum maps cannot be deleted.").toStdString());

    const auto file = userFolder.getChildFile (mapId.fromFirstOccurrenceOf ("user:", false, false) + ".json");

    if (! file.deleteFile())
        return core::fail (core::ErrorCode::notFound, TRANS ("Drum map not found.").toStdString());

    return {};
}

} // namespace fk::services
