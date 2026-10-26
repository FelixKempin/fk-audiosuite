#include "fk/ui/TimeRuler.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int dragThresholdPixels = 4;
} // namespace

TimeRuler::TimeRuler (services::TransportService& transportService, services::TimelineService& timelineService)
    : transport (transportService)
    , timeline (timelineService)
{
}

void TimeRuler::setTimeMapping (TimeMapping newMapping)
{
    mapping = newMapping;
    repaint();
}

void TimeRuler::setGridResolution (services::GridResolution resolution)
{
    gridResolution = resolution;
    repaint();
}

void TimeRuler::setSnapEnabled (bool shouldSnap)
{
    snapEnabled = shouldSnap;
}

void TimeRuler::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    // Loop-Bereich als Band im oberen Drittel, wie in Cubase.
    const auto loop = timeline.getLoopRange();

    if (loop.endSeconds > loop.startSeconds)
    {
        const auto x1 = mapping.secondsToX (loop.startSeconds);
        const auto x2 = mapping.secondsToX (loop.endSeconds);
        g.setColour (timeline.isLooping() ? theme::accent.withAlpha (0.6f) : theme::borderStrong);
        g.fillRect (x1, 0, juce::jmax (1, x2 - x1), getHeight() / 3);
    }

    g.setFont (juce::FontOptions (11.0f));

    // Takte beschriften; feinere Linien nur, wenn Platz ist (sonst wird es zum Strichcode).
    const auto lines = timeline.getGridLines (mapping.xToSeconds (0),
                                              mapping.xToSeconds (getWidth()),
                                              gridResolution,
                                              theme::minGridSpacingPixels / mapping.pixelsPerSecond);
    const bool labelEveryBar = lines.empty() || (lines.size() < 2) || mapping.pixelsPerSecond * 2.0 > 40.0;

    for (const auto& line : lines)
    {
        const auto x = mapping.secondsToX (line.seconds);

        g.setColour (line.isBarStart ? theme::gridBar : theme::gridBeat);
        g.drawVerticalLine (x,
                            line.isBarStart ? static_cast<float> (getHeight() / 3)
                                            : static_cast<float> (getHeight() * 2 / 3),
                            static_cast<float> (getHeight()));

        if (line.isBarStart && labelEveryBar)
        {
            g.setColour (theme::textSecondary);
            g.drawText (juce::String (line.barNumber),
                        x + 3,
                        getHeight() / 3,
                        40,
                        getHeight() * 2 / 3,
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TimeRuler::mouseDown (const juce::MouseEvent& event)
{
    dragStartSeconds = secondsAt (event.x);
}

void TimeRuler::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragStartSeconds.has_value() || std::abs (event.getDistanceFromDragStartX()) < dragThresholdPixels)
        return;

    timeline.setLoopRange ({ *dragStartSeconds, secondsAt (event.x) });
    timeline.setLooping (true);
}

void TimeRuler::mouseUp (const juce::MouseEvent& event)
{
    if (dragStartSeconds.has_value() && std::abs (event.getDistanceFromDragStartX()) < dragThresholdPixels)
        transport.setPositionSeconds (*dragStartSeconds);

    dragStartSeconds.reset();
}

double TimeRuler::secondsAt (int x) const
{
    const auto seconds = juce::jmax (0.0, mapping.xToSeconds (x));
    return snapEnabled ? timeline.snapSeconds (seconds, gridResolution) : seconds;
}

} // namespace fk::ui
