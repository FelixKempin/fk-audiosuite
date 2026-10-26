#pragma once

#include "fk/services/MidiService.h"
#include "fk/ui/IconButton.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fk::ui
{

/// Bildschirm-Keyboard in der untere Zone: Maus oder Computertastatur spielen Noten
/// in das virtuelle MIDI-Gerät; jede Spur mit diesem Eingang hört sie. Voller Klavierumfang
/// A0–C8 (21–108), beim Öffnen auf das mittlere C (C4) gerollt; Oktave-Buttons rollen weiter.
class VirtualKeyboard final : public juce::Component, private juce::MidiKeyboardState::Listener
{
public:
    static constexpr int lowestNote = 21;   ///< A0
    static constexpr int highestNote = 108; ///< C8
    static constexpr int middleC = 60;      ///< C4

    explicit VirtualKeyboard (services::MidiService& midiService);
    ~VirtualKeyboard() override;

    void resized() override;

    /// Rollt das Keyboard so, dass das mittlere C etwa in der Mitte liegt.
    void scrollToMiddleC();
    void shiftOctave (int octaves);
    [[nodiscard]] int getLowestVisibleKey() const noexcept;

private:
    IconButton octaveDownButton;
    IconButton octaveUpButton;
    juce::Label rangeLabel;
    void updateRangeLabel();
    void handleNoteOn (juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;

    services::MidiService& midi;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualKeyboard)
};

} // namespace fk::ui
