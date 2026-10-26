#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"
#include "fk/services/TransportService.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <optional>
#include <vector>

namespace fk::services
{

/// Einstellungen einer MIDI-Spur an einem externen Gerät (Hardware-Synth, Drum-Computer).
struct ExternalMidiSettings
{
    int channel = 1;  ///< 1..16
    int program = -1; ///< 0..127, -1 = kein Programmwechsel
    int bank = -1;    ///< 0..16383 (MSB · 128 + LSB), -1 = keine Bank senden
};

/// MIDI-Spuren an externen Geräten (Cubase: MIDI-Spur mit Ausgang auf einen MIDI-Port). Die Spur hat
/// kein Instrument; ihr Ausgang ist ein MIDI-Ausgang (TrackService::setOutput mit "device:<ID>").
/// Kanal über eine Patch Bay wie beim Rack-Instrument; Programm und Bank gehen beim Ändern und bei
/// jedem Start der Wiedergabe an das Gerät, damit es nach dem Einschalten den richtigen Klang hat.
class ExternalMidiService final : private TransportService::Listener
{
public:
    ExternalMidiService (ProjectService& projectService, TrackService& trackService, TransportService& transport);
    ~ExternalMidiService() override;

    /// Neue MIDI-Spur, Ausgang auf den ersten MIDI-Ausgang (falls es einen gibt), Kanal 1.
    [[nodiscard]] core::Result<te::AudioTrack*> createMidiTrack (const juce::String& name);

    [[nodiscard]] std::optional<ExternalMidiSettings> getSettings (te::AudioTrack& track) const;
    [[nodiscard]] core::Result<void> setChannel (te::AudioTrack& track, int channel);
    [[nodiscard]] core::Result<void> setProgram (te::AudioTrack& track, int program, int bank);

    /// Programm und Bank aller MIDI-Spuren an ihre Geräte senden (auch beim Start der Wiedergabe).
    void sendAllPrograms();

    /// Die Nachrichten für einen Programmwechsel: Bank MSB (CC 0), Bank LSB (CC 32), Program Change.
    [[nodiscard]] static std::vector<juce::MidiMessage> programMessages (const ExternalMidiSettings& settings);

    /// Für Tests und Diagnose: zuletzt an ein Gerät geschickte Nachrichten (auch ohne Gerät gezählt).
    [[nodiscard]] int getNumMessagesSent() const noexcept { return messagesSent; }

private:
    void transportStateChanged() override;
    void sendProgram (te::AudioTrack& track);

    ProjectService& projectService;
    TrackService& trackService;
    TransportService& transport;
    bool wasPlaying = false;
    int messagesSent = 0;
};

} // namespace fk::services
