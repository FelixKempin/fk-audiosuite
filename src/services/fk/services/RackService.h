#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <optional>
#include <vector>

namespace fk::services
{

/// Ein Rack-Instrument (Cubase: Rack-Instrument): eine Instanz eines Instruments, gespielt von
/// mehreren MIDI-Spuren auf eigenen Kanälen, jeder aktivierte Stereo-Ausgang ein eigener Kanal im Mixer.
struct RackInfo
{
    juce::String id;
    juce::String name;
    juce::String instrumentId;         ///< Insert-Kennung des Instruments (Editor öffnen, Presets)
    int numOutputPairs = 1;            ///< Stereo-Ausgänge, die das Instrument hat
    std::vector<bool> activeOutputs;   ///< je Ausgang: hat einen Kanal im Mixer
    std::vector<juce::String> outputNames;
    int numMidiTracks = 0;
};

/// Wofür eine Spur im Rack steht.
struct RackRole
{
    juce::String rackId;
    bool isMidi = true; ///< MIDI-Spur (spielt das Rack) oder Ausgangskanal
    int midiChannel = 1;
    int outputPair = 0;
};

/// Anwendungsfälle rund um Rack-Instrumente. Technisch ist das Instrument ein Tracktion-Rack;
/// jede beteiligte Spur trägt eine Rack-Instanz. MIDI-Spuren setzen ihren Kanal mit einer
/// Patch Bay davor und holen nichts zurück; Ausgangskanäle holen genau ihr Ausgangspaar zurück.
/// Tracktion rechnet das Rack einmal, egal wie viele Spuren daran hängen.
class RackService final
{
public:
    RackService (ProjectService& projectService, TrackService& trackService);

    /// Legt das Rack mit dem Instrument an, dazu eine MIDI-Spur auf Kanal 1 und den Kanal für
    /// Ausgang 1+2. Liefert die MIDI-Spur. Ein Undo-Schritt.
    [[nodiscard]] core::Result<te::AudioTrack*> createRackInstrument (const juce::String& instrumentPluginId,
                                                                      const juce::String& name);

    /// Weitere MIDI-Spur an das Rack, auf dem nächsten freien Kanal (oder dem angegebenen).
    [[nodiscard]] core::Result<te::AudioTrack*> addMidiTrack (const juce::String& rackId, int midiChannel = 0);

    /// MIDI-Kanal einer MIDI-Spur am Rack (1..16): gilt für Clips und Live-Spiel.
    [[nodiscard]] core::Result<void> setMidiChannel (te::AudioTrack& track, int midiChannel);

    /// Ausgang im Mixer ein- oder ausschalten (Cubase: Ausgänge aktivieren). Aus löscht den Kanal.
    [[nodiscard]] core::Result<void> setOutputActive (const juce::String& rackId, int outputPair, bool active);

    [[nodiscard]] std::vector<RackInfo> getRacks() const;
    [[nodiscard]] std::optional<RackInfo> getRack (const juce::String& rackId) const;
    [[nodiscard]] std::optional<RackRole> getRole (te::AudioTrack& track) const;

private:
    [[nodiscard]] te::RackType::Ptr findRack (const juce::String& rackId) const;
    [[nodiscard]] core::Result<te::AudioTrack*> appendMidiTrack (te::RackType& rack, int midiChannel);
    [[nodiscard]] core::Result<te::AudioTrack*> appendOutputTrack (te::RackType& rack, int outputPair);
    [[nodiscard]] te::AudioTrack* findOutputTrack (const juce::String& rackId, int outputPair) const;
    [[nodiscard]] static int numOutputPairs (te::RackType& rack);

    ProjectService& projectService;
    TrackService& trackService;
};

} // namespace fk::services
