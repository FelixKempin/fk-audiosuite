#include "fk/plugins/ui/Lcd.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

Lcd::Lcd (juce::String initialText)
    : text (std::move (initialText))
{
}

void Lcd::setText (const juce::String& newText)
{
    if (text == newText)
        return;

    text = newText;
    repaint();
}

const juce::String& Lcd::getText() const noexcept
{
    return text;
}

void Lcd::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::backgroundInput);
    g.fillRoundedRectangle (area, theme::cornerRadius);
    g.setColour (theme::borderSubtle);
    g.drawRoundedRectangle (area, theme::cornerRadius, 1.0f);

    g.setColour (theme::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    g.drawFittedText (text, getLocalBounds().reduced (theme::spacingSmall, 0), juce::Justification::centred, 2);
}

} // namespace fk::plugins::ui
