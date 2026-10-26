#include "fk/services/NotePreviewService.h"

#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
// Zählt Live-MIDI, das Tracktion an die Spur reicht (dort hängt sonst der Graph als Empfänger).
struct LiveMidiCounter final : fk::services::te::AudioTrack::Listener
{
    void injectLiveMidiMessage (fk::services::te::AudioTrack&,
                                const fk::services::te::MidiMessageWithSource& message,
                                bool& wasUsed) override
    {
        noteOns += message.isNoteOn() ? 1 : 0;
        noteOffs += message.isNoteOff() ? 1 : 0;
        wasUsed = true;
    }

    void recordedMidiMessageSentToPlugins (fk::services::te::AudioTrack&, const juce::MidiMessage&) override {}

    int noteOns = 0;
    int noteOffs = 0;
};
} // namespace

TEST_CASE ("NotePreviewService: Vorhören schickt Note-On an die Spur des Clips, stopAll beendet alles",
           "[services][midiclip]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::NotePreviewService preview { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Preview").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0);

    LiveMidiCounter counter;
    track->addListener (&counter);

    preview.startNote (clipId, 60, 100);
    preview.startNote (clipId, 60, 100); // dieselbe Note: erst aus, dann wieder an
    preview.startNote (clipId, 64, 90);
    REQUIRE (counter.noteOns == 3);
    REQUIRE (counter.noteOffs == 1);

    preview.stopAll();
    REQUIRE (counter.noteOffs == 3);

    preview.startNote ("gibt-es-nicht", 60, 100); // unbekannter Clip: still ignoriert
    REQUIRE (counter.noteOns == 3);
    track->removeListener (&counter);
}
