#include "fk/ui/NoteEditing.h"

#include "fk/services/ErrorText.h"

#include <cmath>

namespace fk::ui
{

namespace
{
// Dieselbe Note nach einer Änderung wiederfinden (der Index kann sich verschoben haben).
bool isSameNote (const services::EditableNote& a, const services::EditableNote& b)
{
    constexpr double tolerance = 1.0e-4;
    return a.noteNumber == b.noteNumber && std::abs (a.startBeats - b.startBeats) < tolerance &&
           std::abs (a.lengthBeats - b.lengthBeats) < tolerance;
}
} // namespace

NoteEditing::NoteEditing (services::MidiClipService& midiClipService)
    : midiClips (midiClipService)
{
}

void NoteEditing::setClip (std::optional<services::MidiClipInfo> newClip)
{
    const bool sameClip = clip.has_value() && newClip.has_value() && clip->id == newClip->id;
    clip = std::move (newClip);
    std::set<int> next;

    if (clip.has_value() && ! expectedAfterEdit.empty())
    {
        for (const auto& expected : expectedAfterEdit)
            for (const auto& note : clip->notes)
                if (! next.contains (note.index) && isSameNote (note, expected))
                {
                    next.insert (note.index);
                    break;
                }
    }
    else if (clip.has_value() && sameClip)
    {
        for (const int index : selected)
            if (index < static_cast<int> (clip->notes.size()))
                next.insert (index);
    }

    expectedAfterEdit.clear();
    setSelection (std::move (next));
}

const std::optional<services::MidiClipInfo>& NoteEditing::getClip() const noexcept
{
    return clip;
}

bool NoteEditing::isSelected (int noteIndex) const
{
    return selected.contains (noteIndex);
}

const std::set<int>& NoteEditing::getSelection() const noexcept
{
    return selected;
}

std::vector<services::EditableNote> NoteEditing::getSelectedNotes() const
{
    std::vector<services::EditableNote> notes;

    if (clip.has_value())
        for (const auto& note : clip->notes)
            if (selected.contains (note.index))
                notes.push_back (note);

    return notes;
}

void NoteEditing::setSelection (std::set<int> indices)
{
    if (indices == selected)
        return;

    selected = std::move (indices);

    if (onSelectionChanged)
        onSelectionChanged();
}

void NoteEditing::selectOnly (int noteIndex)
{
    setSelection ({ noteIndex });
}

void NoteEditing::toggle (int noteIndex)
{
    auto next = selected;

    if (! next.erase (noteIndex))
        next.insert (noteIndex);

    setSelection (std::move (next));
}

void NoteEditing::add (int noteIndex)
{
    auto next = selected;
    next.insert (noteIndex);
    setSelection (std::move (next));
}

void NoteEditing::selectAll()
{
    std::set<int> all;

    if (clip.has_value())
        for (const auto& note : clip->notes)
            all.insert (note.index);

    setSelection (std::move (all));
}

void NoteEditing::addNotes (const std::vector<services::EditableNote>& notes, const juce::String& actionName)
{
    if (! clip.has_value() || notes.empty())
        return;

    expectedAfterEdit = notes;
    handle (midiClips.addNotes (clip->id, notes, actionName));
}

void NoteEditing::updateNotes (const std::vector<services::EditableNote>& notes, const juce::String& actionName)
{
    if (! clip.has_value() || notes.empty())
        return;

    expectedAfterEdit = notes;
    handle (midiClips.updateNotes (clip->id, notes, actionName));
}

void NoteEditing::removeNotes (const std::vector<int>& noteIndices)
{
    if (! clip.has_value() || noteIndices.empty())
        return;

    handle (midiClips.removeNotes (clip->id, noteIndices));
}

void NoteEditing::removeSelection()
{
    removeNotes ({ selected.begin(), selected.end() });
}

void NoteEditing::changeSelection (const std::function<void (services::EditableNote&)>& change,
                                   const juce::String& actionName)
{
    auto notes = getSelectedNotes();

    for (auto& note : notes)
        change (note);

    updateNotes (notes, actionName);
}

void NoteEditing::addChord (const std::vector<int>& noteNumbers, double startBeats, double lengthBeats, int velocity)
{
    if (clip.has_value())
        handle (midiClips.addChord (clip->id, noteNumbers, startBeats, lengthBeats, velocity));
}

void NoteEditing::handle (const core::Result<void>& result)
{
    if (result)
        return;

    expectedAfterEdit.clear();
    juce::Logger::writeToLog ("Key Editor: " + services::errorText (result.error()));

    if (onError)
        onError (services::errorText (result.error()));
}

} // namespace fk::ui
