#pragma once

#include "fk/core/Result.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/ProjectService.h"

#include <functional>
#include <optional>
#include <vector>

namespace fk::services
{

/// Step-Eingabe (Cubase: Step-Eingabe im Key Editor): Jede Taste setzt eine Note am Step-Cursor in der
/// gewählten Länge, der Cursor rückt um einen Schritt weiter. Tasten, die gleichzeitig gehalten werden,
/// landen als Akkord auf derselben Stelle. Gehört wird auf die Tasten der Spur des Clips (alle
/// MIDI-Eingänge, die diese Spur spielen, auch das Bildschirm-Keyboard).
class StepInputService final : private te::MidiInputDevice::MidiKeyChangeDispatcher::Listener
{
public:
    /// Cursor bewegt, Step-Eingabe an oder aus.
    std::function<void()> onChanged;

    StepInputService (ProjectService& projectService, MidiClipService& midiClipService);
    ~StepInputService() override;

    [[nodiscard]] core::Result<void> start (const juce::String& clipId, double startBeats, double stepBeats);
    void stop();
    [[nodiscard]] bool isActive() const noexcept { return clipId.isNotEmpty(); }
    [[nodiscard]] juce::String getClipId() const { return clipId; }
    [[nodiscard]] double getPositionBeats() const noexcept { return positionBeats; }

    void setStepBeats (double beats);
    void setPositionBeats (double beats);

    /// Cursor ohne Note weiter (Pause) oder zurück (steps < 0).
    void advance (int steps = 1);

    /// Wie gespielte Tasten: neue Noten (mit Anschlag) und losgelassene. Für Tests und das Keyboard.
    void keysChanged (const std::vector<int>& notesOn, const std::vector<int>& velocities, const std::vector<int>& notesOff);

private:
    void midiKeyStateChanged (te::AudioTrack* track,
                              const juce::Array<int>& notesOn,
                              const juce::Array<int>& velocities,
                              const juce::Array<int>& notesOff) override;
    void notify();

    ProjectService& projectService;
    MidiClipService& midiClips;
    juce::SharedResourcePointer<te::MidiInputDevice::MidiKeyChangeDispatcher> dispatcher;
    juce::String clipId;
    juce::String trackId;
    double positionBeats = 0.0;
    double stepBeats = 0.25;
    double chordPositionBeats = 0.0; ///< Stelle des laufenden Akkords (solange Tasten gehalten sind)
    int heldKeys = 0;
};

} // namespace fk::services
