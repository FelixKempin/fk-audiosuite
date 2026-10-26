#include "fk/services/ExternalMidiService.h"

#include "fk/services/MidiChannelRouting.h"
#include "fk/services/MidiService.h"
#include "fk/services/TrackProperties.h"

namespace fk::services
{

ExternalMidiService::ExternalMidiService (ProjectService& project, TrackService& tracks, TransportService& transportService)
    : projectService (project)
    , trackService (tracks)
    , transport (transportService)
{
    transport.addListener (*this);
}

ExternalMidiService::~ExternalMidiService()
{
    transport.removeListener (*this);
}

core::Result<te::AudioTrack*> ExternalMidiService::createMidiTrack (const juce::String& name)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Add MIDI track"));
    auto created = trackService.appendAudioTrack (name, 2);

    if (! created)
        return created;

    auto* track = *created;
    auto* undo = &edit->getUndoManager();
    track->state.setProperty (trackProperties::externalMidi, true, undo);
    track->state.setProperty (trackProperties::rackChannel, 1, undo);
    midiChannelRouting::insertPatchBay (*edit, *track, 1);

    // Ausgang: der erste MIDI-Ausgang, ohne Gerät bleibt die Spur am Master (stumm, bis man wählt).
    for (const auto& output : trackService.getAvailableOutputs (*track))
        if (output.id.startsWith ("device:"))
        {
            (void) trackService.setOutput (*track, output.id);
            break;
        }

    for (const auto& input : trackService.getAvailableMidiInputs())
        if (input.id == MidiService::getAllInputsId())
            (void) trackService.setAudioInput (*track, input.id);

    return track;
}

std::optional<ExternalMidiSettings> ExternalMidiService::getSettings (te::AudioTrack& track) const
{
    if (! static_cast<bool> (track.state.getProperty (trackProperties::externalMidi, false)))
        return std::nullopt;

    return ExternalMidiSettings { track.state.getProperty (trackProperties::rackChannel, 1),
                                  track.state.getProperty (trackProperties::midiProgram, -1),
                                  track.state.getProperty (trackProperties::midiBank, -1) };
}

core::Result<void> ExternalMidiService::setChannel (te::AudioTrack& track, int channel)
{
    if (! getSettings (track))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The track is not a MIDI track on a device.").toStdString());

    if (channel < 1 || channel > 16)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The MIDI channel must be between 1 and 16.").toStdString());

    auto* patchBay = midiChannelRouting::findPatchBay (track);

    if (patchBay == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The MIDI track no longer has a channel assignment.").toStdString());

    projectService.beginUndoStep (TRANS ("MIDI channel"));
    midiChannelRouting::routeAllTo (*patchBay, channel);
    track.state.setProperty (trackProperties::rackChannel, channel, &projectService.getEdit()->getUndoManager());
    sendProgram (track); // Programm gilt je Kanal: auf dem neuen Kanal neu setzen
    return {};
}

core::Result<void> ExternalMidiService::setProgram (te::AudioTrack& track, int program, int bank)
{
    if (! getSettings (track))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The track is not a MIDI track on a device.").toStdString());

    if (program < -1 || program > 127 || bank < -1 || bank > 16383)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Program 1-128, bank 0-16383.").toStdString());

    projectService.beginUndoStep (TRANS ("Program change"));
    auto* undo = &projectService.getEdit()->getUndoManager();
    track.state.setProperty (trackProperties::midiProgram, program, undo);
    track.state.setProperty (trackProperties::midiBank, bank, undo);
    sendProgram (track);
    return {};
}

std::vector<juce::MidiMessage> ExternalMidiService::programMessages (const ExternalMidiSettings& settings)
{
    std::vector<juce::MidiMessage> messages;

    if (settings.program < 0)
        return messages;

    if (settings.bank >= 0)
    {
        messages.push_back (juce::MidiMessage::controllerEvent (settings.channel, 0, settings.bank / 128));
        messages.push_back (juce::MidiMessage::controllerEvent (settings.channel, 32, settings.bank % 128));
    }

    messages.push_back (juce::MidiMessage::programChange (settings.channel, settings.program));
    return messages;
}

void ExternalMidiService::sendProgram (te::AudioTrack& track)
{
    const auto settings = getSettings (track);

    if (! settings)
        return;

    auto* edit = projectService.getEdit();
    auto* device = edit != nullptr ? dynamic_cast<te::MidiOutputDevice*> (track.getOutput().getOutputDevice (false))
                                   : nullptr;

    for (const auto& message : programMessages (*settings))
    {
        ++messagesSent;

        if (device != nullptr && device->isEnabled())
            device->fireMessage (message);
    }
}

void ExternalMidiService::sendAllPrograms()
{
    for (auto* track : trackService.getAudioTracks())
        sendProgram (*track);
}

void ExternalMidiService::transportStateChanged()
{
    const bool playing = transport.isPlaying();

    if (playing && ! wasPlaying)
        sendAllPrograms();

    wasPlaying = playing;
}

} // namespace fk::services
