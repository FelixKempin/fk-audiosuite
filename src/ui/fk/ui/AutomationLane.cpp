#include "fk/ui/AutomationLane.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

AutomationLane::AutomationLane (services::AutomationService& automationService,
                                services::TimelineService& timelineService,
                                services::TrackHandle& automatedTrack,
                                int index,
                                juce::Colour trackColour)
    : automation (automationService)
    , timeline (timelineService)
    , track (automatedTrack)
    , laneIndex (index)
    , colour (trackColour)
{
    setTooltip (TRANS ("Click: add a point. Drag: move. Double-click or right-click: remove"));
    automation.addListener (*this);
    refresh();
}

AutomationLane::~AutomationLane()
{
    automation.removeListener (*this);
}

void AutomationLane::setSettings (LaneSettings newSettings)
{
    settings = newSettings;
    repaint();
}

void AutomationLane::refresh()
{
    parameter = automation.getLaneParameter (track, laneIndex);
    points = parameter ? automation.getPoints (track, parameter->id) : std::vector<services::AutomationPointInfo> {};
    repaint();
}

void AutomationLane::automationChanged()
{
    refresh();
}

float AutomationLane::yForValue (float value) const
{
    if (! parameter || parameter->maxValue <= parameter->minValue)
        return static_cast<float> (getHeight()) * 0.5f;

    const auto proportion = (value - parameter->minValue) / (parameter->maxValue - parameter->minValue);
    const auto usable = static_cast<float> (getHeight() - verticalMargin * 2);
    return static_cast<float> (getHeight() - verticalMargin) - proportion * usable;
}

float AutomationLane::valueForY (float y) const
{
    if (! parameter)
        return 0.0f;

    const auto usable = static_cast<float> (getHeight() - verticalMargin * 2);
    const auto proportion = juce::jlimit (0.0f, 1.0f, (static_cast<float> (getHeight() - verticalMargin) - y) / usable);
    return parameter->minValue + proportion * (parameter->maxValue - parameter->minValue);
}

double AutomationLane::secondsForX (int x) const
{
    const auto seconds = juce::jmax (0.0, settings.mapping.xToSeconds (x));
    return settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;
}

int AutomationLane::findPointAt (juce::Point<float> position) const
{
    for (int i = static_cast<int> (points.size()) - 1; i >= 0; --i)
    {
        const auto& point = points[static_cast<std::size_t> (i)];
        const juce::Point<float> centre (static_cast<float> (settings.mapping.secondsToX (point.seconds)),
                                         yForValue (point.value));

        if (centre.getDistanceFrom (position) <= pointRadius * 1.8f)
            return i;
    }

    return -1;
}

void AutomationLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundInput);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

    if (! parameter)
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (TRANS ("Choose a parameter on the left, then click here to set points"),
                    getLocalBounds().reduced (theme::spacingMedium, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // Kurve: vor dem ersten Punkt und nach dem letzten gilt dessen Wert; ohne Punkte der aktuelle Wert.
    juce::Path curve;
    const auto right = static_cast<float> (getWidth());

    if (points.empty())
    {
        const auto y = yForValue (parameter->currentValue);
        curve.startNewSubPath (0.0f, y);
        curve.lineTo (right, y);
    }
    else
    {
        curve.startNewSubPath (0.0f, yForValue (points.front().value));

        for (const auto& point : points)
            curve.lineTo (static_cast<float> (settings.mapping.secondsToX (point.seconds)), yForValue (point.value));

        curve.lineTo (right, yForValue (points.back().value));
    }

    juce::Path fill (curve);
    fill.lineTo (right, static_cast<float> (getHeight()));
    fill.lineTo (0.0f, static_cast<float> (getHeight()));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    for (int i = 0; i < static_cast<int> (points.size()); ++i)
    {
        const auto& point = points[static_cast<std::size_t> (i)];
        const auto x = static_cast<float> (settings.mapping.secondsToX (point.seconds));
        const auto y = yForValue (point.value);
        const bool active = i == draggedPoint || i == hoveredPoint;
        const auto circle = juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f).withCentre ({ x, y });

        g.setColour (active ? theme::textPrimary : colour);
        g.fillEllipse (circle);
        g.setColour (theme::backgroundInput);
        g.drawEllipse (circle, 1.0f);

        if (i == draggedPoint)
        {
            g.setColour (theme::textPrimary);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (automation.getValueText (track, parameter->id, point.value),
                        juce::Rectangle<float> (x + pointRadius + 2.0f, y - 16.0f, 90.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (theme::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (parameter->ownerName.isEmpty() ? parameter->name : parameter->ownerName + ": " + parameter->name,
                getLocalBounds().reduced (theme::spacingSmall, 2),
                juce::Justification::topRight);
}

void AutomationLane::mouseMove (const juce::MouseEvent& event)
{
    const int hit = findPointAt (event.position);

    if (hit != hoveredPoint)
    {
        hoveredPoint = hit;
        setMouseCursor (hit >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void AutomationLane::mouseDown (const juce::MouseEvent& event)
{
    if (onSelected)
        onSelected();

    if (! parameter)
        return;

    const int hit = findPointAt (event.position);

    if (event.mods.isPopupMenu())
    {
        if (hit >= 0)
            removePointAt (hit);

        return;
    }

    if (hit >= 0)
    {
        automation.beginChange (TRANS ("Move automation point"));
        draggedPoint = hit;
        repaint();
        return;
    }

    const auto added = automation.addPoint (track, parameter->id, secondsForX (event.x), valueForY (event.position.y));

    if (! added)
    {
        reportError (added.error());
        return;
    }

    draggedPoint = *added;
}

void AutomationLane::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedPoint < 0 || ! parameter)
        return;

    const auto moved =
        automation.movePoint (track, parameter->id, draggedPoint, secondsForX (event.x), valueForY (event.position.y));

    if (moved)
        draggedPoint = *moved;
    else
        reportError (moved.error());
}

void AutomationLane::mouseUp (const juce::MouseEvent&)
{
    draggedPoint = -1;
    repaint();
}

void AutomationLane::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int hit = findPointAt (event.position);

    if (hit >= 0)
        removePointAt (hit);
}

void AutomationLane::removePointAt (int index)
{
    if (! parameter)
        return;

    draggedPoint = -1;
    hoveredPoint = -1;

    if (const auto result = automation.removePoint (track, parameter->id, index); ! result)
        reportError (result.error());
}

void AutomationLane::reportError (const core::Error& error) const
{
    juce::Logger::writeToLog ("Automation fehlgeschlagen: " + services::errorText (error));
}

} // namespace fk::ui
