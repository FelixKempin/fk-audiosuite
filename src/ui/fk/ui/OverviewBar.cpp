#include "fk/ui/OverviewBar.h"

#include "fk/ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace fk::ui
{

namespace
{
constexpr float gripWidth = 5.0f;         ///< Fangbreite der Rahmenränder in Pixeln
constexpr double minimumVisibleSeconds = 0.5;
} // namespace

OverviewBar::OverviewBar()
{
    setTooltip (TRANS ("Overview: drag the frame = scroll, drag the frame edges = zoom, click beside it = jump there"));
}

void OverviewBar::setContent (std::vector<Row> newRows, double lengthSeconds)
{
    rows = std::move (newRows);
    totalSeconds = std::max (lengthSeconds, 1.0);
    repaint();
}

void OverviewBar::setVisibleRange (double fromSeconds, double toSeconds)
{
    visibleFrom = fromSeconds;
    visibleTo = std::max (toSeconds, fromSeconds + 0.001);
    repaint();
}

double OverviewBar::xToSeconds (float x) const
{
    return static_cast<double> (x) / std::max (1, getWidth()) * totalSeconds;
}

float OverviewBar::secondsToX (double seconds) const
{
    return static_cast<float> (seconds / totalSeconds * getWidth());
}

void OverviewBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    const auto area = getLocalBounds().toFloat().reduced (0.0f, 3.0f);

    if (! rows.empty())
    {
        const auto rowHeight = std::clamp (area.getHeight() / static_cast<float> (rows.size()), 1.0f, 6.0f);

        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const auto y = area.getY() + static_cast<float> (i) * rowHeight;

            if (y + rowHeight > area.getBottom() + 0.5f)
                break; // sehr viele Spuren: die unteren passen nicht mehr hinein

            g.setColour (rows[i].colour.withAlpha (0.85f));

            for (const auto& [start, length] : rows[i].clips)
            {
                const auto x = secondsToX (start);
                g.fillRect (x, y, std::max (1.0f, secondsToX (start + length) - x), std::max (1.0f, rowHeight - 1.0f));
            }
        }
    }

    // Sichtbarer Ausschnitt: abgedunkelt außerhalb, Rahmen innen.
    const auto left = secondsToX (visibleFrom);
    const auto right = secondsToX (visibleTo);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (0.0f, 0.0f, left, static_cast<float> (getHeight()));
    g.fillRect (right, 0.0f, static_cast<float> (getWidth()) - right, static_cast<float> (getHeight()));
    g.setColour (theme::accent);
    g.drawRect (juce::Rectangle<float> (left, 0.5f, std::max (2.0f, right - left), static_cast<float> (getHeight()) - 1.0f),
                1.5f);

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

OverviewBar::Grip OverviewBar::gripAt (float x) const
{
    const auto left = secondsToX (visibleFrom);
    const auto right = secondsToX (visibleTo);

    if (std::abs (x - left) <= gripWidth)
        return Grip::left;

    if (std::abs (x - right) <= gripWidth)
        return Grip::right;

    if (x > left && x < right)
        return Grip::move;

    return Grip::none;
}

void OverviewBar::mouseMove (const juce::MouseEvent& event)
{
    const auto grip = gripAt (event.position.x);
    setMouseCursor (grip == Grip::left || grip == Grip::right ? juce::MouseCursor::LeftRightResizeCursor
                    : grip == Grip::move                      ? juce::MouseCursor::DraggingHandCursor
                                                              : juce::MouseCursor::PointingHandCursor);
}

void OverviewBar::mouseDown (const juce::MouseEvent& event)
{
    auto grip = gripAt (event.position.x);

    if (grip == Grip::none)
    {
        // Klick daneben: Ausschnitt mittig auf die Stelle, dann wie Verschieben weiterziehen.
        const auto width = visibleTo - visibleFrom;
        const auto from = std::max (0.0, xToSeconds (event.position.x) - width / 2.0);
        visibleFrom = from;
        visibleTo = from + width;

        if (onRangeChanged)
            onRangeChanged (visibleFrom, visibleTo);

        grip = Grip::move;
    }

    drag = Drag { grip, visibleFrom, visibleTo, xToSeconds (event.position.x) };
    repaint();
}

void OverviewBar::mouseDrag (const juce::MouseEvent& event)
{
    if (! drag)
        return;

    const auto delta = xToSeconds (event.position.x) - drag->anchorSeconds;
    auto from = drag->startFrom;
    auto to = drag->startTo;

    switch (drag->grip)
    {
        case Grip::move:
        {
            const auto width = to - from;
            from = std::max (0.0, from + delta);
            to = from + width;
            break;
        }
        case Grip::left:
            from = std::clamp (from + delta, 0.0, to - minimumVisibleSeconds);
            break;
        case Grip::right:
            to = std::max (to + delta, from + minimumVisibleSeconds);
            break;
        case Grip::none:
            return;
    }

    visibleFrom = from;
    visibleTo = to;
    repaint();

    if (onRangeChanged)
        onRangeChanged (from, to);
}

void OverviewBar::mouseUp (const juce::MouseEvent&)
{
    drag.reset();
}

} // namespace fk::ui
