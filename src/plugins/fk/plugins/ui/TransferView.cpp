#include "fk/plugins/ui/TransferView.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

namespace
{
constexpr float axisMargin = 30.0f; // Platz für die Beschriftung links und unten
constexpr int refreshHz = 30;
} // namespace

TransferView::TransferView (Transfer transferFunction, Axis x, Axis y, LivePoint live)
    : transfer (std::move (transferFunction))
    , xAxis (std::move (x))
    , yAxis (std::move (y))
    , livePoint (std::move (live))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (refreshHz);
}

TransferView::~TransferView()
{
    stopTimer();
}

void TransferView::timerCallback()
{
    repaint();
}

juce::Rectangle<float> TransferView::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (axisMargin).withTrimmedBottom (axisMargin * 0.7f).reduced (6.0f);
}

float TransferView::xFor (float value) const
{
    const auto area = plotArea();
    return area.getX() + (value - xAxis.minimum) / (xAxis.maximum - xAxis.minimum) * area.getWidth();
}

float TransferView::yFor (float value) const
{
    const auto area = plotArea();
    return area.getBottom() - (value - yAxis.minimum) / (yAxis.maximum - yAxis.minimum) * area.getHeight();
}

void TransferView::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    g.setColour (theme::backgroundInput);
    g.fillRect (area);

    // Raster mit Beschriftung.
    g.setFont (juce::FontOptions (10.0f));

    for (const auto tick : xAxis.ticks)
    {
        g.setColour (theme::gridBeat);
        g.drawVerticalLine (juce::roundToInt (xFor (tick)), area.getY(), area.getBottom());
        g.setColour (theme::textSecondary);
        g.drawText (juce::String (tick, 0),
                    juce::Rectangle<float> (xFor (tick) - 20.0f, area.getBottom() + 2.0f, 40.0f, 12.0f),
                    juce::Justification::centred);
    }

    for (const auto tick : yAxis.ticks)
    {
        g.setColour (theme::gridBeat);
        g.drawHorizontalLine (juce::roundToInt (yFor (tick)), area.getX(), area.getRight());
        g.setColour (theme::textSecondary);
        g.drawText (juce::String (tick, 0),
                    juce::Rectangle<float> (0.0f, yFor (tick) - 6.0f, axisMargin - 4.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    g.setColour (theme::textSecondary);
    g.drawText (
        xAxis.label, area.withTop (area.getBottom() + 12.0f).withHeight (12.0f), juce::Justification::centredRight);
    g.drawText (yAxis.label, area.withHeight (12.0f).translated (4.0f, 2.0f), juce::Justification::topLeft);

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // Bezug: Ausgang = Eingang (nur sinnvoll, wenn beide Achsen dieselbe Einheit haben; dann liegt sie diagonal).
    g.setColour (theme::borderStrong);
    g.drawLine (xFor (xAxis.minimum), yFor (xAxis.minimum), xFor (xAxis.maximum), yFor (xAxis.maximum), 1.0f);

    // Kennlinie je Pixelspalte.
    juce::Path curve;
    const auto columns = juce::jmax (2, juce::roundToInt (area.getWidth()));

    for (int column = 0; column <= columns; ++column)
    {
        const auto input = xAxis.minimum +
                           (xAxis.maximum - xAxis.minimum) * static_cast<float> (column) / static_cast<float> (columns);
        const auto point = juce::Point<float> (xFor (input), yFor (transfer (input)));

        if (column == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    g.setColour (theme::accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    if (livePoint)
        if (const auto point = livePoint())
        {
            g.setColour (theme::solo);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ xFor (point->x), yFor (point->y) }));
        }

    g.restoreState();
}

} // namespace fk::plugins::ui
