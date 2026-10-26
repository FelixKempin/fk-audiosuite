#include "fk/plugins/ui/Meter.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

Meter::Meter (Style meterStyle, std::function<float()> readFunction, juce::String meterLabel)
    : style (meterStyle)
    , readDb (std::move (readFunction))
    , label (std::move (meterLabel))
{
    setTooltip (label);
    startTimerHz (30);
}

void Meter::timerCallback()
{
    const auto value = readDb ? readDb() : 0.0f;

    if (std::abs (value - lastValueDb) < 0.05f)
        return;

    lastValueDb = value;
    repaint();
}

float Meter::getLastValueDb() const noexcept
{
    return lastValueDb;
}

void Meter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    const auto labelArea = area.removeFromTop (theme::controlLabelHeight);
    const auto valueArea = area.removeFromBottom (theme::controlValueHeight);
    const auto bar = area.reduced (theme::spacingMedium, theme::spacingSmall).toFloat();

    g.setColour (theme::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawFittedText (label, labelArea, juce::Justification::centred, 1);

    g.setColour (theme::backgroundInput);
    g.fillRoundedRectangle (bar, 2.0f);

    if (style == Style::level)
    {
        const auto proportion = juce::jlimit (0.0f, 1.0f, (lastValueDb + 60.0f) / 60.0f);
        auto fill = bar.withTrimmedTop (bar.getHeight() * (1.0f - proportion));
        g.setColour (lastValueDb > -1.0f    ? theme::meterClip
                     : lastValueDb > -6.0f  ? theme::meterHigh
                     : lastValueDb > -18.0f ? theme::meterMid
                                            : theme::meterLow);
        g.fillRoundedRectangle (fill, 2.0f);
    }
    else
    {
        const auto proportion = juce::jlimit (0.0f, 1.0f, -lastValueDb / 24.0f);
        auto fill = bar.withTrimmedBottom (bar.getHeight() * (1.0f - proportion));
        g.setColour (theme::accent);
        g.fillRoundedRectangle (fill, 2.0f);
    }

    g.setColour (theme::borderSubtle);
    g.drawRoundedRectangle (bar, 2.0f, 1.0f);

    g.setColour (theme::textPrimary);
    g.drawFittedText (juce::String (lastValueDb, 1) + " dB", valueArea, juce::Justification::centred, 1);
}

} // namespace fk::plugins::ui
