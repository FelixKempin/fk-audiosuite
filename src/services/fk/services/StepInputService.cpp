#include "fk/services/StepInputService.h"

namespace fk::services
{

StepInputService::StepInputService (ProjectService& project, MidiClipService& clips)
    : projectService (project)
    , midiClips (clips)
{
    dispatcher->listeners.add (this);
}

StepInputService::~StepInputService()
{
    dispatcher->listeners.remove (this);
}

core::Result<void> StepInputService::start (const juce::String& newClipId, double startBeats, double newStepBeats)
{
    const auto clip = midiClips.getClip (newClipId);

    if (! clip)
        return std::unexpected (clip.error());

    clipId = newClipId;
    trackId = clip->trackId;
    stepBeats = std::max (newStepBeats, MidiClipService::minNoteLengthBeats);
    positionBeats = std::clamp (startBeats, 0.0, clip->lengthBeats);
    heldKeys = 0;
    notify();
    return {};
}

void StepInputService::stop()
{
    clipId.clear();
    trackId.clear();
    heldKeys = 0;
    notify();
}

void StepInputService::setStepBeats (double beats)
{
    stepBeats = std::max (beats, MidiClipService::minNoteLengthBeats);
}

void StepInputService::setPositionBeats (double beats)
{
    positionBeats = std::max (0.0, beats);
    notify();
}

void StepInputService::advance (int steps)
{
    positionBeats = std::max (0.0, positionBeats + steps * stepBeats);
    notify();
}

void StepInputService::keysChanged (const std::vector<int>& notesOn,
                                    const std::vector<int>& velocities,
                                    const std::vector<int>& notesOff)
{
    if (! isActive())
        return;

    if (! notesOn.empty())
    {
        // Erste Taste eines Akkords: Stelle merken und weiterrücken; weitere, solange gehalten, auf dieselbe Stelle.
        if (heldKeys == 0)
        {
            chordPositionBeats = positionBeats;
            positionBeats += stepBeats;
        }

        std::vector<EditableNote> notes;

        for (std::size_t i = 0; i < notesOn.size(); ++i)
            notes.push_back ({ 0, notesOn[i], chordPositionBeats, stepBeats, i < velocities.size() ? velocities[i] : 100 });

        if (const auto added = midiClips.addNotes (clipId, notes, TRANS ("Step input")); ! added)
            juce::Logger::writeToLog ("Step-Eingabe: " + juce::String (added.error().message));

        heldKeys += static_cast<int> (notesOn.size());
    }

    heldKeys = std::max (0, heldKeys - static_cast<int> (notesOff.size()));
    notify();
}

void StepInputService::midiKeyStateChanged (te::AudioTrack* track,
                                            const juce::Array<int>& notesOn,
                                            const juce::Array<int>& velocities,
                                            const juce::Array<int>& notesOff)
{
    if (! isActive() || track == nullptr || track->itemID.toString() != trackId)
        return;

    keysChanged ({ notesOn.begin(), notesOn.end() }, { velocities.begin(), velocities.end() }, { notesOff.begin(), notesOff.end() });
}

void StepInputService::notify()
{
    if (onChanged)
        onChanged();
}

} // namespace fk::services
