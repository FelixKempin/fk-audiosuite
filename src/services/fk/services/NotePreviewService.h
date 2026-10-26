#pragma once

#include "fk/services/ProjectService.h"

#include <vector>

namespace fk::services
{

/// Vorhören im Key Editor (Cubase: "Akustisches Feedback"): Klick auf eine Taste oder Note,
/// Setzen und Ziehen spielen die Note über das Instrument der Spur, zu der der Clip gehört.
/// Läuft auch bei gestopptem Transport; jede gestartete Note wird mit stopAll beendet.
class NotePreviewService final : private ProjectService::Listener
{
public:
    explicit NotePreviewService (ProjectService& projectService);
    ~NotePreviewService() override;

    /// Startet eine Note auf der Spur des Clips (eine laufende gleiche Note wird vorher beendet).
    void startNote (const juce::String& clipId, int noteNumber, int velocity);

    /// Beendet alle über diesen Dienst gestarteten Noten.
    void stopAll();

private:
    struct SoundingNote
    {
        te::EditItemID trackId;
        int noteNumber = 0;
    };

    void projectChanged() override;
    void send (te::AudioTrack& track, const juce::MidiMessage& message);
    [[nodiscard]] te::AudioTrack* findTrackForClip (const juce::String& clipId) const;

    ProjectService& projectService;
    te::MPESourceID sourceId;
    std::vector<SoundingNote> sounding;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotePreviewService)
};

} // namespace fk::services
