#include "fk/ui/VirtualKeyboard.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

VirtualKeyboard::VirtualKeyboard (services::MidiService& midiService)
    : octaveDownButton (Icon::octaveDown, TRANS ("Shift down one octave"))
    , octaveUpButton (Icon::octaveUp, TRANS ("Shift up one octave"))
    , midi (midiService)
    , keyboard (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    keyboard.setAvailableRange (lowestNote, highestNote);
    keyboard.setOctaveForMiddleC (4); // wissenschaftliche Benennung: mittleres C = C4
    keyboard.setKeyWidth (24.0f);
    keyboard.setScrollButtonsVisible (true);
    octaveDownButton.onClick = [this] { shiftOctave (-1); };
    octaveUpButton.onClick = [this] { shiftOctave (1); };
    rangeLabel.setFont (juce::FontOptions (12.0f));
    rangeLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    rangeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);
    addAndMakeVisible (rangeLabel);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme::accent.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme::accent.withAlpha (0.25f));
    addAndMakeVisible (keyboard);

    keyboardState.addListener (this);
}

VirtualKeyboard::~VirtualKeyboard()
{
    keyboardState.removeListener (this);
}

void VirtualKeyboard::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingMedium);
    auto column = area.removeFromLeft (36);
    octaveUpButton.setBounds (column.removeFromTop (28));
    column.removeFromTop (theme::spacingSmall);
    octaveDownButton.setBounds (column.removeFromTop (28));
    rangeLabel.setBounds (column.removeFromBottom (18).withWidth (60));
    area.removeFromLeft (theme::spacingSmall);
    keyboard.setBounds (area);

    static bool scrolledOnce = false; // beim ersten Layout auf das mittlere C

    if (! scrolledOnce && getWidth() > 0)
    {
        scrolledOnce = true;
        scrollToMiddleC();
    }
}

void VirtualKeyboard::scrollToMiddleC()
{
    const int visibleKeys =
        juce::jmax (1, static_cast<int> (keyboard.getWidth() / keyboard.getKeyWidth() * 12.0f / 7.0f));
    keyboard.setLowestVisibleKey (juce::jlimit (lowestNote, highestNote, middleC - visibleKeys / 2));
    updateRangeLabel();
}

void VirtualKeyboard::shiftOctave (int octaves)
{
    keyboard.setLowestVisibleKey (
        juce::jlimit (lowestNote, highestNote, keyboard.getLowestVisibleKey() + 12 * octaves));
    updateRangeLabel();
}

int VirtualKeyboard::getLowestVisibleKey() const noexcept
{
    return keyboard.getLowestVisibleKey();
}

void VirtualKeyboard::updateRangeLabel()
{
    rangeLabel.setText (juce::MidiMessage::getMidiNoteName (keyboard.getLowestVisibleKey(), true, true, 4),
                        juce::dontSendNotification);
}

void VirtualKeyboard::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    midi.sendNoteOn (midiNoteNumber, velocity);
}

void VirtualKeyboard::handleNoteOff (juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    midi.sendNoteOff (midiNoteNumber);
}

} // namespace fk::ui
