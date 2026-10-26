#include "fk/services/RackService.h"

#include "fk/services/MidiChannelRouting.h"
#include "fk/services/MidiService.h"
#include "fk/services/TrackProperties.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

namespace
{
constexpr auto roleMidi = "midi";
constexpr auto roleOutput = "output";
constexpr int noPin = 0; // Rack-Instanz: Kanal hängt an keinem Pin (Pins zählen ab 1)

juce::String pairName (int outputPair)
{
    return juce::String (outputPair * 2 + 1) + "+" + juce::String (outputPair * 2 + 2);
}
} // namespace

RackService::RackService (ProjectService& project, TrackService& tracks)
    : projectService (project)
    , trackService (tracks)
{
}

te::RackType::Ptr RackService::findRack (const juce::String& rackId) const
{
    if (const auto* edit = projectService.getEdit())
        return edit->getRackList().getRackTypeForID (te::EditItemID::fromVar (rackId));

    return {};
}

int RackService::numOutputPairs (te::RackType& rack)
{
    return std::max (1, (rack.getOutputNames().size() - 1) / 2); // Pin 0 ist MIDI
}

core::Result<te::AudioTrack*> RackService::createRackInstrument (const juce::String& instrumentPluginId,
                                                                 const juce::String& name)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto description =
        edit->engine.getPluginManager().knownPluginList.getTypeForIdentifierString (instrumentPluginId);

    if (description == nullptr || ! description->isInstrument)
        return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("Instrument not found: {id}"), { { "id", instrumentPluginId } }).toStdString());

    projectService.beginUndoStep (TRANS ("Add rack instrument"));

    auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::create (edit->engine, *description));

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::unknown,
                           plugins::i18n::format (TRANS ("Could not load the instrument: {name}"), { { "name", description->name } }).toStdString());

    auto rack = edit->getRackList().addNewRack();
    rack->rackName = name.isNotEmpty() ? name : description->name;

    // Tracktions Vorgabe: Pin 0 ist MIDI, dazu zwei Audio-Ein- und -Ausgänge. MIDI bleibt; das Rack
    // bekommt genau die Ausgänge des Instruments und keinen Audio-Eingang.
    while (rack->getInputNames().size() > 1)
        rack->removeInput (rack->getInputNames().size() - 1);

    while (rack->getOutputNames().size() > 1)
        rack->removeOutput (rack->getOutputNames().size() - 1);

    rack->addPlugin (plugin, { 0.5f, 0.5f }, false);

    juce::StringArray outputNames;
    plugin->getChannelNames (nullptr, &outputNames);

    for (int channel = 0; channel < outputNames.size(); ++channel)
    {
        rack->addOutput (-1, plugins::i18n::format (TRANS ("Out {channel}"), { { "channel", juce::String (channel + 1) } }));
        rack->addConnection (plugin->itemID, channel + 1, {}, channel + 1);
    }

    rack->addConnection ({}, 0, plugin->itemID, 0); // MIDI hinein

    auto midiTrack = appendMidiTrack (*rack, 1);

    if (! midiTrack)
        return midiTrack;

    if (const auto output = appendOutputTrack (*rack, 0); ! output)
        return std::unexpected (output.error());

    return midiTrack;
}

core::Result<te::AudioTrack*> RackService::addMidiTrack (const juce::String& rackId, int midiChannel)
{
    auto rack = findRack (rackId);

    if (rack == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Rack instrument not found.").toStdString());

    if (midiChannel == 0)
    {
        // Nächster Kanal, den noch keine MIDI-Spur dieses Racks nutzt.
        std::vector<bool> used (17, false);

        for (auto* track : trackService.getAudioTracks())
            if (const auto role = getRole (*track); role && role->rackId == rackId && role->isMidi)
                used[static_cast<std::size_t> (role->midiChannel)] = true;

        midiChannel = 1;

        while (midiChannel < 16 && used[static_cast<std::size_t> (midiChannel)])
            ++midiChannel;
    }

    projectService.beginUndoStep (TRANS ("MIDI track to rack"));
    return appendMidiTrack (*rack, midiChannel);
}

core::Result<te::AudioTrack*> RackService::appendMidiTrack (te::RackType& rack, int midiChannel)
{
    if (midiChannel < 1 || midiChannel > 16)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The MIDI channel must be between 1 and 16.").toStdString());

    auto* edit = projectService.getEdit();
    auto created = trackService.appendAudioTrack (
        plugins::i18n::format (TRANS ("{rack} channel {channel}"), { { "rack", rack.rackName }, { "channel", juce::String (midiChannel) } }), 2);

    if (! created)
        return created;

    auto* track = *created;
    auto* undo = &edit->getUndoManager();

    midiChannelRouting::insertPatchBay (*edit, *track, midiChannel);

    auto instancePlugin = edit->getPluginCache().createNewPlugin (te::RackInstance::create (rack));
    track->pluginList.insertPlugin (instancePlugin, 1, nullptr);

    // Die MIDI-Spur speist nur ein: kein Audio hinein, nichts zurück (das kommt über die Ausgangskanäle).
    if (auto* instance = dynamic_cast<te::RackInstance*> (instancePlugin.get()))
        for (int channel = 0; channel < instance->getNumChannelMappings(); ++channel)
        {
            instance->setInputMapping (channel, noPin);
            instance->setOutputMapping (channel, noPin);
        }

    track->state.setProperty (trackProperties::rack, rack.itemID.toString(), undo);
    track->state.setProperty (trackProperties::rackRole, roleMidi, undo);
    track->state.setProperty (trackProperties::rackChannel, midiChannel, undo);

    // Wie eine Instrumentenspur: hört auf alle MIDI-Eingänge.
    for (const auto& input : trackService.getAvailableMidiInputs())
        if (input.id == MidiService::getAllInputsId())
            (void) trackService.setAudioInput (*track, input.id);

    return track;
}

core::Result<te::AudioTrack*> RackService::appendOutputTrack (te::RackType& rack, int outputPair)
{
    auto* edit = projectService.getEdit();
    auto created = trackService.appendAudioTrack (rack.rackName + " " + pairName (outputPair), 2);

    if (! created)
        return created;

    auto* track = *created;
    auto* undo = &edit->getUndoManager();

    auto instancePlugin = edit->getPluginCache().createNewPlugin (te::RackInstance::create (rack));
    track->pluginList.insertPlugin (instancePlugin, 0, nullptr);

    if (auto* instance = dynamic_cast<te::RackInstance*> (instancePlugin.get()))
    {
        instance->setNumInputChannels (2);
        instance->setNumOutputChannels (2);

        for (int channel = 0; channel < instance->getNumChannelMappings(); ++channel)
        {
            instance->setInputMapping (channel, noPin);
            instance->setOutputMapping (channel, outputPair * 2 + channel + 1);
        }

        instance->dryValue = 0.0f; // nur das Rack, kein Durchgang der (leeren) Spur
    }

    track->state.setProperty (trackProperties::rack, rack.itemID.toString(), undo);
    track->state.setProperty (trackProperties::rackRole, roleOutput, undo);
    track->state.setProperty (trackProperties::rackOutput, outputPair, undo);
    return track;
}

core::Result<void> RackService::setMidiChannel (te::AudioTrack& track, int midiChannel)
{
    const auto role = getRole (track);

    if (! role || ! role->isMidi)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The track is not a MIDI track on a rack.").toStdString());

    if (midiChannel < 1 || midiChannel > 16)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The MIDI channel must be between 1 and 16.").toStdString());

    auto* patchBay = midiChannelRouting::findPatchBay (track);

    if (patchBay == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The MIDI track no longer has a channel assignment.").toStdString());

    projectService.beginUndoStep (TRANS ("MIDI channel"));
    midiChannelRouting::routeAllTo (*patchBay, midiChannel);
    track.state.setProperty (trackProperties::rackChannel, midiChannel, &projectService.getEdit()->getUndoManager());
    return {};
}

core::Result<void> RackService::setOutputActive (const juce::String& rackId, int outputPair, bool active)
{
    auto rack = findRack (rackId);

    if (rack == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Rack instrument not found.").toStdString());

    if (outputPair < 0 || outputPair >= numOutputPairs (*rack))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The instrument does not have this output.").toStdString());

    auto* existing = findOutputTrack (rackId, outputPair);

    if (active == (existing != nullptr))
        return {};

    if (active)
    {
        projectService.beginUndoStep (TRANS ("Enable output"));

        if (const auto created = appendOutputTrack (*rack, outputPair); ! created)
            return std::unexpected (created.error());

        return {};
    }

    trackService.deleteTrack (*existing);
    return {};
}

te::AudioTrack* RackService::findOutputTrack (const juce::String& rackId, int outputPair) const
{
    for (auto* track : trackService.getAudioTracks())
        if (const auto role = getRole (*track); role && role->rackId == rackId && ! role->isMidi &&
                                                role->outputPair == outputPair)
            return track;

    return nullptr;
}

std::optional<RackRole> RackService::getRole (te::AudioTrack& track) const
{
    const auto rackId = track.state.getProperty (trackProperties::rack).toString();

    if (rackId.isEmpty())
        return std::nullopt;

    RackRole role;
    role.rackId = rackId;
    role.isMidi = track.state.getProperty (trackProperties::rackRole).toString() == roleMidi;
    role.midiChannel = track.state.getProperty (trackProperties::rackChannel, 1);
    role.outputPair = track.state.getProperty (trackProperties::rackOutput, 0);
    return role;
}

std::optional<RackInfo> RackService::getRack (const juce::String& rackId) const
{
    for (auto& rack : getRacks())
        if (rack.id == rackId)
            return rack;

    return std::nullopt;
}

std::vector<RackInfo> RackService::getRacks() const
{
    std::vector<RackInfo> racks;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return racks;

    for (auto* rack : edit->getRackList().getTypes())
    {
        RackInfo info;
        info.id = rack->itemID.toString();
        info.name = rack->rackName;
        info.numOutputPairs = numOutputPairs (*rack);
        info.activeOutputs.assign (static_cast<std::size_t> (info.numOutputPairs), false);

        for (int pair = 0; pair < info.numOutputPairs; ++pair)
            info.outputNames.push_back (pairName (pair));

        for (auto* plugin : rack->getPlugins())
            if (plugin->isSynth())
                info.instrumentId = plugin->itemID.toString();

        for (auto* track : trackService.getAudioTracks())
            if (const auto role = getRole (*track); role && role->rackId == info.id)
            {
                if (role->isMidi)
                    ++info.numMidiTracks;
                else if (role->outputPair < info.numOutputPairs)
                    info.activeOutputs[static_cast<std::size_t> (role->outputPair)] = true;
            }

        racks.push_back (std::move (info));
    }

    return racks;
}

} // namespace fk::services
