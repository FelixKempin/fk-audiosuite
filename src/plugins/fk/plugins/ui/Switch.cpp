#include "fk/plugins/ui/Switch.h"

#include "fk/plugins/i18n/Text.h"

namespace fk::plugins::ui
{

Switch::Switch (juce::RangedAudioParameter& parameter)
    : attachment (parameter, button)
{
    button.setButtonText (parameter.getName (24));
    button.setTooltip (i18n::format (TRANS ("{name} on/off"), { { "name", parameter.getName (64) } }));
    addAndMakeVisible (button);
}

void Switch::resized()
{
    button.setBounds (getLocalBounds());
}

juce::ToggleButton& Switch::getButton() noexcept
{
    return button;
}

} // namespace fk::plugins::ui
