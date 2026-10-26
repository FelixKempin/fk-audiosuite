#include "fk/plugins/ui/Choice.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

Choice::Choice (juce::AudioParameterChoice& parameter)
{
    nameLabel.setText (parameter.getName (24), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (nameLabel);

    comboBox.addItemList (parameter.choices, 1);
    comboBox.setTooltip (parameter.getName (64));
    addAndMakeVisible (comboBox);
    attachment = std::make_unique<juce::ComboBoxParameterAttachment> (parameter, comboBox);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromTop (theme::controlLabelHeight));
    comboBox.setBounds (area.withSizeKeepingCentre (area.getWidth() - theme::spacingSmall * 2, 24));
}

juce::ComboBox& Choice::getComboBox() noexcept
{
    return comboBox;
}

} // namespace fk::plugins::ui
