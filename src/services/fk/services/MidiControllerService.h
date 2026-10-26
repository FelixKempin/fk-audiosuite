#pragma once

#include "fk/core/Result.h"
#include "fk/services/ControllerTypes.h"
#include "fk/services/ProjectService.h"

#include <utility>
#include <vector>

namespace fk::services
{

/// Controller-Ereignisse in MIDI-Clips bearbeiten: Sustain, Mod-Wheel, Pitch-Bend, Aftertouch,
/// beliebige CCs. Gelesen werden sie über MidiClipService::getClip (MidiClipInfo::controllers);
/// Änderungen meldet ebenfalls der MidiClipService (midiClipChanged), weil sie im Clip liegen.
/// Ein Ereignis wird über seinen Index in der Ereignisliste des Clips angesprochen.
class MidiControllerService final
{
public:
    explicit MidiControllerService (ProjectService& projectService);

    [[nodiscard]] core::Result<void> addControllerEvent (const juce::String& clipId, int type, double beats, int value);
    [[nodiscard]] core::Result<void>
    setControllerEvent (const juce::String& clipId, int eventIndex, double beats, int value);
    [[nodiscard]] core::Result<void> removeControllerEvent (const juce::String& clipId, int eventIndex);

    /// Löscht alle Ereignisse einer Art im Bereich [fromBeats, toBeats).
    [[nodiscard]] core::Result<void>
    removeControllerEvents (const juce::String& clipId, int type, double fromBeats, double toBeats);

    /// Neu zeichnen in einem Undo-Schritt: alte Ereignisse der Art in [fromBeats, toBeats) weg,
    /// die Punkte (Schlag, Wert) hinein.
    [[nodiscard]] core::Result<void> replaceControllerEvents (const juce::String& clipId,
                                                              int type,
                                                              double fromBeats,
                                                              double toBeats,
                                                              const std::vector<std::pair<double, int>>& points);

private:
    /// Gebackene Spuren sperren ihre Clips (ADR 0009).
    [[nodiscard]] static core::Result<void> checkClipUnlocked (te::MidiClip& clip);
    [[nodiscard]] te::MidiClip* findClip (const juce::String& clipId) const;

    ProjectService& projectService;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiControllerService)
};

} // namespace fk::services
