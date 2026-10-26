#pragma once

#include "fk/engine/EngineContext.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace fk::services
{

namespace te = tracktion;

/// Ein MIDI-Eingang, wie ihn die Einstellungen zeigen: Hardware oder virtuell (Bildschirm-Keyboard,
/// "Alle MIDI-Eingänge"), an- oder abgeschaltet.
struct MidiDeviceInfo
{
    juce::String id;
    juce::String name;
    bool enabled = false;
    bool isVirtual = false;
};

/// MIDI-Geräte und MIDI-Eingaben, die nicht von einem Gerät kommen: Das Bildschirm-Keyboard spielt
/// über ein virtuelles MIDI-Gerät der Engine, das Spuren wie jedes andere als Eingang wählen können.
/// Die Geräteliste pflegt Tracktion (Hot-Plug per Timer); wir schalten Geräte an/ab und melden Änderungen.
class MidiService final : private juce::ChangeListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Geräte kamen hinzu, gingen weg oder wurden an-/abgeschaltet (auch durch Ein-/Ausstecken).
        virtual void midiDevicesChanged() = 0;
    };

    explicit MidiService (engine::EngineContext& engineContext);
    ~MidiService() override;

    /// Name des virtuellen Geräts, wie er in der Eingangsliste erscheint.
    [[nodiscard]] static juce::String getVirtualKeyboardName();

    /// Kennung und Anzeigename von Tracktions Sammelgerät, das alle Hardware-Eingänge bündelt.
    [[nodiscard]] static juce::String getAllInputsId();
    [[nodiscard]] static juce::String getAllInputsName();

    /// Legt das virtuelle Gerät an, falls es fehlt. Einmal beim Start aufrufen.
    void ensureVirtualKeyboardExists();

    /// Alle MIDI-Eingänge, die die Engine kennt, auch abgeschaltete (für die Einstellungen).
    [[nodiscard]] std::vector<MidiDeviceInfo> getInputDevices() const;
    void setInputDeviceEnabled (const juce::String& deviceId, bool shouldBeEnabled);

    /// Sucht Hardware neu (Tracktion prüft ohnehin regelmäßig; der Button macht es sofort).
    void rescanDevices();

    void sendNoteOn (int noteNumber, float velocity);
    void sendNoteOff (int noteNumber);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void send (const juce::MidiMessage& message);

    engine::EngineContext& engineContext;
    te::MPESourceID sourceId;
    juce::ListenerList<Listener> listeners;
};

} // namespace fk::services
