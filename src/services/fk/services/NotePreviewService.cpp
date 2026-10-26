#include "fk/services/NotePreviewService.h"

#include <algorithm>

namespace fk::services
{

NotePreviewService::NotePreviewService (ProjectService& service)
    : projectService (service)
    , sourceId (te::createUniqueMPESourceID())
{
    projectService.addListener (*this);
}

NotePreviewService::~NotePreviewService()
{
    stopAll();
    projectService.removeListener (*this);
}

void NotePreviewService::startNote (const juce::String& clipId, int noteNumber, int velocity)
{
    auto* track = findTrackForClip (clipId);

    if (track == nullptr)
        return;

    // Dieselbe Note nicht doppelt anschlagen: erst beenden, dann neu.
    const auto same = std::find_if (sounding.begin(),
                                    sounding.end(),
                                    [track, noteNumber] (const SoundingNote& note)
                                    { return note.trackId == track->itemID && note.noteNumber == noteNumber; });

    if (same != sounding.end())
    {
        send (*track, juce::MidiMessage::noteOff (1, noteNumber));
        sounding.erase (same);
    }

    send (*track,
          juce::MidiMessage::noteOn (
              1, juce::jlimit (0, 127, noteNumber), static_cast<juce::uint8> (juce::jlimit (1, 127, velocity))));
    sounding.push_back ({ track->itemID, noteNumber });
}

void NotePreviewService::stopAll()
{
    auto* edit = projectService.getEdit();

    for (const auto& note : sounding)
        if (edit != nullptr)
            if (auto* track = te::findAudioTrackForID (*edit, note.trackId))
                send (*track, juce::MidiMessage::noteOff (1, note.noteNumber));

    sounding.clear();
}

void NotePreviewService::projectChanged()
{
    sounding.clear(); // das alte Edit ist weg, seine Spuren auch
}

void NotePreviewService::send (te::AudioTrack& track, const juce::MidiMessage& message)
{
    // Zeitstempel in der Audio-Uhr, wie beim Bildschirm-Keyboard (MidiService::send).
    auto stamped = message;
    stamped.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    track.injectLiveMidiMessage (stamped, sourceId);
}

te::AudioTrack* NotePreviewService::findTrackForClip (const juce::String& clipId) const
{
    if (auto* edit = projectService.getEdit())
        if (auto* clip = te::findClipForID (*edit, te::EditItemID::fromString (clipId)))
            return dynamic_cast<te::AudioTrack*> (clip->getClipTrack());

    return nullptr;
}

} // namespace fk::services
