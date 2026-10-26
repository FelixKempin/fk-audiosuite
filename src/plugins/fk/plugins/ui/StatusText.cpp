#include "fk/plugins/ui/StatusText.h"

namespace fk::plugins::ui
{

StatusText::StatusText (std::function<juce::String()> readFunction)
    : readText (std::move (readFunction))
{
    addAndMakeVisible (lcd);
    timerCallback();
    startTimerHz (10);
}

void StatusText::resized()
{
    lcd.setBounds (getLocalBounds().reduced (4, 0).withSizeKeepingCentre (getWidth() - 8, 24));
}

void StatusText::timerCallback()
{
    if (readText)
        lcd.setText (readText());
}

} // namespace fk::plugins::ui
