#include "fk/ui/ArticulationLane.h"

#include "fk/ui/Theme.h"

#include <algorithm>

namespace fk::ui
{

ArticulationLane::ArticulationLane()
{
    setTooltip (TRANS ("Articulations: clicking in a row above a note sets that articulation (for all selected notes if there is a selection), clicking again removes it"));
}

void ArticulationLane::setMap (std::optional<engine::ExpressionMap> newMap)
{
    map = std::move (newMap);
    repaint();
}

void ArticulationLane::setClip (std::optional<services::MidiClipInfo> newClip)
{
    clip = std::move (newClip);
    repaint();
}

void ArticulationLane::setSelectedNotes (std::vector<int> noteIndices)
{
    selected = std::move (noteIndices);
    repaint();
}

void ArticulationLane::setPixelsPerBeat (double pixels)
{
    pixelsPerBeat = pixels;
    repaint();
}

void ArticulationLane::setScrollX (int x)
{
    scrollX = x;
    repaint();
}

void ArticulationLane::setHeaderWidth (int width)
{
    headerWidth = width;
    repaint();
}

int ArticulationLane::getPreferredHeight() const
{
    return hasMap() ? static_cast<int> (map->articulations.size()) * rowHeight + 2 : 0;
}

float ArticulationLane::xForBeats (double beats) const
{
    return static_cast<float> (headerWidth + beats * pixelsPerBeat - scrollX);
}

void ArticulationLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    if (! hasMap())
        return;

    const auto& articulations = map->articulations;

    // Notenblöcke zuerst (im Rasterbereich), dann die Namensspalte darüber.
    g.saveState();
    g.reduceClipRegion (headerWidth, 0, getWidth() - headerWidth, getHeight());

    for (std::size_t row = 0; row < articulations.size(); ++row)
    {
        const auto y = static_cast<float> (row) * rowHeight;
        g.setColour (theme::borderSubtle.withAlpha (0.5f));
        g.drawHorizontalLine (static_cast<int> (y + rowHeight), static_cast<float> (headerWidth), static_cast<float> (getWidth()));

        if (! clip)
            continue;

        for (const auto& note : clip->notes)
            if (note.articulation == articulations[row].id)
            {
                const bool isSelected = std::ranges::find (selected, note.index) != selected.end();
                const auto x = xForBeats (note.startBeats);
                const auto width = std::max (3.0f, static_cast<float> (note.lengthBeats * pixelsPerBeat) - 1.0f);
                g.setColour ((isSelected ? theme::accent.brighter (0.3f) : theme::accent).withAlpha (0.85f));
                g.fillRoundedRectangle (x, y + 2.0f, width, rowHeight - 4.0f, 2.0f);
            }
    }

    g.restoreState();

    g.setColour (theme::backgroundElevated);
    g.fillRect (0, 0, headerWidth, getHeight());
    g.setFont (juce::FontOptions (9.0f));

    for (std::size_t row = 0; row < articulations.size(); ++row)
    {
        const auto& articulation = articulations[row];
        g.setColour (theme::textSecondary);
        g.drawText ((articulation.symbol.isNotEmpty() ? articulation.symbol + " " : juce::String()) + articulation.name,
                    3, static_cast<int> (row) * rowHeight, headerWidth - 4, rowHeight, juce::Justification::centredLeft, true);
    }
}

void ArticulationLane::mouseDown (const juce::MouseEvent& event)
{
    if (! hasMap() || ! clip || event.x < headerWidth)
        return;

    const auto row = static_cast<std::size_t> (event.y / rowHeight);

    if (row >= map->articulations.size())
        return;

    const double beats = (event.x - headerWidth + scrollX) / pixelsPerBeat;
    const auto& id = map->articulations[row].id;

    // Note unter dem Zeiger; ist sie ausgewählt, gilt der Klick für die ganze Auswahl.
    const auto hit = std::ranges::find_if (clip->notes, [beats] (const services::EditableNote& note)
                                           { return beats >= note.startBeats && beats < note.startBeats + note.lengthBeats; });

    if (hit == clip->notes.end())
        return;

    std::vector<int> targets { hit->index };

    if (std::ranges::find (selected, hit->index) != selected.end())
        targets = selected;

    // Haben schon alle Ziele diese Spielweise, nimmt der Klick sie weg.
    const bool allHaveIt = std::ranges::all_of (targets, [this, &id] (int index)
                                                {
                                                    const auto note = std::ranges::find (clip->notes, index, &services::EditableNote::index);
                                                    return note != clip->notes.end() && note->articulation == id;
                                                });

    if (onSet)
        onSet (targets, allHaveIt ? juce::String() : id);
}

} // namespace fk::ui
