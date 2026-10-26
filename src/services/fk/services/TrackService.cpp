#include "fk/services/TrackService.h"

#include "fk/engine/EngineContext.h"
#include "fk/services/BakeLock.h"
#include "fk/services/InstrumentOutputs.h"
#include "fk/services/MidiService.h"
#include "fk/services/TrackColours.h"
#include "fk/services/TrackProperties.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace fk::services
{

namespace
{
// Eigene Eigenschaften im Spur-Zustand. Tracktion ignoriert unbekannte Properties und speichert sie mit.
const juce::Identifier channelsProperty ("fkChannels");
const juce::Identifier groupProperty ("fkGroup");
const juce::Identifier collapsedProperty ("fkCollapsed"); // Ordner zugeklappt (Ansicht, ohne Undo)
constexpr auto masterOutputId = "master";
constexpr auto deviceOutputPrefix = "device:";

/// Name eines Audio-Anschlusses wie auf dem Interface: "Eingang 1", "Ausgang 3+4", ab 1 gezählt.
/// Tracktion übernimmt sonst die Kanalnamen des Treibers, je nach System "channel 1" (ALSA) oder
/// "capture_AUX 0" (JACK/PipeWire).
juce::String channelName (const te::ChannelConfiguration& channels, bool isInput)
{
    juce::StringArray numbers;

    for (const auto& channel : channels)
        numbers.add (juce::String (channel.indexInDevice + 1));

    return plugins::i18n::format (isInput ? TRANS ("Input {channels}") : TRANS ("Output {channels}"),
                                  { { "channels", numbers.joinIntoString ("+") } });
}

/// Wie channelName, ein vom Nutzer vergebener Alias hat aber Vorrang.
juce::String displayName (const te::IODevice& device, const te::ChannelConfiguration& channels, bool isInput)
{
    const auto alias = device.getAliasIfSet();
    return alias.isNotEmpty() ? alias : channelName (channels, isInput);
}

/// Spur oder Ordnerspur im Projektbaum (beide ändern, welche Zeilen das Arrangement hat).
bool isTrackOrFolder (const juce::ValueTree& tree)
{
    return tree.hasType (te::IDs::TRACK) || tree.hasType (te::IDs::FOLDERTRACK);
}

/// Spuren und Ordner direkt in parent (nullptr = oberste Ebene), ohne Tracktions feste Spuren.
std::vector<te::Track*> organisableChildren (te::Edit& edit, te::Track* parent)
{
    std::vector<te::Track*> children;
    const auto candidates = parent != nullptr ? parent->getAllSubTracks (false) : te::getTopLevelTracks (edit);

    for (auto* track : candidates)
        if (dynamic_cast<te::AudioTrack*> (track) != nullptr || dynamic_cast<te::FolderTrack*> (track) != nullptr)
            children.push_back (track);

    return children;
}
} // namespace

TrackService::TrackService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

TrackService::~TrackService()
{
    if (auto* edit = projectService.getEdit())
        edit->engine.getDeviceManager().removeChangeListener (this);

    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

void TrackService::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Geräteliste der Engine hat sich geändert (Ein-/Ausstecken, An-/Abschalten): Die Spurköpfe
    // lesen ihre Eingangslisten über contentChanged neu.
    markContentChanged();
}

core::Result<te::AudioTrack*> TrackService::createAudioTrack (const juce::String& name, int numChannels)
{
    projectService.beginUndoStep (TRANS ("Add track"));
    return appendAudioTrack (name, numChannels);
}

core::Result<te::AudioTrack*> TrackService::appendAudioTrack (const juce::String& name, int numChannels)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    // Tracktion hängt die neue Spur hinter die vorhandenen Audiospuren und hält seine festen
    // Spuren (Tempo, Marker, Master) an ihrem Platz. Ein eigener TrackInsertPoint hinter
    // "der letzten Spur überhaupt" ließ Tracktion still umsortieren, und Undo kam durcheinander.
    const auto numAudioTracks = te::getAudioTracks (*edit).size();
    edit->ensureNumberOfAudioTracks (numAudioTracks + 1);
    auto* track = te::getAudioTracks (*edit).getLast();

    if (track == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the track.").toStdString());

    if (name.isNotEmpty())
        track->setName (name);

    track->state.setProperty (channelsProperty, numChannels == 1 ? 1 : 2, &edit->getUndoManager());
    track->setColour (paletteColour (static_cast<int> (numAudioTracks))); // feste Farbe, wandert beim Umsortieren mit

    return track;
}

core::Result<te::AudioTrack*> TrackService::createGroupTrack (const juce::String& name)
{
    auto created = createAudioTrack (name, 2);

    if (! created)
        return created;

    (*created)->state.setProperty (groupProperty, true, &projectService.getEdit()->getUndoManager());
    return created;
}

juce::String TrackService::getTrackId (te::AudioTrack& track) const
{
    return track.itemID.toString();
}

te::AudioTrack* TrackService::findTrack (const juce::String& trackId) const
{
    if (auto* edit = projectService.getEdit())
        for (auto* track : te::getAudioTracks (*edit))
            if (track->itemID.toString() == trackId)
                return track;

    return nullptr;
}

std::vector<AudioInputInfo> TrackService::getAvailableAudioInputs (te::AudioTrack& track) const
{
    const auto wanted = getNumChannels (track);
    std::vector<AudioInputInfo> matching;

    for (const auto& input : getAvailableAudioInputs())
        if (input.numChannels == wanted)
            matching.push_back (input);

    return matching;
}

std::vector<OutputInfo> TrackService::getAvailableOutputs (te::AudioTrack& track) const
{
    // MIDI-Spur an einem Gerät: die MIDI-Ausgänge statt Master und Gruppen.
    if (getKind (track) == TrackKind::midi)
    {
        std::vector<OutputInfo> midiOutputs;

        if (auto* edit = projectService.getEdit())
        {
            auto& devices = edit->engine.getDeviceManager();

            for (int i = 0; i < devices.getNumMidiOutDevices(); ++i)
                if (auto* device = devices.getMidiOutDevice (i); device != nullptr && device->isEnabled())
                    midiOutputs.push_back ({ deviceOutputPrefix + device->getDeviceID(), device->getAliasOrName() });
        }

        return midiOutputs;
    }

    std::vector<OutputInfo> outputs { { masterOutputId, TRANS ("Master") } };

    if (auto* edit = projectService.getEdit())
        for (auto* candidate : te::getAudioTracks (*edit))
            if (candidate != &track && isGroupTrack (*candidate) && ! candidate->getOutput().feedsInto (&track))
                outputs.push_back ({ candidate->itemID.toString(), candidate->getName() });

    // Das Standard-Paar ist der Master-Ausgang; die übrigen Paare sind Direct Outs.
    if (auto* edit = projectService.getEdit())
    {
        auto& devices = edit->engine.getDeviceManager();

        for (auto* device : devices.getWaveOutputDevices())
            if (device->isEnabled() && device->getDeviceID() != devices.getDefaultWaveOutDeviceID())
                outputs.push_back ({ deviceOutputPrefix + device->getDeviceID(),
                                     displayName (*device, device->getChannels(), false) });
    }

    return outputs;
}

core::Result<void> TrackService::setOutput (te::AudioTrack& track, const juce::String& outputId)
{
    if (projectService.getEdit() == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Assign output"));

    if (outputId == masterOutputId || outputId.isEmpty())
    {
        track.getOutput().setOutputToDefaultDevice (false);
        return {};
    }

    if (outputId.startsWith (deviceOutputPrefix))
    {
        const auto deviceId = outputId.fromFirstOccurrenceOf (deviceOutputPrefix, false, false);
        auto* device = projectService.getEdit()->engine.getDeviceManager().findOutputDeviceForID (deviceId);

        // MIDI-Spuren gehen an MIDI-Ausgänge, alle anderen an Audio-Ausgänge.
        if (device == nullptr || device->isMidi() != (getKind (track) == TrackKind::midi) || ! device->isEnabled())
            return core::fail (core::ErrorCode::notFound, TRANS ("Output not found.").toStdString());

        track.getOutput().setOutputToDeviceID (deviceId);
        return {};
    }

    auto* destination = findTrack (outputId);

    if (destination == nullptr || ! isGroupTrack (*destination))
        return core::fail (core::ErrorCode::notFound, TRANS ("Group track not found.").toStdString());

    // Kreise verhindern: Die Gruppe darf nicht (auch über Zwischengruppen) in diese Spur münden.
    if (destination == &track || destination->getOutput().feedsInto (&track))
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("Routing loop: {name} already feeds into this track."),
                                                  { { "name", destination->getName() } })
                               .toStdString());

    track.getOutput().setOutputToTrack (destination);
    return {};
}

juce::String TrackService::getOutputId (te::AudioTrack& track) const
{
    if (auto* destination = track.getOutput().getDestinationTrack())
        return destination->itemID.toString();

    if (auto* edit = projectService.getEdit(); edit != nullptr && ! track.getOutput().usesDefaultAudioOut())
        if (auto* device = track.getOutput().getOutputDevice (false);
            device != nullptr && device->getDeviceID() != edit->engine.getDeviceManager().getDefaultWaveOutDeviceID())
            return deviceOutputPrefix + device->getDeviceID();

    return masterOutputId;
}

std::vector<StudioPort> TrackService::getStudioPorts() const
{
    std::vector<StudioPort> ports;
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return ports;

    auto& devices = edit->engine.getDeviceManager();

    for (auto* device : devices.getWaveInputDevices())
        ports.push_back ({ device->getDeviceID(),
                           channelName (device->getChannels(), true),
                           device->getAliasIfSet(),
                           true,
                           static_cast<int> (device->getChannels().getNumChannels()) });

    for (auto* device : devices.getWaveOutputDevices())
        ports.push_back ({ device->getDeviceID(),
                           channelName (device->getChannels(), false),
                           device->getAliasIfSet(),
                           false,
                           static_cast<int> (device->getChannels().getNumChannels()) });

    return ports;
}

core::Result<void> TrackService::setPortAlias (const juce::String& portId, const juce::String& alias)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto& devices = edit->engine.getDeviceManager();

    for (auto* device : devices.getWaveInputDevices())
        if (device->getDeviceID() == portId)
        {
            device->setAlias (alias);
            markContentChanged();
            return {};
        }

    for (auto* device : devices.getWaveOutputDevices())
        if (device->getDeviceID() == portId)
        {
            device->setAlias (alias);
            markContentChanged();
            return {};
        }

    return core::fail (core::ErrorCode::notFound, TRANS ("Connection not found.").toStdString());
}

bool TrackService::isGroupTrack (te::AudioTrack& track)
{
    return static_cast<bool> (track.state.getProperty (groupProperty, false));
}

int TrackService::getNumChannels (te::AudioTrack& track)
{
    return static_cast<int> (track.state.getProperty (channelsProperty, 2)) == 1 ? 1 : 2;
}

TrackKind TrackService::getKind (te::AudioTrack& track) const
{
    if (isGroupTrack (track))
        return TrackKind::group;

    if (static_cast<bool> (track.state.getProperty (trackProperties::fx, false)))
        return TrackKind::fx;

    if (track.state.getProperty (trackProperties::rackRole).toString() == "output")
        return TrackKind::rackOutput;

    if (static_cast<bool> (track.state.getProperty (trackProperties::externalMidi, false)))
        return TrackKind::midi;

    return hasInstrument (track) ? TrackKind::instrument : TrackKind::audio;
}

core::Result<te::AudioTrack*> TrackService::createInstrumentTrack (const juce::String& name,
                                                                   const juce::String& instrumentPluginId)
{
    auto created = createAudioTrack (name);

    if (! created)
        return created;

    auto* edit = projectService.getEdit();
    const auto description =
        edit->engine.getPluginManager().knownPluginList.getTypeForIdentifierString (instrumentPluginId);

    if (description == nullptr || ! description->isInstrument)
    {
        edit->deleteTrack (*created);
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("Instrument not found: {id}"), { { "id", instrumentPluginId } }).toStdString());
    }

    auto plugin = (*created)->pluginList.insertPlugin (te::ExternalPlugin::create (edit->engine, *description), 0);

    if (plugin == nullptr)
    {
        edit->deleteTrack (*created);
        return core::fail (core::ErrorCode::unknown,
                           plugins::i18n::format (TRANS ("Could not load the instrument: {name}"), { { "name", description->name } }).toStdString());
    }

    instrumentOutputs::useMainOutputOnly (*plugin);

    // Wie Cubase: Eine neue Instrumentenspur hört auf alle MIDI-Eingänge. Fehlt das Sammelgerät
    // (abgeschaltet), bleibt die Spur ohne Eingang; der Nutzer wählt dann im Spurkopf.
    for (const auto& input : getAvailableMidiInputs())
        if (input.id == MidiService::getAllInputsId())
            if (const auto assigned = setAudioInput (**created, input.id); ! assigned)
                juce::Logger::writeToLog ("Instrumentenspur ohne MIDI-Eingang: " + assigned.error().message);

    return created;
}

void TrackService::deleteTrack (te::AudioTrack& track)
{
    projectService.beginUndoStep (TRANS ("Delete track"));
    if (auto* edit = projectService.getEdit())
        edit->deleteTrack (&track);
}

std::vector<te::AudioTrack*> TrackService::getAudioTracks() const
{
    std::vector<te::AudioTrack*> result;

    if (auto* edit = projectService.getEdit())
        for (auto* track : te::getAudioTracks (*edit))
            result.push_back (track);

    return result;
}

TrackInfo TrackService::getTrackInfo (te::AudioTrack& track) const
{
    TrackInfo info;
    info.name = track.getName();
    info.colour = track.getColour();
    info.hidden = track.isHidden();

    // Projekte vor 2026-10-12 haben keine Spurfarbe gespeichert: Palettenfarbe nach Position wie bisher.
    if (info.colour.isTransparent())
        if (const auto* edit = projectService.getEdit())
            info.colour = paletteColour (te::getAudioTracks (*edit).indexOf (&track));
    info.kind = getKind (track);
    info.numChannels = getNumChannels (track);
    info.inputId = getAudioInputId (track);
    info.outputId = getOutputId (track);
    info.isInstrument = info.kind == TrackKind::instrument;

    for (auto plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            info.instrumentName = plugin->getName();

    info.muted = track.isMuted (false);
    info.solo = track.isSolo (false);
    info.armed = isArmed (track);
    info.baked = bakeLock::isBaked (track);

    if (auto* group = bakeLock::findBakedDestination (track))
        info.bakedIntoGroup = group->getName();

    if (auto* destination = track.getOutput().getDestinationTrack())
        info.outputName = destination->getName();
    else if (const auto outputId = getOutputId (track); outputId.startsWith (deviceOutputPrefix))
        info.outputName = track.getOutput().getOutputDevice (false)->getAliasOrName();
    else
        info.outputName = TRANS ("Master");

    info.inRack = track.state.getProperty (trackProperties::rack).toString().isNotEmpty();

    return info;
}

std::vector<ClipInfo> TrackService::getClips (te::AudioTrack& track) const
{
    std::vector<ClipInfo> clips;

    for (auto* clip : track.getClips())
    {
        // Tracktion liefert die Basisklasse; wir unterscheiden Audio und MIDI per dynamic_cast
        // (legitim: eine Verzweigung nach Untertyp an genau einer Stelle).
        const auto position = clip->getPosition();
        ClipInfo info;
        info.id = clip->itemID.toString();
        info.name = clip->getName();
        info.startSeconds = position.getStart().inSeconds();
        info.lengthSeconds = position.getLength().inSeconds();
        info.offsetSeconds = position.getOffset().inSeconds();
        info.muted = clip->isMuted();

        if (auto* audioClip = dynamic_cast<te::WaveAudioClip*> (clip))
        {
            info.audioFile = audioClip->getOriginalFile();
            info.fadeInSeconds = audioClip->getFadeIn().inSeconds();
            info.fadeOutSeconds = audioClip->getFadeOut().inSeconds();
            info.gainDb = audioClip->getGainDB();
            info.reversed = audioClip->getIsReversed();
            info.followsTempo = audioClip->getAutoTempo();
            info.speedRatio = audioClip->getSpeedRatio();
            info.pitchSemitones = audioClip->getPitchChange();

            if (const auto waveInfo = audioClip->getWaveInfo();
                waveInfo.sampleRate > 0.0 && audioClip->getLoopInfo().getNumBeats() > 0.0)
                info.sourceBpm = audioClip->getLoopInfo().getBpm (waveInfo);
        }
        else if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        {
            info.isMidi = true;

            for (auto* note : midiClip->getSequence().getNotes())
            {
                const auto start = midiClip->getTimeOfContentBeat (note->getStartBeat());
                const auto end = midiClip->getTimeOfContentBeat (note->getEndBeat());
                info.notes.push_back (
                    { note->getNoteNumber(), start.inSeconds(), (end - start).inSeconds(), note->getVelocity() });
            }
        }
        else
        {
            continue; // andere Clip-Arten (Step, Marker ...) zeigen wir noch nicht
        }

        // Takes (Loop-Aufnahme): Zahl, aktueller, Namen ohne Tracktions "1. "-Präfix.
        info.numTakes = clip->getNumTakes (true);
        info.currentTake = clip->getCurrentTake();
        info.showingTakes = clip->isShowingTakes();

        for (const auto& description : clip->getTakeDescriptions())
            info.takeNames.push_back (description.fromFirstOccurrenceOf (". ", false, false));

        clips.push_back (std::move (info));
    }

    return clips;
}

void TrackService::setTrackName (te::AudioTrack& track, const juce::String& name)
{
    projectService.beginUndoStep (TRANS ("Rename track"));
    track.setName (name);
}

std::vector<te::AudioTrack*> TrackService::getVisibleTracks() const
{
    auto visible = getAudioTracks();
    std::erase_if (visible, [] (const te::AudioTrack* track) { return track->isHidden(); });
    return visible;
}

void TrackService::setTrackColour (te::AudioTrack& track, juce::Colour colour)
{
    projectService.beginUndoStep (TRANS ("Track color"));
    track.setColour (colour);
}

void TrackService::setHidden (te::AudioTrack& track, bool shouldBeHidden)
{
    projectService.beginUndoStep (shouldBeHidden ? TRANS ("Hide track") : TRANS ("Show track"));
    track.setHidden (shouldBeHidden);
}

core::Result<void> TrackService::moveTrack (te::AudioTrack& track, int newIndex)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto others = getAudioTracks();
    const auto oldPosition = std::ranges::find (others, &track);

    if (oldPosition == others.end())
        return core::fail (core::ErrorCode::notFound, TRANS ("Track not found.").toStdString());

    const auto oldIndex = static_cast<int> (std::distance (others.begin(), oldPosition));
    others.erase (oldPosition);
    newIndex = juce::jlimit (0, static_cast<int> (others.size()), newIndex);

    if (newIndex == oldIndex)
        return {};

    // Einfügepunkt relativ zu einer anderen Audiospur: So bleiben Tracktions feste Spuren
    // (Tempo, Marker, Master) vorn, egal wohin die Spur wandert.
    projectService.beginUndoStep (TRANS ("Move track"));

    if (others.empty())
        return {};

    if (newIndex == 0)
        edit->moveTrack (&track, te::TrackInsertPoint (*others.front(), true));
    else
        edit->moveTrack (&track, te::TrackInsertPoint (*others[static_cast<std::size_t> (newIndex - 1)], false));

    return {};
}

core::Result<FolderHandle*> TrackService::createFolderTrack (const juce::String& name)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Add folder track"));
    auto folder = edit->insertNewFolderTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr, false);

    if (folder == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the folder track.").toStdString());

    folder->setName (name.isNotEmpty() ? name : TRANS ("Folder"));
    folder->setColour (juce::Colour (0xff8a93a6)); // neutrales Grau: Ordner sollen Spuren nicht übertönen
    return folder.get();
}

std::vector<FolderHandle*> TrackService::getFolders() const
{
    std::vector<FolderHandle*> folders;

    if (auto* edit = projectService.getEdit())
        for (auto* track : te::getAllTracks (*edit))
            if (auto* folder = dynamic_cast<te::FolderTrack*> (track))
                folders.push_back (folder);

    return folders;
}

FolderInfo TrackService::getFolderInfo (FolderHandle& folder) const
{
    FolderInfo info;
    info.name = folder.getName();
    info.colour = folder.getColour();
    info.collapsed = folder.state.getProperty (collapsedProperty, false);
    info.muted = folder.isMuted (false);
    info.solo = folder.isSolo (false);

    if (auto* edit = projectService.getEdit())
        info.numTracks = static_cast<int> (organisableChildren (*edit, &folder).size());

    return info;
}

FolderHandle* TrackService::getParentFolder (te::Track& track) const
{
    return track.getParentFolderTrack();
}

std::vector<TrackHandle*> TrackService::getTracksInFolder (FolderHandle& folder) const
{
    std::vector<TrackHandle*> contained;

    for (auto* track : folder.getAllSubTracks (true))
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
            contained.push_back (audioTrack);

    return contained;
}

void TrackService::setFolderName (FolderHandle& folder, const juce::String& name)
{
    projectService.beginUndoStep (TRANS ("Rename folder"));
    folder.setName (name);
}

void TrackService::setFolderColour (FolderHandle& folder, juce::Colour colour)
{
    projectService.beginUndoStep (TRANS ("Folder color"));
    folder.setColour (colour);
}

void TrackService::setFolderMuted (FolderHandle& folder, bool shouldBeMuted)
{
    projectService.beginUndoStep (TRANS ("Mute folder"));
    folder.setMute (shouldBeMuted);
}

void TrackService::setFolderSolo (FolderHandle& folder, bool shouldBeSolo)
{
    projectService.beginUndoStep (TRANS ("Solo folder"));
    folder.setSolo (shouldBeSolo);
}

void TrackService::setFolderCollapsed (FolderHandle& folder, bool shouldBeCollapsed)
{
    folder.state.setProperty (collapsedProperty, shouldBeCollapsed, nullptr);
}

core::Result<void> TrackService::deleteFolder (FolderHandle& folder, bool keepContents)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (keepContents ? TRANS ("Dissolve folder") : TRANS ("Delete folder with contents"));

    // Inhalt in Reihenfolge vor den Ordner holen: jede Spur vor den Ordner, der danach weiterwandert.
    if (keepContents)
        for (auto* child : organisableChildren (*edit, &folder))
            edit->moveTrack (child, te::TrackInsertPoint (folder, true));

    edit->deleteTrack (&folder);
    return {};
}

std::vector<TrackListEntry> TrackService::getTrackList() const
{
    std::vector<TrackListEntry> entries;
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return entries;

    // Tiefensuche: Ordner vor ihrem Inhalt, zugeklappte ohne Inhalt.
    const std::function<void (te::Track*, int)> visit = [&] (te::Track* parent, int depth)
    {
        for (auto* track : organisableChildren (*edit, parent))
        {
            if (auto* folder = dynamic_cast<te::FolderTrack*> (track))
            {
                entries.push_back ({ nullptr, folder, depth });

                if (! folder->state.getProperty (collapsedProperty, false))
                    visit (folder, depth + 1);
            }
            else if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track); ! audioTrack->isHidden())
            {
                entries.push_back ({ audioTrack, nullptr, depth });
            }
        }
    };

    visit (nullptr, 0);
    return entries;
}

core::Result<void> TrackService::moveTrackBefore (te::Track& track, te::Track* before)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (before == &track)
        return {};

    // Einen Ordner nicht in sich selbst schieben.
    for (auto* parent = before != nullptr ? before->getParentFolderTrack() : nullptr; parent != nullptr;
         parent = parent->getParentFolderTrack())
        if (parent == &track)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("A folder cannot be placed inside itself.").toStdString());

    projectService.beginUndoStep (TRANS ("Move track"));

    if (before != nullptr)
    {
        edit->moveTrack (&track, te::TrackInsertPoint (*before, true));
        return {};
    }

    // Ans Ende der obersten Ebene: hinter das letzte andere Element dort.
    auto topLevel = organisableChildren (*edit, nullptr);
    std::erase (topLevel, &track);

    if (! topLevel.empty())
        edit->moveTrack (&track, te::TrackInsertPoint (*topLevel.back(), false));
    else
        edit->moveTrack (&track, te::TrackInsertPoint (nullptr, nullptr));

    return {};
}

core::Result<void> TrackService::moveIntoFolder (te::Track& track, FolderHandle* folder)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (folder == nullptr)
    {
        // Aus dem Ordner: direkt hinter den Ordner, in dem die Spur liegt.
        auto* parent = track.getParentFolderTrack();

        if (parent == nullptr)
            return {};

        projectService.beginUndoStep (TRANS ("Move out of folder"));
        edit->moveTrack (&track, te::TrackInsertPoint (*parent, false));
        return {};
    }

    for (te::Track* parent = folder; parent != nullptr; parent = parent->getParentFolderTrack())
        if (parent == &track)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("A folder cannot be placed inside itself.").toStdString());

    projectService.beginUndoStep (TRANS ("Move into folder"));
    auto children = organisableChildren (*edit, folder);
    std::erase (children, &track);
    edit->moveTrack (&track, te::TrackInsertPoint (folder, children.empty() ? nullptr : children.back()));
    return {};
}

core::Result<void> TrackService::moveWithinFolder (te::Track& track, int delta)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto siblings = organisableChildren (*edit, track.getParentFolderTrack());
    const auto position = std::ranges::find (siblings, &track);

    if (position == siblings.end())
        return core::fail (core::ErrorCode::notFound, TRANS ("Track not found.").toStdString());

    const auto index = std::distance (siblings.begin(), position) + delta;

    if (index < 0 || index >= static_cast<std::ptrdiff_t> (siblings.size()))
        return {};

    projectService.beginUndoStep (TRANS ("Move track"));
    edit->moveTrack (&track, te::TrackInsertPoint (*siblings[static_cast<std::size_t> (index)], delta < 0));
    return {};
}

core::Result<te::AudioTrack*> TrackService::duplicateTrack (te::AudioTrack& track)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (bakeLock::isBaked (track))
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Baked tracks cannot be duplicated. Please unbake the track first.").toStdString());

    projectService.beginUndoStep (TRANS ("Duplicate track"));
    auto copy = edit->copyTrack (&track, te::TrackInsertPoint (track, false));
    auto* audioCopy = dynamic_cast<te::AudioTrack*> (copy.get());

    if (audioCopy == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not copy the track.").toStdString());

    audioCopy->setName (plugins::i18n::format (TRANS ("{name} copy"), { { "name", track.getName() } }));
    return audioCopy;
}

void TrackService::setMuted (te::AudioTrack& track, bool shouldBeMuted)
{
    track.setMute (shouldBeMuted);
}

void TrackService::setSolo (te::AudioTrack& track, bool shouldBeSolo)
{
    track.setSolo (shouldBeSolo);
}

std::vector<AudioInputInfo> TrackService::getAvailableAudioInputs() const
{
    return getAvailableInputs (false);
}

std::vector<AudioInputInfo> TrackService::getAvailableMidiInputs() const
{
    return getAvailableInputs (true);
}

std::vector<AudioInputInfo> TrackService::getAvailableInputs (bool midi) const
{
    std::vector<AudioInputInfo> inputs;

    if (auto* edit = projectService.getEdit())
    {
        for (auto* instance : edit->getAllInputDevices())
        {
            auto& device = instance->getInputDevice();
            const bool matchesKind =
                midi ? isMidiDevice (device) : device.getDeviceType() == te::InputDevice::waveDevice;

            if (! matchesKind || ! device.isEnabled())
                continue;

            if (auto* waveDevice = dynamic_cast<te::WaveInputDevice*> (&device))
            {
                const auto& channels = waveDevice->getChannels();
                inputs.push_back ({ device.getDeviceID(), displayName (device, channels, true), channels.getNumChannels() });
                continue;
            }

            const auto name = device.getDeviceID() == MidiService::getAllInputsId() ? MidiService::getAllInputsName()
                                                                                    : device.getAliasOrName();
            inputs.push_back ({ device.getDeviceID(), name, 1 });
        }
    }

    return inputs;
}

bool TrackService::isMidiDevice (const te::InputDevice& device)
{
    const auto type = device.getDeviceType();
    return type == te::InputDevice::physicalMidiDevice || type == te::InputDevice::virtualMidiDevice;
}

bool TrackService::hasInstrument (te::AudioTrack& track)
{
    for (auto plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            return true;

    return false;
}

core::Result<void> TrackService::setAudioInput (te::AudioTrack& track, const juce::String& inputId)
{
    projectService.beginUndoStep (TRANS ("Assign input"));
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto& undoManager = edit->getUndoManager();

    // Bisherige Zuweisung lösen (eine Spur hat in M1 höchstens einen Eingang).
    if (auto* previous = findInputInstanceForTrack (track))
    {
        const auto removed = previous->removeTarget (track.itemID, &undoManager);

        if (removed.failed())
            return core::fail (core::ErrorCode::deviceError, removed.getErrorMessage().toStdString());
    }

    if (inputId.isEmpty())
        return {};

    auto* instance = findInputInstanceById (inputId);

    if (instance == nullptr)
        return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("Input not found: {id}"), { { "id", inputId } }).toStdString());

    // MIDI-Eingänge immer durchhören (wie Cubase: die Instrumentenspur spielt sofort, auch ohne
    // Scharfschaltung). Nur hier umschalten, nie während einer Aufnahme: Ein Moduswechsel baut
    // den Audio-Graphen neu auf und würde die laufende Aufnahme abbrechen.
    if (isMidiDevice (instance->getInputDevice()) &&
        instance->getInputDevice().getMonitorMode() != te::InputDevice::MonitorMode::on)
        instance->getInputDevice().setMonitorMode (te::InputDevice::MonitorMode::on);

    // moveToTrack = false: Der Eingang darf gleichzeitig weiteren Spuren zugewiesen bleiben.
    const auto assigned = instance->setTarget (track.itemID, false, &undoManager);

    if (! assigned)
        return core::fail (core::ErrorCode::deviceError, assigned.error().toStdString());

    return {};
}

juce::String TrackService::getAudioInputId (te::AudioTrack& track) const
{
    if (auto* instance = findInputInstanceForTrack (track))
        return instance->getInputDevice().getDeviceID();

    return {};
}

void TrackService::setArmed (te::AudioTrack& track, bool shouldBeArmed)
{
    if (auto* instance = findInputInstanceForTrack (track))
        instance->setRecordingEnabled (track.itemID, shouldBeArmed);
}

bool TrackService::isArmed (te::AudioTrack& track) const
{
    if (auto* instance = findInputInstanceForTrack (track))
        return instance->isRecordingEnabled (track.itemID);

    return false;
}

void TrackService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void TrackService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void TrackService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
        // Mehrfaches Anmelden ist bei JUCE unschädlich (Liste ohne Duplikate); die Engine überlebt jedes Edit.
        edit->engine.getDeviceManager().addChangeListener (this);
    }

    // Synchron, nicht asynchron: Beim Projektwechsel müssen alle Spur-Handles sofort weg,
    // weil das alte Edit direkt nach dieser Benachrichtigung zerstört wird.
    structureChangePending = true;
    cancelPendingUpdate();
    handleAsyncUpdate();
}

void TrackService::handleAsyncUpdate()
{
    // Geräte oder Zuweisungen haben sich geändert: Nur zugewiesene Eingänge bekommen den großen Puffer.
    if (auto* edit = projectService.getEdit())
        engine::EngineContext::sizeRetrospectiveBuffers (*edit);

    const bool structure = std::exchange (structureChangePending, false);
    const bool content = std::exchange (contentChangePending, false);

    if (structure)
        listeners.call ([] (Listener& listener) { listener.tracksChanged(); });
    else if (content)
        listeners.call ([] (Listener& listener) { listener.contentChanged(); });
}

void TrackService::markStructureChanged()
{
    structureChangePending = true;
    triggerAsyncUpdate();
}

void TrackService::markContentChanged()
{
    contentChangePending = true;
    triggerAsyncUpdate();
}

namespace
{
// Eingangszuweisungen und Scharfschaltung: Die Spurköpfe lesen sie neu, die Spuren bleiben.
bool isInputAssignment (const juce::ValueTree& tree)
{
    return tree.hasType (te::IDs::INPUTDEVICES) || tree.hasType (te::IDs::INPUTDEVICE) ||
           tree.hasType (te::IDs::INPUTDEVICEDESTINATION);
}

// Ein Clip jeder Art (Audio, MIDI, ...) oder etwas darin (Noten, Controller, Takes).
bool isInsideClip (juce::ValueTree tree)
{
    for (; tree.isValid() && ! tree.hasType (te::IDs::TRACK); tree = tree.getParent())
        if (te::Clip::isClipState (tree))
            return true;

    return false;
}
} // namespace

void TrackService::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (isTrackOrFolder (child))
        markStructureChanged();
    else if (isInsideClip (child) || isInsideClip (parent) || isInputAssignment (child) || isInputAssignment (parent))
        markContentChanged();
}

void TrackService::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (isTrackOrFolder (child))
    {
        // Sofort melden: Tracktion hat die Spur schon aus seiner Liste genommen und gibt sie gleich
        // frei. Bis zu einer asynchronen Meldung liefen Pegel-Timer weiter auf das alte Handle.
        structureChangePending = true;
        cancelPendingUpdate();
        handleAsyncUpdate();
        return;
    }

    if (isInsideClip (child) || isInsideClip (parent) || isInputAssignment (child) || isInputAssignment (parent))
        markContentChanged();
}

void TrackService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    // Farbe und Sichtbarkeit ändern, welche Zeilen es gibt und wie sie aussehen: Ansichten bauen neu.
    if (isTrackOrFolder (tree) &&
        (property == te::IDs::hidden || property == te::IDs::colour || property == collapsedProperty))
        markStructureChanged();
    else if (tree.hasType (te::IDs::FOLDERTRACK))
        markContentChanged();
    else if (tree.hasType (te::IDs::TRACK) || isInsideClip (tree) || isInputAssignment (tree))
        markContentChanged();
}

void TrackService::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (te::IDs::EDIT) || parent.hasType (te::IDs::FOLDERTRACK))
        markStructureChanged();
}

te::InputDeviceInstance* TrackService::findInputInstanceForTrack (te::AudioTrack& track) const
{
    // Audio- und MIDI-Eingänge gleichermaßen. getTargets() statt te::isOnTargetTrack(): Letzteres
    // prüft zusätzlich einen Slot-Index, den Tracktion bei MIDI-Eingängen anders belegt.
    if (auto* edit = projectService.getEdit())
        for (auto* instance : edit->getAllInputDevices())
            if (instance->getTargets().contains (track.itemID))
                return instance;

    return nullptr;
}

te::InputDeviceInstance* TrackService::findInputInstanceById (const juce::String& inputId) const
{
    if (auto* edit = projectService.getEdit())
        for (auto* instance : edit->getAllInputDevices())
            if (instance->getInputDevice().getDeviceID() == inputId)
                return instance;

    return nullptr;
}

} // namespace fk::services
