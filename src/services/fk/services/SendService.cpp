#include "fk/services/SendService.h"

#include "fk/services/TrackProperties.h"

namespace fk::services
{

namespace
{
constexpr float silenceDb = -100.0f;
} // namespace

SendService::SendService (ProjectService& project, TrackService& tracks)
    : projectService (project)
    , trackService (tracks)
{
    projectService.addListener (*this);
    projectChanged();
}

SendService::~SendService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

core::Result<te::AudioTrack*> SendService::createFxTrack (const juce::String& name)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const int bus = nextFreeBus();
    auto created = trackService.createAudioTrack (name, 2);

    if (! created)
        return created;

    auto* track = *created;
    auto& undoManager = edit->getUndoManager();
    track->state.setProperty (trackProperties::fx, true, &undoManager);
    track->state.setProperty (trackProperties::fxBus, bus, &undoManager);
    edit->setAuxBusName (bus, name);

    // Der Aux-Return empfängt alle Sends dieses Busses; er steht ganz vorn in der Kette,
    // damit die Inserts der FX-Spur (Hall, Delay) das Summensignal bearbeiten.
    auto returnPlugin = edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {});

    if (auto* auxReturn = dynamic_cast<te::AuxReturnPlugin*> (returnPlugin.get()))
    {
        auxReturn->busNumber = bus;
        track->pluginList.insertPlugin (returnPlugin, 0, nullptr);
    }
    else
    {
        edit->deleteTrack (track);
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the aux return.").toStdString());
    }

    return track;
}

std::vector<FxTrackInfo> SendService::getFxTracks() const
{
    std::vector<FxTrackInfo> fxTracks;

    for (auto* track : trackService.getAudioTracks())
        if (static_cast<bool> (track->state.getProperty (trackProperties::fx, false)))
            fxTracks.push_back ({ track->itemID.toString(),
                                  track->getName(),
                                  static_cast<int> (track->state.getProperty (trackProperties::fxBus, 0)) });

    return fxTracks;
}

std::vector<SendInfo> SendService::getSends (TrackHandle& track) const
{
    std::vector<SendInfo> sends;

    if (static_cast<bool> (track.state.getProperty (trackProperties::fx, false)))
        return sends; // FX-Spuren senden nicht an FX-Spuren (keine Rückkopplungen in M6)

    const int volumeIndex = indexOfVolumePlugin (track);

    for (const auto& fx : getFxTracks())
    {
        SendInfo info;
        info.fxTrackId = fx.id;
        info.fxName = fx.name;
        info.bus = fx.bus;

        if (auto* send = findSend (track, fx.bus))
        {
            info.exists = true;
            info.gainDb = send->getGainDb();
            info.preFader = track.state.indexOf (send->state) < volumeIndex;
        }

        sends.push_back (info);
    }

    return sends;
}

core::Result<void> SendService::setSendLevel (TrackHandle& track, const juce::String& fxTrackId, float gainDb)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto* fxTrack = trackService.findTrack (fxTrackId);

    if (fxTrack == nullptr || ! static_cast<bool> (fxTrack->state.getProperty (trackProperties::fx, false)))
        return core::fail (core::ErrorCode::notFound, TRANS ("FX track not found.").toStdString());

    const int bus = static_cast<int> (fxTrack->state.getProperty (trackProperties::fxBus, 0));
    auto* send = findSend (track, bus);

    projectService.beginUndoStep (TRANS ("Send level"));

    if (send == nullptr)
    {
        // Neuer Send: post Fader, also direkt hinter Lautstärke/Pan (Index in der Plugin-Liste).
        const int index = track.pluginList.indexOf (track.getVolumePlugin()) + 1;
        auto plugin = track.pluginList.insertPlugin (te::AuxSendPlugin::create(), index);
        send = dynamic_cast<te::AuxSendPlugin*> (plugin.get());

        if (send == nullptr)
            return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the send.").toStdString());

        send->busNumber = bus;
    }

    send->setGainDb (juce::jlimit (silenceDb, 6.0f, gainDb));
    return {};
}

core::Result<void> SendService::setSendPreFader (TrackHandle& track, const juce::String& fxTrackId, bool preFader)
{
    auto* fxTrack = trackService.findTrack (fxTrackId);

    if (fxTrack == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("FX track not found.").toStdString());

    auto* send = findSend (track, static_cast<int> (fxTrack->state.getProperty (trackProperties::fxBus, 0)));

    if (send == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("There is no send to this FX track.").toStdString());

    const int currentIndex = track.state.indexOf (send->state);
    const int volumeIndex = indexOfVolumePlugin (track);
    const bool isPre = currentIndex < volumeIndex;

    if (isPre == preFader)
        return {};

    // Pre = Knoten vor Lautstärke/Pan, Post = dahinter. Beim Verschieben nach hinten rückt der
    // Lautstärke-Knoten um eins nach vorn, deshalb dort das Ziel = alter Lautstärke-Index.
    projectService.beginUndoStep (preFader ? TRANS ("Send pre-fader") : TRANS ("Send post-fader"));
    const int targetIndex = preFader ? volumeIndex : volumeIndex;
    track.state.moveChild (currentIndex, targetIndex, &projectService.getEdit()->getUndoManager());
    return {};
}

core::Result<void> SendService::removeSend (TrackHandle& track, const juce::String& fxTrackId)
{
    auto* fxTrack = trackService.findTrack (fxTrackId);

    if (fxTrack == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("FX track not found.").toStdString());

    auto* send = findSend (track, static_cast<int> (fxTrack->state.getProperty (trackProperties::fxBus, 0)));

    if (send == nullptr)
        return {};

    projectService.beginUndoStep (TRANS ("Remove send"));
    send->deleteFromParent();
    return {};
}

void SendService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void SendService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void SendService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
    }

    cancelPendingUpdate();
    handleAsyncUpdate();
}

void SendService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.sendsChanged(); });
}

void SendService::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::PLUGIN) || child.hasType (te::IDs::TRACK))
        triggerAsyncUpdate();
}

void SendService::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::PLUGIN) || child.hasType (te::IDs::TRACK))
        triggerAsyncUpdate();
}

void SendService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.hasType (te::IDs::PLUGIN) || tree.hasType (te::IDs::TRACK))
        triggerAsyncUpdate();
}

void SendService::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (te::IDs::PLUGININSTANCE) || parent.hasType (te::IDs::TRACK))
        triggerAsyncUpdate();
}

te::AuxSendPlugin* SendService::findSend (TrackHandle& track, int bus) const
{
    for (auto* send : track.pluginList.getPluginsOfType<te::AuxSendPlugin>())
        if (send->getBusNumber() == bus)
            return send;

    return nullptr;
}

int SendService::indexOfVolumePlugin (TrackHandle& track) const
{
    // Index im Spur-Zustand (track.state), nicht in der Plugin-Liste: Der Zustand enthält
    // außer Plugins auch andere Knoten, und nur dort wird umsortiert.
    if (auto* volume = track.getVolumePlugin())
        return track.state.indexOf (volume->state);

    return track.state.getNumChildren();
}

int SendService::nextFreeBus() const
{
    int bus = 0;

    for (const auto& fx : getFxTracks())
        bus = juce::jmax (bus, fx.bus + 1);

    return bus;
}

} // namespace fk::services
