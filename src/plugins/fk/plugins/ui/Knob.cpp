#include "fk/plugins/ui/Knob.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

Knob::Knob (juce::RangedAudioParameter& parameter)
    : attachment (parameter, slider)
{
    nameLabel.setText (parameter.getName (24), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, theme::knobWidth, theme::controlValueHeight);
    slider.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
    slider.setTooltip (
        i18n::format (TRANS ("{name} (double-click: default value, Ctrl+drag: fine)"), { { "name", parameter.getName (64) } }));

    // Die Bindung formatiert den Wert wie der Parameter (Nachkommastellen); die Einheit ("dB",
    // "Hz") hängt der Parameter nicht selbst an, also hier ergänzen.
    if (const auto unit = parameter.getLabel(); unit.isNotEmpty())
    {
        slider.textFromValueFunction = [unit, format = slider.textFromValueFunction] (double value)
        { return (format != nullptr ? format (value) : juce::String (value)) + " " + unit; };
        slider.updateText(); // die Bindung hat den Text schon ohne Einheit gesetzt
    }
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromTop (theme::controlLabelHeight));
    slider.setBounds (area);
}

void Knob::setDisplayName (const juce::String& name)
{
    nameLabel.setText (name, juce::dontSendNotification);
}

juce::Slider& Knob::getSlider() noexcept
{
    return slider;
}

} // namespace fk::plugins::ui
