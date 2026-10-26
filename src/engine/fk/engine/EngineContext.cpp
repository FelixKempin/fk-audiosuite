#include "fk/engine/EngineContext.h"

#include "fk/engine/FkEngineBehaviour.h"
#include "fk/engine/FkUIBehaviour.h"
#include "fk/plugins/PluginCatalog.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::engine
{

EngineContext::EngineContext (const juce::String& applicationName)
    : engine (applicationName, std::make_unique<FkUIBehaviour>(), std::make_unique<FkEngineBehaviour>())
{
    // Tracktion liest diese Einstellung, wenn es den Puffer eines Eingangs anlegt: Neue Eingänge
    // starten klein, sizeRetrospectiveBuffers vergrößert die zugewiesenen. 0 hieße "abgeschaltet".
    engine.getPropertyStorage().setProperty (te::SettingID::retrospectiveRecord, retrospectiveStandbySeconds);
}

void EngineContext::sizeRetrospectiveBuffers (te::Edit& edit)
{
    for (auto* instance : edit.getAllInputDevices())
    {
        auto& device = instance->getInputDevice();
        const bool isAudio = device.getDeviceType() == te::InputDevice::waveDevice;

        // MIDI-Puffer sind klein (nur Ereignisse): Die behalten immer die volle Länge.
        device.updateRetrospectiveBufferLength (
            isAudio && instance->getTargets().isEmpty() ? retrospectiveStandbySeconds : retrospectiveSeconds);
    }
}

void EngineContext::setFocusedEdit (te::Edit* edit) noexcept
{
    if (auto* behaviour = dynamic_cast<FkUIBehaviour*> (&engine.getUIBehaviour()))
        behaviour->setFocusedEdit (edit);
}

void EngineContext::setProgressWindowsEnabled (bool shouldShow) noexcept
{
    if (auto* behaviour = dynamic_cast<FkUIBehaviour*> (&engine.getUIBehaviour()))
        behaviour->setProgressWindowsEnabled (shouldShow);
}

EngineContext::~EngineContext() = default;

bool EngineContext::handlePluginScanWorkerStartup (const juce::String& commandLine)
{
    return te::PluginManager::startChildProcessPluginScan (commandLine);
}

te::Engine& EngineContext::getEngine() noexcept
{
    return engine;
}

te::DeviceManager& EngineContext::getDeviceManager() noexcept
{
    return engine.getDeviceManager();
}

juce::AudioDeviceManager& EngineContext::getAudioDeviceManager() noexcept
{
    return engine.getDeviceManager().deviceManager;
}

void EngineContext::registerInternalPlugins()
{
    auto& pluginManager = engine.getPluginManager();

    for (const auto& description : plugins::PluginCatalog::getDescriptions())
        pluginManager.knownPluginList.addType (description);

    // Tracktion fragt diese Funktion, wenn es ein Plugin aus einer Beschreibung erzeugen soll.
    // Unsere Plugins bauen wir selbst, alles andere geht den normalen Weg über JUCE.
    pluginManager.createPluginInstance =
        [&pluginManager] (const juce::PluginDescription& description,
                          double sampleRate,
                          int blockSize,
                          juce::String& errorMessage) -> std::unique_ptr<juce::AudioPluginInstance>
    {
        if (plugins::PluginCatalog::isInternal (description))
        {
            if (auto instance = plugins::PluginCatalog::createInstance (description))
                return instance;

            errorMessage = plugins::i18n::format (TRANS ("Unknown FK plugin: {name}"), { { "name", description.name } });
            return nullptr;
        }

        return pluginManager.pluginFormatManager.createPluginInstance (
            description, sampleRate, blockSize, errorMessage);
    };
}

void EngineContext::enableAllAudioInputs()
{
    auto& deviceManager = engine.getDeviceManager();

    for (int i = 0; i < deviceManager.getNumWaveInDevices(); ++i)
    {
        if (auto* input = deviceManager.getWaveInDevice (i))
        {
            // Kanalbelegung (Mono/Stereo) kommt aus FkEngineBehaviour::describeWaveDevices.
            input->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            input->setEnabled (true);
        }
    }
}

} // namespace fk::engine
