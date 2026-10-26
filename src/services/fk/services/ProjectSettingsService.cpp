#include "fk/services/ProjectSettingsService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace fk::services
{

namespace
{
const juce::Identifier settingsNode ("FKPROJECTSETTINGS");
const juce::Identifier sampleRateProperty ("sampleRate");
const juce::Identifier recordFormatProperty ("recordFormat");
const juce::Identifier bitDepthProperty ("bitDepth");
const juce::Identifier audioRecordModeProperty ("audioRecordMode");
const juce::Identifier midiRecordModeProperty ("midiRecordMode");
const juce::Identifier timecodeFpsProperty ("timecodeFps");
const juce::Identifier timeDisplayProperty ("timeDisplay");
const juce::Identifier autoCrossfadesProperty ("autoCrossfades");

/// Alle Audio-Clips unter tree (Tracktion speichert Wave-Clips als AUDIOCLIP).
void forEachAudioClipState (juce::ValueTree tree, const std::function<void (juce::ValueTree&)>& action)
{
    if (tree.hasType (te::IDs::AUDIOCLIP))
        action (tree);

    for (auto child : tree)
        forEachAudioClipState (child, action);
}

juce::String formatKey (RecordFormat format)
{
    switch (format)
    {
        case RecordFormat::flac:
            return "flac";
        case RecordFormat::aiff:
            return "aiff";
        case RecordFormat::wav:
            break;
    }

    return "wav";
}

RecordFormat formatFromKey (const juce::String& key)
{
    if (key == "flac")
        return RecordFormat::flac;

    if (key == "aiff")
        return RecordFormat::aiff;

    return RecordFormat::wav;
}

juce::AudioFormat* engineFormat (te::Engine& engine, RecordFormat format)
{
    auto& formats = engine.getAudioFileFormatManager();

    switch (format)
    {
        case RecordFormat::flac:
            return formats.getFlacFormat();
        case RecordFormat::aiff:
            return formats.getAiffFormat();
        case RecordFormat::wav:
            break;
    }

    return formats.getWavFormat();
}
} // namespace

ProjectSettingsService::ProjectSettingsService (engine::EngineContext& context, ProjectService& service)
    : engineContext (context)
    , projectService (service)
{
    projectService.addListener (*this);
}

ProjectSettingsService::~ProjectSettingsService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

ProjectSettings ProjectSettingsService::getSettings() const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return defaultSettings();

    const auto node = edit->state.getChildWithName (settingsNode);

    if (! node.isValid())
        return defaultSettings();

    ProjectSettings settings;
    settings.sampleRate = node.getProperty (sampleRateProperty, settings.sampleRate);
    settings.recordFormat = formatFromKey (node.getProperty (recordFormatProperty).toString());
    settings.bitDepth = node.getProperty (bitDepthProperty, settings.bitDepth);
    settings.audioRecordMode = static_cast<AudioRecordMode> (
        juce::jlimit (0, 1, static_cast<int> (node.getProperty (audioRecordModeProperty, 0))));
    settings.midiRecordMode = static_cast<MidiRecordMode> (
        juce::jlimit (0, 2, static_cast<int> (node.getProperty (midiRecordModeProperty, 1))));
    settings.timecodeFps = node.getProperty (timecodeFpsProperty, settings.timecodeFps);
    settings.timeDisplay =
        static_cast<TimeDisplay> (juce::jlimit (0, 2, static_cast<int> (node.getProperty (timeDisplayProperty, 0))));
    return settings;
}

core::Result<void> ProjectSettingsService::setSettings (const ProjectSettings& settings)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (settings.timecodeFps != 24 && settings.timecodeFps != 25 && settings.timecodeFps != 30)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Timecode is available with 24, 25 or 30 frames per second.").toStdString());

    if (settings.sampleRate <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The sample rate must be greater than 0.").toStdString());

    if (! std::ranges::contains (getBitDepths (settings.recordFormat), settings.bitDepth))
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("{depth} bit is not available for {format}."),
                                                  { { "depth", juce::String (settings.bitDepth) },
                                                    { "format", getFormatName (settings.recordFormat) } })
                               .toStdString());

    projectService.beginUndoStep (TRANS ("Project settings"));
    auto* undoManager = &edit->getUndoManager();
    auto node = edit->state.getOrCreateChildWithName (settingsNode, undoManager);
    node.setProperty (sampleRateProperty, settings.sampleRate, undoManager);
    node.setProperty (recordFormatProperty, formatKey (settings.recordFormat), undoManager);
    node.setProperty (bitDepthProperty, settings.bitDepth, undoManager);
    node.setProperty (audioRecordModeProperty, static_cast<int> (settings.audioRecordMode), undoManager);
    node.setProperty (midiRecordModeProperty, static_cast<int> (settings.midiRecordMode), undoManager);
    node.setProperty (timecodeFpsProperty, settings.timecodeFps, undoManager);
    node.setProperty (timeDisplayProperty, static_cast<int> (settings.timeDisplay), undoManager);
    node.setProperty (autoCrossfadesProperty, settings.autoCrossfades, undoManager);

    applyToInputs (settings);
    applyAutoCrossfades (settings.autoCrossfades);
    return {};
}

std::vector<int> ProjectSettingsService::getBitDepths (RecordFormat format)
{
    // JUCE schreibt WAV mit 32 Bit als Float; FLAC und AIFF kennen hier nur Ganzzahlen.
    if (format == RecordFormat::wav)
        return { 16, 24, 32 };

    return { 16, 24 };
}

juce::String ProjectSettingsService::getFormatName (RecordFormat format)
{
    return formatKey (format).toUpperCase();
}

double ProjectSettingsService::getDeviceSampleRate() const
{
    auto* device = engineContext.getAudioDeviceManager().getCurrentAudioDevice();
    return device != nullptr ? device->getCurrentSampleRate() : 0.0;
}

std::vector<double> ProjectSettingsService::getAvailableSampleRates() const
{
    std::vector<double> rates;

    if (auto* device = engineContext.getAudioDeviceManager().getCurrentAudioDevice())
        for (const auto rate : device->getAvailableSampleRates())
            rates.push_back (rate);

    return rates;
}

core::Result<void> ProjectSettingsService::setDeviceSampleRate (double sampleRate)
{
    auto& deviceManager = engineContext.getAudioDeviceManager();
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("No audio device is active (Studio > Audio settings).").toStdString());

    if (! device->getAvailableSampleRates().contains (sampleRate))
    {
        const auto rate = juce::String (juce::roundToInt (sampleRate));
        const auto message =
            deviceManager.getCurrentAudioDeviceType() == "JACK"
                ? plugins::i18n::format (TRANS ("The audio device \"{device}\" does not support {rate} Hz. Under PipeWire/JACK the server sets the rate, e.g. with \"pw-metadata -n settings 0 clock.force-rate {rate}\"."),
                                         { { "device", device->getName() }, { "rate", rate } })
                : plugins::i18n::format (TRANS ("The audio device \"{device}\" does not support {rate} Hz."),
                                         { { "device", device->getName() }, { "rate", rate } });

        return core::fail (core::ErrorCode::invalidArgument, message.toStdString());
    }

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.sampleRate = sampleRate;

    if (const auto error = deviceManager.setAudioDeviceSetup (setup, true); error.isNotEmpty())
        return core::fail (core::ErrorCode::ioError, error.toStdString());

    return {};
}

bool ProjectSettingsService::canDeviceRunAt (double sampleRate) const
{
    auto* device = engineContext.getAudioDeviceManager().getCurrentAudioDevice();
    return device != nullptr && device->getAvailableSampleRates().contains (sampleRate);
}

bool ProjectSettingsService::deviceMatchesProject() const
{
    const auto deviceRate = getDeviceSampleRate();
    return deviceRate <= 0.0 || std::abs (deviceRate - getSettings().sampleRate) < 0.5;
}

void ProjectSettingsService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return;

    observedEditState = edit->state;
    observedEditState.addListener (this);

    // Ältere Projekte und neue ohne Knoten: Vorgaben festschreiben (ohne Undo, das Projekt hat sie implizit).
    if (! edit->state.getChildWithName (settingsNode).isValid())
    {
        const auto defaults = defaultSettings();
        auto node = edit->state.getOrCreateChildWithName (settingsNode, nullptr);
        node.setProperty (sampleRateProperty, defaults.sampleRate, nullptr);
        node.setProperty (recordFormatProperty, formatKey (defaults.recordFormat), nullptr);
        node.setProperty (bitDepthProperty, defaults.bitDepth, nullptr);
    }

    applyToInputs (getSettings());
    applyAutoCrossfades (getSettings().autoCrossfades);
}

void ProjectSettingsService::applyAutoCrossfades (bool enabled)
{
    // Ohne Undo: Die Einstellung selbst ist der Undo-Schritt; die Clips folgen ihr nur.
    if (auto* edit = projectService.getEdit())
        forEachAudioClipState (edit->state,
                               [enabled] (juce::ValueTree& clip)
                               { clip.setProperty (te::IDs::autoCrossfade, enabled, nullptr); });
}

void ProjectSettingsService::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    // Gleicher Wert ändert nichts (auch beim Undo eines Löschens kommt der Clip mit seinem Wert zurück).
    const auto enabled = getSettings().autoCrossfades;
    forEachAudioClipState (
        child, [enabled] (juce::ValueTree& clip) { clip.setProperty (te::IDs::autoCrossfade, enabled, nullptr); });
}

void ProjectSettingsService::applyToInputs (const ProjectSettings& settings)
{
    auto& engine = engineContext.getEngine();
    const auto formatName = engineFormat (engine, settings.recordFormat)->getFormatName();
    const auto audioModes = te::WaveInputDevice::getMergeModes(); // 0 = überlagern, 1 = ersetzen
    const auto audioMode = audioModes[settings.audioRecordMode == AudioRecordMode::replace ? 1 : 0];

    for (auto* input : engine.getDeviceManager().getWaveInputDevices())
    {
        input->setOutputFormat (formatName);
        input->setBitDepth (settings.bitDepth);
        input->setMergeMode (audioMode);
    }

    for (auto& input : engine.getDeviceManager().getMidiInDevices())
    {
        input->mergeRecordings = settings.midiRecordMode == MidiRecordMode::merge;
        input->replaceExistingClips = settings.midiRecordMode == MidiRecordMode::replace;
    }
}

ProjectSettings ProjectSettingsService::defaultSettings() const
{
    ProjectSettings settings;

    if (const auto deviceRate = getDeviceSampleRate(); deviceRate > 0.0)
        settings.sampleRate = deviceRate;

    return settings;
}

} // namespace fk::services
