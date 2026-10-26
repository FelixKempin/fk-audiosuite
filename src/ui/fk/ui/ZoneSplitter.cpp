#include "fk/ui/ZoneSplitter.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

ZoneSplitter::ZoneSplitter()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip (TRANS ("Drag: make the lower zone larger or smaller, double-click: default height"));
}

void ZoneSplitter::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    g.setColour (isMouseOverOrDragging() ? theme::accent : theme::borderStrong);
    const auto centre = getLocalBounds().getCentre();
    g.fillRoundedRectangle (juce::Rectangle<float> (40.0f, 2.0f).withCentre (centre.toFloat()), 1.0f);
}

void ZoneSplitter::mouseDown (const juce::MouseEvent&)
{
    if (onDragStarted)
        onDragStarted();
}

void ZoneSplitter::mouseDrag (const juce::MouseEvent& event)
{
    if (onDragged)
        onDragged (event.getDistanceFromDragStartY());
}

void ZoneSplitter::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onReset)
        onReset();
}

} // namespace fk::ui
