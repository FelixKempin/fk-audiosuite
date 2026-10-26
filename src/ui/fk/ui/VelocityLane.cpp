#include "fk/ui/VelocityLane.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

VelocityLane::VelocityLane (services::MidiClipService& midiClipService)
    : midiClips (midiClipService)
{
    setTooltip (TRANS ("Velocity per note: drag the bars vertically"));
}

void VelocityLane::setClip (std::optional<services::MidiClipInfo> newClip)
{
    clip = std::move (newClip);
    drag.reset();
    repaint();
}

void VelocityLane::setSelectedNotes (const std::vector<int>& noteIndices)
{
    selected = { noteIndices.begin(), noteIndices.end() };
    repaint();
}

int VelocityLane::shownVelocity (const services::EditableNote& note) const
{
    if (! drag)
        return note.velocity;

    if (drag->noteIndex == note.index)
        return drag->velocity;

    if (drag->withSelection && selected.contains (note.index))
        return juce::jlimit (1, 127, note.velocity + drag->velocity - drag->originalVelocity);

    return note.velocity;
}

void VelocityLane::setPixelsPerBeat (double pixels)
{
    pixelsPerBeat = pixels;
    repaint();
}

void VelocityLane::setScrollX (int x)
{
    if (scrollX == x)
        return;

    scrollX = x;
    repaint();
}

int VelocityLane::xForBeat (double beat) const
{
    return static_cast<int> (std::round (beat * pixelsPerBeat)) - scrollX;
}

int VelocityLane::velocityForY (int y) const
{
    const auto usable = static_cast<double> (getHeight() - topMargin);
    return juce::jlimit (1, 127, static_cast<int> (std::round ((getHeight() - y) / usable * 127.0)));
}

int VelocityLane::yForVelocity (int velocity) const
{
    const auto usable = static_cast<double> (getHeight() - topMargin);
    return getHeight() - static_cast<int> (std::round (velocity / 127.0 * usable));
}

const services::EditableNote* VelocityLane::findNoteAt (juce::Point<int> position) const
{
    if (! clip)
        return nullptr;

    // Von hinten: spätere Noten liegen über früheren, wie im Raster.
    for (auto it = clip->notes.rbegin(); it != clip->notes.rend(); ++it)
    {
        const int x = xForBeat (it->startBeats);

        if (position.x >= x - 2 && position.x < x + barWidth + 2)
            return &*it;
    }

    return nullptr;
}

void VelocityLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundInput);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));

    // Hilfslinien bei 32, 64, 96, 127.
    for (const int velocity : { 32, 64, 96, 127 })
    {
        g.setColour (theme::gridBeat);
        g.drawHorizontalLine (yForVelocity (velocity), 0.0f, static_cast<float> (getWidth()));
    }

    g.setColour (theme::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (TRANS ("Velocity"), 4, 2, 60, 12, juce::Justification::centredLeft);

    if (! clip)
        return;

    for (const auto& note : clip->notes)
    {
        const bool dragging = drag && drag->noteIndex == note.index;
        const bool isSelected = selected.contains (note.index);
        const int velocity = shownVelocity (note);
        const int x = xForBeat (note.startBeats);
        const int y = yForVelocity (velocity);
        const auto base = isSelected ? theme::accent.brighter (0.35f) : theme::accent;

        g.setColour (dragging ? theme::textPrimary
                              : base.withAlpha (0.45f + 0.55f * static_cast<float> (velocity) / 127.0f));
        g.fillRect (x, y, barWidth, getHeight() - y);
        g.setColour (theme::textPrimary.withAlpha (0.7f));
        g.fillRect (x, y, barWidth, 2);

        if (dragging)
            g.drawText (juce::String (velocity), x + barWidth + 3, y - 7, 30, 14, juce::Justification::centredLeft);
    }
}

void VelocityLane::mouseDown (const juce::MouseEvent& event)
{
    const auto* note = findNoteAt (event.getPosition());

    if (note == nullptr)
        return;

    drag = Drag {
        note->index, note->velocity, velocityForY (event.y), selected.contains (note->index) && selected.size() > 1
    };
    repaint();
}

void VelocityLane::mouseDrag (const juce::MouseEvent& event)
{
    if (! drag)
        return;

    drag->velocity = velocityForY (event.y);
    repaint();
}

void VelocityLane::mouseUp (const juce::MouseEvent&)
{
    if (! drag || ! clip)
        return;

    // Erst alle Werte berechnen (shownVelocity braucht den laufenden Zug), dann in einem Schritt setzen.
    std::vector<services::EditableNote> changed;

    for (const auto& note : clip->notes)
        if (note.index == drag->noteIndex || (drag->withSelection && selected.contains (note.index)))
        {
            auto copy = note;
            copy.velocity = shownVelocity (note);
            changed.push_back (copy);
        }

    drag.reset();
    repaint();

    if (const auto result = midiClips.updateNotes (clip->id, changed, TRANS ("Change velocity"));
        ! result)
    {
        juce::Logger::writeToLog ("Velocity: " + services::errorText (result.error()));

        if (onError)
            onError (services::errorText (result.error()));
    }
}

} // namespace fk::ui
