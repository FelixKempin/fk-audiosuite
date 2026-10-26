#include "fk/services/MidiControllerService.h"

#include "fk/services/BakeLock.h"

namespace fk::services
{

MidiControllerService::MidiControllerService (ProjectService& service)
    : projectService (service)
{
}

core::Result<void>
MidiControllerService::addControllerEvent (const juce::String& clipId, int type, double beats, int value)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Set controller"));
    clip->getSequence().addControllerEvent (te::BeatPosition::fromBeats (juce::jmax (0.0, beats)),
                                            type,
                                            juce::jlimit (0, controllerTypes::maxValue (type), value),
                                            &clip->edit.getUndoManager());
    return {};
}

core::Result<void>
MidiControllerService::setControllerEvent (const juce::String& clipId, int eventIndex, double beats, int value)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto& events = clip->getSequence().getControllerEvents();

    if (eventIndex < 0 || eventIndex >= events.size())
        return core::fail (core::ErrorCode::notFound, TRANS ("Controller event not found.").toStdString());

    auto* event = events[eventIndex];
    auto* undo = &clip->edit.getUndoManager();
    projectService.beginUndoStep (TRANS ("Change controller"));
    event->setBeatPosition (te::BeatPosition::fromBeats (juce::jmax (0.0, beats)), undo);
    event->setControllerValue (juce::jlimit (0, controllerTypes::maxValue (event->getType()), value), undo);
    return {};
}

core::Result<void> MidiControllerService::removeControllerEvent (const juce::String& clipId, int eventIndex)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto& events = clip->getSequence().getControllerEvents();

    if (eventIndex < 0 || eventIndex >= events.size())
        return core::fail (core::ErrorCode::notFound, TRANS ("Controller event not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Delete controller"));
    clip->getSequence().removeControllerEvent (*events[eventIndex], &clip->edit.getUndoManager());
    return {};
}

core::Result<void> MidiControllerService::replaceControllerEvents (const juce::String& clipId,
                                                                   int type,
                                                                   double fromBeats,
                                                                   double toBeats,
                                                                   const std::vector<std::pair<double, int>>& points)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("MIDI clip not found.").toStdString());

    if (auto unlocked = checkClipUnlocked (*clip); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (points.empty() ? TRANS ("Delete controller") : TRANS ("Draw controller"));
    auto& sequence = clip->getSequence();
    auto* undo = &clip->edit.getUndoManager();

    for (int i = sequence.getControllerEvents().size(); --i >= 0;)
    {
        auto* event = sequence.getControllerEvents()[i];
        const auto beat = event->getBeatPosition().inBeats();

        if (event->getType() == type && beat >= fromBeats && beat < toBeats)
            sequence.removeControllerEvent (*event, undo);
    }

    for (const auto& [beats, value] : points)
        sequence.addControllerEvent (te::BeatPosition::fromBeats (juce::jmax (0.0, beats)),
                                     type,
                                     juce::jlimit (0, controllerTypes::maxValue (type), value),
                                     undo);

    return {};
}

core::Result<void>
MidiControllerService::removeControllerEvents (const juce::String& clipId, int type, double fromBeats, double toBeats)
{
    return replaceControllerEvents (clipId, type, fromBeats, toBeats, {});
}

core::Result<void> MidiControllerService::checkClipUnlocked (te::MidiClip& clip)
{
    if (auto* track = dynamic_cast<te::AudioTrack*> (clip.getClipTrack()))
        return bakeLock::checkClipsUnlocked (*track);

    return {};
}

te::MidiClip* MidiControllerService::findClip (const juce::String& clipId) const
{
    if (auto* edit = projectService.getEdit())
        return dynamic_cast<te::MidiClip*> (te::findClipForID (*edit, te::EditItemID::fromString (clipId)));

    return nullptr;
}

} // namespace fk::services
