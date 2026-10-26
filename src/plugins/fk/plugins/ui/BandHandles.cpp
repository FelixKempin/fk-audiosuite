#include "fk/plugins/ui/BandHandles.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

BandHandles::BandHandles()
{
    setTooltip (TRANS ("Drag band: frequency and gain. Mouse wheel: Q. Double-click: gain to 0 dB"));
}

BandHandles::~BandHandles() = default;

void BandHandles::addHandle (juce::RangedAudioParameter& frequency,
                             juce::RangedAudioParameter* gain,
                             juce::RangedAudioParameter* q,
                             juce::Colour colour,
                             juce::String label)
{
    auto handle = std::make_unique<Handle> (frequency, gain, q, colour, std::move (label));
    auto* raw = handle.get();

    // ParameterAttachment ruft den Callback bei jeder Änderung (auch aus Automation und Undo)
    // im Message-Thread; sendInitialUpdate() liefert sofort den aktuellen Wert.
    handle->frequencyAttachment = std::make_unique<juce::ParameterAttachment> (frequency,
                                                                               [this, raw] (float value)
                                                                               {
                                                                                   raw->frequencyHz = value;
                                                                                   changed();
                                                                               });
    handle->frequencyAttachment->sendInitialUpdate();

    if (gain != nullptr)
    {
        handle->gainAttachment = std::make_unique<juce::ParameterAttachment> (*gain,
                                                                              [this, raw] (float value)
                                                                              {
                                                                                  raw->gainDb = value;
                                                                                  changed();
                                                                              });
        handle->gainAttachment->sendInitialUpdate();
    }

    if (q != nullptr)
    {
        handle->qAttachment = std::make_unique<juce::ParameterAttachment> (*q,
                                                                           [this, raw] (float value)
                                                                           {
                                                                               raw->qValue = value;
                                                                               changed();
                                                                           });
        handle->qAttachment->sendInitialUpdate();
    }

    handles.push_back (std::move (handle));
}

void BandHandles::setScale (const FrequencyScale& newScale)
{
    scale = newScale;
    repaint();
}

int BandHandles::getNumHandles() const noexcept
{
    return static_cast<int> (handles.size());
}

juce::Point<float> BandHandles::getHandleCentre (int index) const
{
    const auto& handle = *handles[static_cast<std::size_t> (index)];
    return { scale.xForHz (handle.frequencyHz), scale.yForDb (handle.gain != nullptr ? handle.gainDb : 0.0f) };
}

void BandHandles::changed()
{
    repaint();

    if (onChanged)
        onChanged();
}

int BandHandles::handleAt (juce::Point<float> position) const
{
    for (int i = getNumHandles() - 1; i >= 0; --i)
        if (getHandleCentre (i).getDistanceFrom (position) <= radius * 1.6f)
            return i;

    return -1;
}

void BandHandles::paint (juce::Graphics& g)
{
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));

    for (int i = 0; i < getNumHandles(); ++i)
    {
        const auto& handle = *handles[static_cast<std::size_t> (i)];
        const auto centre = getHandleCentre (i);
        const bool active = i == draggedHandle || i == hoveredHandle;
        const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        g.setColour (handle.colour.withAlpha (active ? 1.0f : 0.85f));
        g.fillEllipse (circle);
        g.setColour (theme::backgroundBase);
        g.drawEllipse (circle, 1.5f);
        g.setColour (theme::backgroundBase);
        g.drawText (handle.label, circle, juce::Justification::centred);
    }
}

void BandHandles::mouseMove (const juce::MouseEvent& event)
{
    const int hit = handleAt (event.position);

    if (hit != hoveredHandle)
    {
        hoveredHandle = hit;
        setMouseCursor (hit >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void BandHandles::mouseDown (const juce::MouseEvent& event)
{
    draggedHandle = handleAt (event.position);

    if (draggedHandle < 0)
        return;

    // Geste beginnen: Der Host (Automation, Undo) fasst alle Änderungen bis mouseUp zusammen.
    auto& handle = *handles[static_cast<std::size_t> (draggedHandle)];
    handle.frequencyAttachment->beginGesture();

    if (handle.gainAttachment)
        handle.gainAttachment->beginGesture();
}

void BandHandles::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedHandle < 0)
        return;

    auto& handle = *handles[static_cast<std::size_t> (draggedHandle)];
    handle.frequencyAttachment->setValueAsPartOfGesture (scale.hzForX (event.position.x));

    if (handle.gainAttachment)
        handle.gainAttachment->setValueAsPartOfGesture (scale.dbForY (event.position.y));
}

void BandHandles::mouseUp (const juce::MouseEvent&)
{
    if (draggedHandle < 0)
        return;

    auto& handle = *handles[static_cast<std::size_t> (draggedHandle)];
    handle.frequencyAttachment->endGesture();

    if (handle.gainAttachment)
        handle.gainAttachment->endGesture();

    draggedHandle = -1;
    repaint();
}

void BandHandles::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int hit = handleAt (event.position);

    if (hit < 0)
        return;

    auto& handle = *handles[static_cast<std::size_t> (hit)];

    if (handle.gainAttachment && handle.gain != nullptr)
        handle.gainAttachment->setValueAsCompleteGesture (
            handle.gain->convertFrom0to1 (handle.gain->getDefaultValue()));
}

void BandHandles::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const int hit = draggedHandle >= 0 ? draggedHandle : handleAt (event.position);

    if (hit < 0)
        return;

    auto& handle = *handles[static_cast<std::size_t> (hit)];

    if (! handle.qAttachment || handle.q == nullptr)
        return;

    // Q multiplikativ ändern: Ein Rastschritt sind etwa 10 Prozent, in beide Richtungen gleich fühlbar.
    const float factor = std::pow (1.1f, wheel.deltaY * 10.0f);
    const auto range = handle.q->getNormalisableRange();
    handle.qAttachment->setValueAsCompleteGesture (juce::jlimit (range.start, range.end, handle.qValue * factor));
}

} // namespace fk::plugins::ui
