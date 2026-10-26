#include "fk/plugins/ui/CurveView.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

CurveView::CurveView (ResponseFunction responseFunction)
    : response (std::move (responseFunction))
{
    setInterceptsMouseClicks (false, false);
}

void CurveView::setScale (const FrequencyScale& newScale)
{
    scale = newScale;
    updatePath();
    repaint();
}

void CurveView::refresh()
{
    updatePath();
    repaint();
}

void CurveView::updatePath()
{
    curve.clear();
    fill.clear();

    if (! response || scale.bounds.isEmpty())
        return;

    const auto zeroY = scale.yForDb (0.0f);
    const int numColumns = static_cast<int> (scale.bounds.getWidth());

    for (int column = 0; column <= numColumns; ++column)
    {
        const float x = scale.bounds.getX() + static_cast<float> (column);
        const float db = juce::jlimit (scale.minDb - 6.0f, scale.maxDb + 6.0f, response (scale.hzForX (x)));
        const float y = scale.yForDb (db);

        if (column == 0)
        {
            curve.startNewSubPath (x, y);
            fill.startNewSubPath (x, zeroY);
        }
        else
        {
            curve.lineTo (x, y);
        }

        fill.lineTo (x, y);
    }

    fill.lineTo (scale.bounds.getRight(), zeroY);
    fill.closeSubPath();
}

void CurveView::paint (juce::Graphics& g)
{
    g.setColour (theme::solo.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (theme::solo);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace fk::plugins::ui
