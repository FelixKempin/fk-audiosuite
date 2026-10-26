#pragma once

#include "fk/engine/EngineContext.h"
#include "fk/engine/TestToneSource.h"
#include "fk/services/MidiService.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <vector>

namespace fk::ui
{

/// Audio- und MIDI-Einstellungen: Gerät, Samplerate, Puffergröße, Kanäle; dazu ein Testton,
/// um die Ausgänge zu prüfen; darunter alle MIDI-Eingänge zum An- und Abschalten (Hot-Plug:
/// die Liste baut sich bei Ein-/Ausstecken selbst neu). Wird als eigenes Fenster geöffnet.
class AudioSettingsComponent final : public juce::Component, private services::MidiService::Listener
{
public:
    AudioSettingsComponent (engine::EngineContext& engineContext, services::MidiService& midiService);
    ~AudioSettingsComponent() override;

    void resized() override;

private:
    void midiDevicesChanged() override;
    void rebuildMidiList();

    engine::EngineContext& engineContext;
    services::MidiService& midi;
    engine::TestToneSource toneSource;
    juce::AudioSourcePlayer sourcePlayer;

    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::TextButton toneButton;
    juce::Slider frequencySlider;
    juce::Label frequencyLabel;
    juce::Label midiTitle;
    juce::TextButton rescanButton;
    std::vector<std::unique_ptr<juce::ToggleButton>> midiToggles;
    std::vector<juce::String> midiDeviceIds;
    juce::Label midiEmptyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsComponent)
};

} // namespace fk::ui
