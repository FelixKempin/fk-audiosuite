#include "fk/services/MixerService.h"

#include "fk/services/BakeLock.h"
#include "fk/services/ChannelTrim.h"

#include <optional>

namespace fk::services
{

namespace
{
constexpr auto masterMeterKey = "master";
} // namespace

MixerService::MeterClient::MeterClient (te::LevelMeasurer& measurerToUse)
    : measurer (measurerToUse)
{
    measurer.addClient (client);
}

MixerService::MeterClient::~MeterClient()
{
    measurer.removeClient (client);
}

MixerService::MixerService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

MixerService::~MixerService()
{
    meterClients.clear();
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

float MixerService::getVolumeDb (TrackHandle& track) const
{
    if (auto* volume = track.getVolumePlugin())
        return volume->getVolumeDb();

    return 0.0f;
}

void MixerService::setVolumeDb (TrackHandle& track, float volumeDb)
{
    if (auto* volume = track.getVolumePlugin())
        volume->setVolumeDb (volumeDb);
}

float MixerService::getPan (TrackHandle& track) const
{
    if (auto* volume = track.getVolumePlugin())
        return volume->getPan();

    return 0.0f;
}

void MixerService::setPan (TrackHandle& track, float pan)
{
    if (auto* volume = track.getVolumePlugin())
        volume->setPan (juce::jlimit (-1.0f, 1.0f, pan));
}

void MixerService::beginGesture (TrackHandle& track, MixParameter parameter)
{
    if (auto* volume = track.getVolumePlugin())
        if (auto* automatable = parameter == MixParameter::volume ? volume->volParam.get() : volume->panParam.get())
            automatable->parameterChangeGestureBegin();
}

void MixerService::endGesture (TrackHandle& track, MixParameter parameter)
{
    if (auto* volume = track.getVolumePlugin())
        if (auto* automatable = parameter == MixParameter::volume ? volume->volParam.get() : volume->panParam.get())
            automatable->parameterChangeGestureEnd();
}

te::ExternalPlugin* MixerService::findTrim (TrackHandle& track) const
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (channelTrim::isChannelTrim (*plugin))
            return dynamic_cast<te::ExternalPlugin*> (plugin);

    return nullptr;
}

core::Result<te::ExternalPlugin*> MixerService::ensureTrim (TrackHandle& track)
{
    if (bakeLock::isBaked (track) || bakeLock::findBakedDestination (track) != nullptr)
        return std::unexpected (bakeLock::lockedError (track));

    if (auto* existing = findTrim (track))
        return existing;

    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    // getTypes() liefert eine Kopie der Liste: die Beschreibung kopieren, keinen Zeiger hineinhalten.
    std::optional<juce::PluginDescription> description;

    for (const auto& type : edit->engine.getPluginManager().knownPluginList.getTypes())
        if (channelTrim::isChannelTrim (type))
            description = type;

    if (! description)
        return core::fail (core::ErrorCode::notFound, TRANS ("FK Trim is not registered.").toStdString());

    // Direkt hinter das Instrument (falls es eins gibt), vor alle Inserts.
    int index = 0;
    int position = 0;

    for (auto* plugin : track.pluginList.getPlugins())
    {
        ++position;

        if (plugin->isSynth())
            index = position;
    }

    auto plugin = track.pluginList.insertPlugin (te::ExternalPlugin::create (edit->engine, *description), index);
    return dynamic_cast<te::ExternalPlugin*> (plugin.get());
}

namespace
{
/// Tracktion führt Plugin-Parameter normiert (0..1); der Wertebereich steht am JUCE-Parameter.
juce::RangedAudioParameter* rangedParameter (te::ExternalPlugin& plugin, const juce::String& parameterId)
{
    if (auto* instance = plugin.getAudioPluginInstance())
        for (auto* parameter : instance->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
                ranged != nullptr && ranged->getParameterID() == parameterId)
                return ranged;

    return nullptr;
}
} // namespace

float MixerService::getTrimDb (TrackHandle& track) const
{
    if (auto* trim = findTrim (track))
        if (auto* ranged = rangedParameter (*trim, plugins::TrimProcessor::gainId))
            return ranged->convertFrom0to1 (ranged->getValue());

    return 0.0f;
}

core::Result<void> MixerService::setTrimDb (TrackHandle& track, float trimDb)
{
    projectService.beginUndoStep (TRANS ("Trim"));
    const auto trim = ensureTrim (track);

    if (! trim)
        return std::unexpected (trim.error());

    // Am Plugin selbst setzen: Tracktion übernimmt den Wert über seinen Parameter-Listener in
    // Automation, Projektzustand und Render.
    if (auto* ranged = rangedParameter (**trim, plugins::TrimProcessor::gainId))
        ranged->setValueNotifyingHost (ranged->convertTo0to1 (juce::jlimit (-24.0f, 24.0f, trimDb)));

    triggerAsyncUpdate();
    return {};
}

bool MixerService::isPhaseInverted (TrackHandle& track) const
{
    if (auto* trim = findTrim (track))
        if (auto* ranged = rangedParameter (*trim, plugins::TrimProcessor::invertId))
            return ranged->getValue() > 0.5f;

    return false;
}

core::Result<void> MixerService::setPhaseInverted (TrackHandle& track, bool shouldInvert)
{
    projectService.beginUndoStep (TRANS ("Phase"));
    const auto trim = ensureTrim (track);

    if (! trim)
        return std::unexpected (trim.error());

    if (auto* ranged = rangedParameter (**trim, plugins::TrimProcessor::invertId))
        ranged->setValueNotifyingHost (shouldInvert ? 1.0f : 0.0f);

    triggerAsyncUpdate();
    return {};
}

float MixerService::getMasterVolumeDb() const
{
    if (const auto* edit = projectService.getEdit())
        if (auto master = edit->getMasterVolumePlugin())
            return master->getVolumeDb();

    return 0.0f;
}

void MixerService::setMasterVolumeDb (float volumeDb)
{
    if (auto* edit = projectService.getEdit())
        if (auto master = edit->getMasterVolumePlugin())
            master->setVolumeDb (volumeDb);
}

LevelReading MixerService::getLevels (TrackHandle& track)
{
    auto* meterPlugin = track.getLevelMeterPlugin();

    if (auto* meterClient =
            meterClientFor (track.itemID.toString(), meterPlugin != nullptr ? &meterPlugin->measurer : nullptr))
        return readLevels (*meterClient);

    return {};
}

LevelReading MixerService::getMasterLevels()
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return {};

    te::LevelMeasurer* measurer = nullptr;

    if (auto* masterTrack = edit->getMasterTrack())
        for (auto* meterPlugin : masterTrack->pluginList.getPluginsOfType<te::LevelMeterPlugin>())
            measurer = &meterPlugin->measurer;

    if (auto* meterClient = meterClientFor (masterMeterKey, measurer))
        return readLevels (*meterClient);

    return {};
}

void MixerService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void MixerService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void MixerService::projectChanged()
{
    // Meter-Abnehmer zuerst: Sie zeigen in das Edit, das gleich zerstört wird.
    meterClients.clear();

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

void MixerService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.mixerChanged(); });
}

void MixerService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (te::IDs::PLUGIN) && (property == te::IDs::volume || property == te::IDs::pan))
        triggerAsyncUpdate();
}

LevelReading MixerService::readLevels (MeterClient& meterClient)
{
    const auto left = meterClient.client.getAndClearAudioLevel (0).dB;
    const auto right =
        meterClient.client.getNumChannelsUsed() > 1 ? meterClient.client.getAndClearAudioLevel (1).dB : left;
    return { left, right };
}

MixerService::MeterClient* MixerService::meterClientFor (const juce::String& key, te::LevelMeasurer* measurer)
{
    if (measurer == nullptr)
        return nullptr;

    auto found = meterClients.find (key);

    if (found == meterClients.end())
        found = meterClients.emplace (key, std::make_unique<MeterClient> (*measurer)).first;

    return found->second.get();
}

} // namespace fk::services
