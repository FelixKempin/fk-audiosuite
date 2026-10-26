#include "fk/services/MidiClipService.h"

#include "fk/engine/ExpressionMap.h"
#include "fk/services/BakeLock.h"

#include <algorithm>
#include <cmath>

namespace fk::services
{

namespace
{
constexpr int defaultColourIndex = 0;
} // namespace

MidiClipService::MidiClipService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

MidiClipService::~MidiClipService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

core::Result<juce::String>
MidiClipService::createEmptyClip (TrackHandle& track, double startSeconds, double lengthSeconds)
{
    if (projectService.getEdit() == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (lengthSeconds <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The clip length must be positive.").toStdString());

    if (auto unlocked = bakeLock::checkClipsUnlocked (track); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Add MIDI clip"));

    const te::TimeRange range { te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds)),
                                te::TimeDuration::fromSeconds (lengthSeconds) };
    auto clip = track.insertMIDIClip ("MIDI", range, nullptr);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the MIDI clip.").toStdString());

    return clip->itemID.toString();
}

core::Result<MidiClipInfo> MidiClipService::getClip (const juce::String& clipId) const
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    MidiClipInfo info;
    info.id = clipId;
    info.name = clip->getName();

    if (auto* track = clip->getClipTrack())
        info.trackId = track->itemID.toString();

    // Clip-Länge in Schlägen: Start und Ende des Clips über die Tempospur in Beats umrechnen.
    const auto range = clip->getEditTimeRange();
    const auto& tempo = clip->edit.tempoSequence;
    info.lengthBeats = (tempo.toBeats (range.getEnd()) - tempo.toBeats (range.getStart())).inBeats();
    info.startSeconds = range.getStart().inSeconds();

    int index = 0;

    for (auto* note : clip->getSequence().getNotes())
    {
        info.notes.push_back ({ index,
                                note->getNoteNumber(),
                                note->getStartBeat().inBeats(),
                                note->getLengthBeats().inBeats(),
                                note->getVelocity(),
                                note->state.getProperty (engine::noteArticulationProperty).toString() });
        ++index;
    }

    index = 0;

    for (auto* event : clip->getSequence().getControllerEvents())
    {
        info.controllers.push_back (
            { index, event->getType(), event->getBeatPosition().inBeats(), event->getControllerValue() });
        ++index;
    }

    return info;
}

core::Result<void> MidiClipService::addNote (
    const juce::String& clipId, int noteNumber, double startBeats, double lengthBeats, int velocity)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    if (noteNumber < 0 || noteNumber > 127 || lengthBeats <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Invalid note.").toStdString());

    projectService.beginUndoStep (TRANS ("Insert note"));
    clip->getSequence().addNote (noteNumber,
                                 te::BeatPosition::fromBeats (juce::jmax (0.0, startBeats)),
                                 te::BeatDuration::fromBeats (lengthBeats),
                                 juce::jlimit (1, 127, velocity),
                                 defaultColourIndex,
                                 &clip->edit.getUndoManager());
    return {};
}

core::Result<void> MidiClipService::addChord (const juce::String& clipId,
                                              const std::vector<int>& noteNumbers,
                                              double startBeats,
                                              double lengthBeats,
                                              int velocity)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    if (noteNumbers.empty() || lengthBeats <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Empty chord.").toStdString());

    projectService.beginUndoStep (TRANS ("Insert chord"));

    for (const int noteNumber : noteNumbers)
        if (noteNumber >= 0 && noteNumber <= 127)
            clip->getSequence().addNote (noteNumber,
                                         te::BeatPosition::fromBeats (juce::jmax (0.0, startBeats)),
                                         te::BeatDuration::fromBeats (lengthBeats),
                                         juce::jlimit (1, 127, velocity),
                                         defaultColourIndex,
                                         &clip->edit.getUndoManager());

    return {};
}

core::Result<void> MidiClipService::removeNote (const juce::String& clipId, int noteIndex)
{
    return removeNotes (clipId, { noteIndex });
}

core::Result<void>
MidiClipService::moveNote (const juce::String& clipId, int noteIndex, int newNoteNumber, double newStartBeats)
{
    return changeNote (clipId,
                       noteIndex,
                       TRANS ("Move note"),
                       [newNoteNumber, newStartBeats] (EditableNote& note)
                       {
                           note.noteNumber = newNoteNumber;
                           note.startBeats = newStartBeats;
                       });
}

core::Result<void> MidiClipService::setNoteLength (const juce::String& clipId, int noteIndex, double newLengthBeats)
{
    return changeNote (clipId,
                       noteIndex,
                       TRANS ("Change note length"),
                       [newLengthBeats] (EditableNote& note) { note.lengthBeats = newLengthBeats; });
}

core::Result<void> MidiClipService::setNoteVelocity (const juce::String& clipId, int noteIndex, int velocity)
{
    return changeNote (clipId,
                       noteIndex,
                       TRANS ("Change velocity"),
                       [velocity] (EditableNote& note) { note.velocity = velocity; });
}

core::Result<void> MidiClipService::changeNote (const juce::String& clipId,
                                                int noteIndex,
                                                const juce::String& actionName,
                                                const std::function<void (EditableNote&)>& change)
{
    const auto clip = getClip (clipId);

    if (! clip)
        return std::unexpected (clip.error());

    if (noteIndex < 0 || noteIndex >= static_cast<int> (clip->notes.size()))
        return core::fail (core::ErrorCode::notFound, TRANS ("Note not found.").toStdString());

    auto note = clip->notes[static_cast<std::size_t> (noteIndex)];
    change (note);
    return updateNotes (clipId, { note }, actionName);
}

core::Result<void>
MidiClipService::quantise (const juce::String& clipId, double gridBeats, const std::vector<int>& noteIndices)
{
    QuantiseSettings settings;
    settings.gridBeats = gridBeats;
    return quantise (clipId, settings, noteIndices);
}

double MidiClipService::quantisedStart (double startBeats, const QuantiseSettings& settings)
{
    const double grid = settings.gridBeats;
    const auto step = static_cast<long long> (std::llround (startBeats / grid));
    double target = static_cast<double> (step) * grid;

    // Swing: jeder zweite Schritt nach hinten, bis zu einem Drittel des Schritts (Triolen-Gefühl).
    if (step % 2 != 0)
        target += std::clamp (settings.swing, 0.0, 1.0) * grid / 3.0;

    if (! settings.groove.empty())
        target += settings.groove[static_cast<std::size_t> (step % static_cast<long long> (settings.groove.size()))];

    target = std::max (0.0, target);
    return startBeats + (target - startBeats) * std::clamp (settings.strength, 0.0, 1.0);
}

core::Result<void>
MidiClipService::quantise (const juce::String& clipId, const QuantiseSettings& settings, const std::vector<int>& noteIndices)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    if (settings.gridBeats <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The grid must be positive.").toStdString());

    std::vector<te::MidiNote*> notes;

    if (noteIndices.empty())
        for (auto* note : clip->getSequence().getNotes())
            notes.push_back (note);
    else if (auto found = findNotes (*clip, noteIndices))
        notes = *found;
    else
        return std::unexpected (found.error());

    projectService.beginUndoStep (TRANS ("Quantize"));
    auto& undoManager = clip->edit.getUndoManager();

    for (auto* note : notes)
        note->setStartAndLength (te::BeatPosition::fromBeats (quantisedStart (note->getStartBeat().inBeats(), settings)),
                                 note->getLengthBeats(),
                                 &undoManager);

    return {};
}

core::Result<std::vector<double>>
MidiClipService::extractGroove (const juce::String& clipId, double gridBeats, int stepsPerCycle) const
{
    const auto clip = getClip (clipId);

    if (! clip)
        return std::unexpected (clip.error());

    if (gridBeats <= 0.0 || stepsPerCycle < 1)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Grid and steps must be positive.").toStdString());

    std::vector<double> sums (static_cast<std::size_t> (stepsPerCycle), 0.0);
    std::vector<int> counts (static_cast<std::size_t> (stepsPerCycle), 0);

    for (const auto& note : clip->notes)
    {
        const auto step = static_cast<long long> (std::llround (note.startBeats / gridBeats));
        const auto slot = static_cast<std::size_t> (step % stepsPerCycle);
        sums[slot] += note.startBeats - static_cast<double> (step) * gridBeats;
        ++counts[slot];
    }

    if (std::ranges::all_of (counts, [] (int count) { return count == 0; }))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The clip has no notes to extract a groove from.").toStdString());

    std::vector<double> groove;

    for (std::size_t i = 0; i < sums.size(); ++i)
        groove.push_back (counts[i] > 0 ? sums[i] / counts[i] : 0.0);

    return groove;
}

std::vector<std::pair<juce::String, juce::String>> MidiClipService::listClips() const
{
    std::vector<std::pair<juce::String, juce::String>> clips;

    if (const auto* edit = projectService.getEdit())
        for (auto* track : te::getAudioTracks (*edit))
            for (auto* clip : track->getClips())
                if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                    clips.emplace_back (clip->itemID.toString(), track->getName() + ": " + clip->getName());

    return clips;
}

juce::String MidiClipService::functionName (MidiFunction function)
{
    switch (function)
    {
        case MidiFunction::fixedLength:
            return TRANS ("Fixed length");
        case MidiFunction::legato:
            return TRANS ("Legato");
        case MidiFunction::transpose:
            return TRANS ("Transpose");
        case MidiFunction::humanizeTiming:
            return TRANS ("Humanize timing");
        case MidiFunction::humanizeVelocity:
            return TRANS ("Humanize velocity");
        case MidiFunction::fixedVelocity:
            return TRANS ("Fixed velocity");
        case MidiFunction::scaleVelocity:
            return TRANS ("Scale velocity");
        case MidiFunction::deleteDoubles:
            return TRANS ("Delete duplicate notes");
    }

    return {};
}

std::vector<EditableNote>
MidiClipService::transform (std::vector<EditableNote> notes, const MidiFunctionParams& params, double clipLengthBeats)
{
    juce::Random random (params.randomSeed);
    const auto symmetric = [&random] (double range) { return (random.nextDouble() * 2.0 - 1.0) * range; };

    switch (params.function)
    {
        case MidiFunction::fixedLength:
            for (auto& note : notes)
                note.lengthBeats = std::max (minNoteLengthBeats, params.beats);
            break;

        case MidiFunction::legato:
        {
            // Nach Anfang sortiert; Akkorde (gleicher Anfang) reichen gemeinsam bis zum nächsten Anfang.
            std::vector<double> starts;

            for (const auto& note : notes)
                starts.push_back (note.startBeats);

            std::ranges::sort (starts);

            for (auto& note : notes)
            {
                const auto next = std::ranges::upper_bound (starts, note.startBeats + 1.0e-9);
                const double end = next != starts.end() ? *next : std::max (note.startBeats + note.lengthBeats, clipLengthBeats);
                note.lengthBeats = std::max (minNoteLengthBeats, end - note.startBeats);
            }

            break;
        }

        case MidiFunction::transpose:
            for (auto& note : notes)
                note.noteNumber = std::clamp (note.noteNumber + params.semitones, 0, 127);
            break;

        case MidiFunction::humanizeTiming:
            for (auto& note : notes)
                note.startBeats = std::max (0.0, note.startBeats + symmetric (params.amount));
            break;

        case MidiFunction::humanizeVelocity:
            for (auto& note : notes)
                note.velocity = std::clamp (note.velocity + juce::roundToInt (symmetric (params.amount)), 1, 127);
            break;

        case MidiFunction::fixedVelocity:
            for (auto& note : notes)
                note.velocity = std::clamp (params.velocity, 1, 127);
            break;

        case MidiFunction::scaleVelocity:
            for (auto& note : notes)
                note.velocity = std::clamp (juce::roundToInt (note.velocity * params.amount), 1, 127);
            break;

        case MidiFunction::deleteDoubles:
        {
            std::vector<EditableNote> kept;

            for (const auto& note : notes)
            {
                const bool duplicate = std::ranges::any_of (kept,
                                                            [&note] (const EditableNote& other)
                                                            {
                                                                return other.noteNumber == note.noteNumber
                                                                       && std::abs (other.startBeats - note.startBeats) < 1.0 / 128.0;
                                                            });

                if (! duplicate)
                    kept.push_back (note);
            }

            return kept;
        }
    }

    return notes;
}

core::Result<void> MidiClipService::setArticulation (const juce::String& clipId,
                                                     const std::vector<int>& noteIndices,
                                                     const juce::String& articulationId)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto notes = findNotes (*clip, noteIndices);

    if (! notes)
        return std::unexpected (notes.error());

    projectService.beginUndoStep (TRANS ("Articulation"));
    auto* undoManager = &clip->edit.getUndoManager();

    for (auto* note : *notes)
    {
        if (articulationId.isEmpty())
            note->state.removeProperty (engine::noteArticulationProperty, undoManager);
        else
            note->state.setProperty (engine::noteArticulationProperty, articulationId, undoManager);
    }

    return {};
}

core::Result<void> MidiClipService::applyFunction (const juce::String& clipId,
                                                   const MidiFunctionParams& params,
                                                   const std::vector<int>& noteIndices)
{
    const auto clip = getClip (clipId);

    if (! clip)
        return std::unexpected (clip.error());

    std::vector<EditableNote> chosen;

    for (const auto& note : clip->notes)
        if (noteIndices.empty() || std::ranges::find (noteIndices, note.index) != noteIndices.end())
            chosen.push_back (note);

    if (chosen.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("There are no notes in the clip.").toStdString());

    const auto changed = transform (chosen, params, clip->lengthBeats);

    if (params.function == MidiFunction::deleteDoubles)
    {
        std::vector<int> removed;

        for (const auto& note : chosen)
            if (std::ranges::none_of (changed, [&note] (const EditableNote& kept) { return kept.index == note.index; }))
                removed.push_back (note.index);

        if (removed.empty())
            return {};

        return removeNotes (clipId, removed);
    }

    return updateNotes (clipId, changed, functionName (params.function));
}

core::Result<void> MidiClipService::updateNotes (const juce::String& clipId,
                                                 const std::vector<EditableNote>& notes,
                                                 const juce::String& actionName)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    std::vector<int> indices;

    for (const auto& note : notes)
        indices.push_back (note.index);

    // Erst alle Zeiger holen, dann ändern: Eine geänderte Startzeit kann die Reihenfolge der
    // Liste verschieben, die Indizes gelten nur für den Stand vor der Änderung.
    const auto targets = findNotes (*clip, indices);

    if (! targets)
        return std::unexpected (targets.error());

    projectService.beginUndoStep (actionName);
    auto* undo = &clip->edit.getUndoManager();

    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        auto* target = (*targets)[i];
        target->setNoteNumber (juce::jlimit (0, 127, notes[i].noteNumber), undo);
        target->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, notes[i].startBeats)),
                                   te::BeatDuration::fromBeats (juce::jmax (minNoteLengthBeats, notes[i].lengthBeats)),
                                   undo);
        target->setVelocity (juce::jlimit (1, 127, notes[i].velocity), undo);
    }

    return {};
}

core::Result<void> MidiClipService::addNotes (const juce::String& clipId,
                                              const std::vector<EditableNote>& notes,
                                              const juce::String& actionName)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (actionName);

    for (const auto& note : notes)
        clip->getSequence().addNote (juce::jlimit (0, 127, note.noteNumber),
                                     te::BeatPosition::fromBeats (juce::jmax (0.0, note.startBeats)),
                                     te::BeatDuration::fromBeats (juce::jmax (minNoteLengthBeats, note.lengthBeats)),
                                     juce::jlimit (1, 127, note.velocity),
                                     defaultColourIndex,
                                     &clip->edit.getUndoManager());

    return {};
}

core::Result<void> MidiClipService::removeNotes (const juce::String& clipId, const std::vector<int>& noteIndices)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto targets = findNotes (*clip, noteIndices);

    if (! targets)
        return std::unexpected (targets.error());

    projectService.beginUndoStep (noteIndices.size() == 1 ? TRANS ("Delete note") : TRANS ("Delete notes"));

    for (auto* note : *targets)
        clip->getSequence().removeNote (*note, &clip->edit.getUndoManager());

    return {};
}

core::Result<std::vector<te::MidiNote*>> MidiClipService::findNotes (te::MidiClip& clip,
                                                                     const std::vector<int>& noteIndices) const
{
    std::vector<te::MidiNote*> notes;

    for (const int index : noteIndices)
    {
        auto* note = findNote (clip, index);

        if (note == nullptr)
            return core::fail (core::ErrorCode::notFound, TRANS ("Note not found.").toStdString());

        notes.push_back (note);
    }

    return notes;
}

void MidiClipService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void MidiClipService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void MidiClipService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
    }

    cancelPendingUpdate();
    handleAsyncUpdate();
}

void MidiClipService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.midiClipChanged(); });
}

namespace
{
// Ein MIDI-Clip oder etwas darin: Noten, Controller-Ereignisse (CONTROL), Sequenz.
bool isInsideMidiClip (juce::ValueTree tree)
{
    for (; tree.isValid() && ! tree.hasType (te::IDs::TRACK); tree = tree.getParent())
        if (tree.hasType (te::IDs::MIDICLIP))
            return true;

    return false;
}
} // namespace

void MidiClipService::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (isInsideMidiClip (child) || isInsideMidiClip (parent))
        triggerAsyncUpdate();
}

void MidiClipService::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (isInsideMidiClip (child) || isInsideMidiClip (parent))
        triggerAsyncUpdate();
}

void MidiClipService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (isInsideMidiClip (tree))
        triggerAsyncUpdate();
}

core::Result<void> MidiClipService::checkClipUnlocked (te::MidiClip& clip)
{
    if (auto* track = dynamic_cast<te::AudioTrack*> (clip.getClipTrack()))
        return bakeLock::checkClipsUnlocked (*track);

    return {};
}

te::MidiClip* MidiClipService::findClip (const juce::String& clipId) const
{
    if (auto* edit = projectService.getEdit())
        return dynamic_cast<te::MidiClip*> (te::findClipForID (*edit, te::EditItemID::fromString (clipId)));

    return nullptr;
}

te::MidiNote* MidiClipService::findNote (te::MidiClip& clip, int noteIndex) const
{
    const auto& notes = clip.getSequence().getNotes();
    return noteIndex >= 0 && noteIndex < notes.size() ? notes[noteIndex] : nullptr;
}

} // namespace fk::services
