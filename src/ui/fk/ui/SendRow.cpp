#include "fk/ui/SendRow.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr double minLevelDb = -60.0;
constexpr double maxLevelDb = 6.0;
} // namespace

SendRow::SendRow()
    : preButton (TRANS ("Pre"))
{
    nameLabel.setFont (juce::FontOptions (11.0f));
    nameLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    nameLabel.setMinimumHorizontalScale (0.7f);

    levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setRange (minLevelDb, maxLevelDb, 0.1);
    levelSlider.setSkewFactorFromMidPoint (-12.0);
    levelSlider.setDoubleClickReturnValue (true, minLevelDb);
    levelSlider.setTooltip (TRANS ("Send level; fully left = off"));
    levelSlider.onValueChange = [this]
    {
        if (onLevelChanged)
            onLevelChanged (levelSlider.getValue() <= minLevelDb ? -100.0f
                                                                 : static_cast<float> (levelSlider.getValue()));
    };

    preButton.setClickingTogglesState (true);
    preButton.setTooltip (TRANS ("Pre fader: send independent of the track fader"));
    preButton.onClick = [this]
    {
        if (onPreFaderChanged)
            onPreFaderChanged (preButton.getToggleState());
    };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (levelSlider);
    addAndMakeVisible (preButton);
}

void SendRow::setSend (const services::SendInfo& send)
{
    info = send;
    nameLabel.setText (send.fxName, juce::dontSendNotification);
    levelSlider.setValue (send.exists ? juce::jlimit (minLevelDb, maxLevelDb, static_cast<double> (send.gainDb))
                                      : minLevelDb,
                          juce::dontSendNotification);
    preButton.setToggleState (send.preFader, juce::dontSendNotification);
    preButton.setEnabled (send.exists);
}

const services::SendInfo& SendRow::getSend() const noexcept
{
    return info;
}

void SendRow::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromLeft (juce::jmin (70, area.getWidth() / 3)));
    preButton.setBounds (area.removeFromRight (34));
    area.removeFromRight (theme::spacingSmall);
    levelSlider.setBounds (area);
}

} // namespace fk::ui
