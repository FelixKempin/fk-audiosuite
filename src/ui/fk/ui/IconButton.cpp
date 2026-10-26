#include "fk/ui/IconButton.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

IconButton::IconButton (Icon buttonIcon, const juce::String& tooltip, const juce::String& buttonLabel)
    : juce::Button (buttonLabel.isNotEmpty() ? buttonLabel : tooltip)
    , icon (buttonIcon)
    , label (buttonLabel)
    , onColour (theme::accent)
{
    setTooltip (tooltip);
}

void IconButton::setOnColour (juce::Colour colour)
{
    onColour = colour;
    repaint();
}

void IconButton::setCompact (bool shouldBeCompact)
{
    if (compact != shouldBeCompact)
    {
        compact = shouldBeCompact;
        repaint();
    }
}

void IconButton::setIcon (Icon newIcon)
{
    icon = newIcon;
    repaint();
}

void IconButton::setLabel (const juce::String& newLabel)
{
    label = newLabel;
    repaint();
}

void IconButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto area = getLocalBounds().toFloat();
    const bool on = getToggleState();

    auto background = theme::backgroundElevated;

    if (on)
        background = onColour;
    else if (isButtonDown)
        background = theme::borderStrong;
    else if (isMouseOver)
        background = theme::backgroundElevated.brighter (0.15f);

    if (! isEnabled())
        background = background.withAlpha (0.4f);

    g.setColour (background);
    g.fillRoundedRectangle (area.reduced (0.5f), 4.0f);

    const auto foreground = isEnabled() ? theme::textPrimary : theme::textDisabled;

    if (label.isEmpty() || compact)
    {
        drawIcon (g,
                  icon,
                  area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                              juce::jmin (area.getWidth(), area.getHeight())),
                  foreground);
        return;
    }

    // Symbol links, Text rechts.
    auto bounds = getLocalBounds().reduced (theme::spacingSmall, 2);
    const auto iconArea = bounds.removeFromLeft (bounds.getHeight()).toFloat();
    drawIcon (g, icon, iconArea, foreground);
    g.setColour (foreground);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (label, bounds.withTrimmedLeft (2), juce::Justification::centredLeft, true);
}

} // namespace fk::ui
