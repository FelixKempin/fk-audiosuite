#include "fk/services/BakeService.h"

#include "fk/engine/FkUIBehaviour.h"
#include "fk/services/BakeLock.h"
#include "fk/services/TrackProperties.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

namespace
{
juce::String modeName (BakeMode mode)
{
    switch (mode)
    {
        case BakeMode::track:
            return "track";
        case BakeMode::inserts:
            return "inserts";
        case BakeMode::group:
            return "group";
        case BakeMode::none:
            break;
    }

    return {};
}

BakeMode modeFromName (const juce::String& name)
{
    if (name == "track")
        return BakeMode::track;

    if (name == "group")
        return BakeMode::group;

    return BakeMode::inserts;
}

// Inserts im Sinne der Oberfläche: fremde und eigene Effekt-Plugins, nicht das Instrument.
bool isInsert (te::Plugin& plugin)
{
    return dynamic_cast<te::ExternalPlugin*> (&plugin) != nullptr && ! plugin.isSynth();
}

int indexOfInstrument (te::AudioTrack& track)
{
    for (int i = 0; i < track.pluginList.size(); ++i)
        if (dynamic_cast<te::ExternalPlugin*> (track.pluginList[i]) != nullptr && track.pluginList[i]->isSynth())
            return i;

    return -1;
}

/// Leitet die Spur für die Dauer des Renderns auf den Master und danach zurück, ohne Undo-Schritt.
/// Tracktions Renderer rendert nur Spuren mit Ausgabegerät; vor einer Gruppe lehnt er ab (ADR 0009).
class ScopedRouteToMaster final
{
public:
    explicit ScopedRouteToMaster (te::AudioTrack& track)
        : device (track.state.getChildWithName (te::IDs::OUTPUTDEVICES).getChildWithName (te::IDs::DEVICE))
        , previous (device.getProperty (te::IDs::name))
        , active (track.getOutput().getDestinationTrack() != nullptr && device.isValid())
    {
        if (active)
            device.setProperty (te::IDs::name, te::DeviceManager::getDefaultAudioOutDeviceName (false), nullptr);
    }

    ~ScopedRouteToMaster()
    {
        if (active)
            device.setProperty (te::IDs::name, previous, nullptr);
    }

private:
    juce::ValueTree device;
    juce::var previous;
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE (ScopedRouteToMaster)
};
} // namespace

BakeService::BakeService (ProjectService& project, TrackService& trackService)
    : projectService (project)
    , tracks (trackService)
{
}

core::Result<void> BakeService::bakeTrack (TrackHandle& track)
{
    const auto instrument = indexOfInstrument (track);

    if (instrument < 0)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Bake Track is only available on instrument tracks. On audio tracks, use Bake Inserts.").toStdString());

    return bakeUpTo (track, instrument, BakeMode::track);
}

core::Result<void> BakeService::bakeInserts (TrackHandle& track, int insertCount)
{
    const bool isGroup = tracks.getTrackInfo (track).kind == TrackKind::group;
    int lastIndex = indexOfInstrument (track); // ohne Inserts: nur das Instrument (oder nichts)
    int inserts = 0;

    for (int i = 0; i < track.pluginList.size(); ++i)
    {
        if (! isInsert (*track.pluginList[i]))
            continue;

        if (insertCount >= 0 && inserts >= insertCount)
            break;

        lastIndex = i;
        ++inserts;
    }

    if (lastIndex < 0 && ! isGroup)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Nothing to bake: the track has neither an instrument nor inserts.").toStdString());

    return bakeUpTo (track, lastIndex, isGroup ? BakeMode::group : BakeMode::inserts);
}

core::Result<void> BakeService::bakeUpTo (TrackHandle& track, int pluginIndex, BakeMode mode)
{
    if (projectService.getEdit() == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (bakeLock::findBakedDestination (track) != nullptr)
        return std::unexpected (bakeLock::lockedError (track));

    // Rack-Spuren teilen sich ein Instrument mit anderen Spuren; ein Bake einer einzelnen Spur
    // hätte keinen eigenen Klang (MIDI-Spur) oder nähme den anderen ihre Quelle (Ausgangskanal).
    if (tracks.getTrackInfo (track).inRack)
        return core::fail (core::ErrorCode::unsupported,
                           TRANS ("Tracks of a rack instrument cannot be baked. Export the outputs as stems instead.").toStdString());

    // Neu backen ersetzt den alten Bake (anderer Freeze-Punkt).
    if (bakeLock::isBaked (track))
        if (auto result = unbake (track); ! result)
            return result;

    if (pluginIndex >= 0)
        track.insertFreezePointAfterPlugin (track.pluginList[pluginIndex]);
    else
        track.pluginList.insertPlugin (te::FreezePointPlugin::create(), 0);

    juce::Result frozen = juce::Result::ok();

    {
        const ScopedRouteToMaster route (track);
        frozen = track.setFrozen (true, te::Track::individualFreeze); // rendert sofort (blockierend)
    }

    // Abgebrochen: Die Datei ist unvollständig. Alles zurück auf live, als wäre nichts passiert.
    if (auto* ui = dynamic_cast<engine::FkUIBehaviour*> (&track.edit.engine.getUIBehaviour());
        ui != nullptr && ui->wasLastTaskCancelled())
    {
        if (bakeLock::isBaked (track))
            if (auto undone = unbake (track); ! undone)
                return undone;

        track.removeFreezePoint();
        return core::fail (core::ErrorCode::cancelled, TRANS ("Bake cancelled.").toStdString());
    }

    if (frozen.failed() || ! bakeLock::isBaked (track))
    {
        track.removeFreezePoint();
        return core::fail (core::ErrorCode::ioError,
                           (frozen.failed() ? plugins::i18n::format (TRANS ("Bake failed: {error}"), { { "error", frozen.getErrorMessage() } })
                                                : TRANS ("Bake failed: no audio data was produced."))
                               .toStdString());
    }

    track.state.setProperty (trackProperties::bake, modeName (mode), nullptr);
    return {};
}

core::Result<void> BakeService::unbake (TrackHandle& track)
{
    if (! bakeLock::isBaked (track))
        return {};

    track.unfreeze (te::Track::individualFreeze);
    track.removeFreezePoint();
    track.state.removeProperty (trackProperties::bake, nullptr);

    // Die gerenderte Datei wird nicht mehr gebraucht; Platz auf der Platte zurückgeben.
    te::TemporaryFileManager::getFreezeFileForTrack (track).deleteFile();
    return {};
}

BakeState BakeService::getBakeState (TrackHandle& track) const
{
    BakeState state;
    const auto kind = tracks.getTrackInfo (track).kind;
    state.canBakeTrack = kind == TrackKind::instrument;
    state.canBakeInserts = kind != TrackKind::fx;

    if (auto* group = bakeLock::findBakedDestination (track))
    {
        state.lockedByGroup = true;
        state.lockingGroupName = group->getName();
    }

    if (! bakeLock::isBaked (track))
        return state;

    state.mode = modeFromName (track.state.getProperty (trackProperties::bake).toString());

    for (auto* plugin : track.pluginList)
        if (isInsert (*plugin) && plugin->isFrozen())
            ++state.bakedInserts;

    return state;
}

} // namespace fk::services
