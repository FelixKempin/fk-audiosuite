#include "fk/ui/NoteDrag.h"

#include <algorithm>
#include <cmath>

namespace fk::ui
{

namespace
{
double snapped (double beat, double gridBeats)
{
    return std::round (beat / gridBeats) * gridBeats;
}
} // namespace

void NoteDrag::beginEdit (Mode newMode, const std::vector<services::EditableNote>& notes, int anchorIndex, bool asCopy)
{
    *this = {};
    mode = newMode;
    copy = asCopy;
    anchor = anchorIndex;

    for (const auto& note : notes)
    {
        originals.emplace (note.index, note);
        previews.emplace (note.index, note);
    }
}

void NoteDrag::beginDraw (services::EditableNote note)
{
    *this = {};
    mode = Mode::draw;
    drawn = note;
    drawnMinimumLength = note.lengthBeats;
}

void NoteDrag::beginLasso (juce::Point<int> start)
{
    *this = {};
    mode = Mode::lasso;
    lassoStart = start;
    lasso = { start, start };
}

void NoteDrag::update (juce::Point<int> distance, juce::Point<int> position, const Scale& scale)
{
    const double deltaBeats = distance.x / scale.pixelsPerBeat;
    moved = moved || std::abs (distance.x) > moveThresholdPixels || std::abs (distance.y) > moveThresholdPixels;

    switch (mode)
    {
        case Mode::move:
        {
            // Die gegriffene Note rastet ein, alle anderen gehen um denselben Abstand mit; keine
            // Note rutscht vor den Clip-Anfang oder aus dem Tonumfang.
            const auto& grabbed = originals.at (anchor);
            double beatShift = snapped (grabbed.startBeats + deltaBeats, scale.gridBeats) - grabbed.startBeats;
            int semitones = -distance.y / scale.rowHeight;

            for (const auto& [index, original] : originals)
            {
                beatShift = std::max (beatShift, -original.startBeats);
                semitones = std::clamp (semitones, -original.noteNumber, scale.numNotes - 1 - original.noteNumber);
            }

            for (auto& [index, preview] : previews)
            {
                preview.startBeats = originals.at (index).startBeats + beatShift;
                preview.noteNumber = originals.at (index).noteNumber + semitones;
            }

            break;
        }

        case Mode::trimStart:
            for (auto& [index, preview] : previews)
            {
                const auto& original = originals.at (index);
                const double end = original.startBeats + original.lengthBeats;
                const double start = std::clamp (snapped (original.startBeats + deltaBeats, scale.gridBeats),
                                                 0.0,
                                                 end - services::MidiClipService::minNoteLengthBeats);
                preview.startBeats = start;
                preview.lengthBeats = end - start;
            }
            break;

        case Mode::trimEnd:
            for (auto& [index, preview] : previews)
            {
                const auto& original = originals.at (index);
                const double end = snapped (original.startBeats + original.lengthBeats + deltaBeats, scale.gridBeats);
                preview.lengthBeats = std::max (scale.gridBeats, end - original.startBeats);
            }
            break;

        case Mode::velocity:
        {
            const int change = -distance.y / pixelsPerVelocityStep;

            for (auto& [index, preview] : previews)
                preview.velocity = std::clamp (originals.at (index).velocity + change, 1, 127);

            break;
        }

        case Mode::draw:
        {
            const double end = snapped (position.x / scale.pixelsPerBeat, scale.gridBeats);
            drawn.lengthBeats = std::max (drawnMinimumLength, end - drawn.startBeats);
            break;
        }

        case Mode::lasso:
            lasso = juce::Rectangle<int> (lassoStart, position);
            break;

        case Mode::none:
            break;
    }
}

} // namespace fk::ui
