#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/KeyEditor.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("KeyEditor: Schnappschuss mit Tonart und Stufenakkord als PNG (FK_UI_SNAPSHOT_DIR)", "[ui][snapshot]")
{
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

    if (directory.isEmpty())
        SKIP ("FK_UI_SNAPSHOT_DIR nicht gesetzt");

    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    REQUIRE (env.projectService.createProject (env.tempFolder, "KeyEditor").has_value());
    REQUIRE (signatureService.setKey (0, 7, fk::services::ScaleKind::major).has_value()); // G-Dur
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 8.0);

    // Stufen I, IV, V7 als Akkorde über je einen Takt.
    using fk::services::ChordKind;
    using fk::services::SignatureService;
    REQUIRE (
        midiClips
            .addChord (clipId,
                       SignatureService::getChordNotes (7, fk::services::ScaleKind::major, 1, ChordKind::triad, 0, 4),
                       0.0,
                       4.0,
                       100)
            .has_value());
    REQUIRE (
        midiClips
            .addChord (clipId,
                       SignatureService::getChordNotes (7, fk::services::ScaleKind::major, 4, ChordKind::triad, 0, 4),
                       4.0,
                       4.0,
                       100)
            .has_value());
    REQUIRE (
        midiClips
            .addChord (clipId,
                       SignatureService::getChordNotes (7, fk::services::ScaleKind::major, 5, ChordKind::seventh, 0, 4),
                       8.0,
                       4.0,
                       100)
            .has_value());

    // Unterschiedliche Anschlagsstärken, damit die Velocity-Lane etwas zeigt.
    const auto notes = midiClips.getClip (clipId)->notes;
    for (std::size_t i = 0; i < notes.size(); ++i)
        REQUIRE (midiClips.setNoteVelocity (clipId, notes[i].index, 40 + static_cast<int> (i) * 9).has_value());

    fk::services::MidiControllerService midiControllers { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::NotePreviewService notePreview { env.projectService };
    fk::ui::KeyEditor editor (
        { midiClips, midiControllers, notePreview, transport, env.timelineService, signatureService });
    editor.setSize (1100, 360);
    editor.setClip (clipId);
    TestEnvironment::pumpMessages (100);

    const juce::File folder (directory);
    REQUIRE (folder.createDirectory());
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    folder.getChildFile ("KeyEditor_Tonart.png").deleteFile(); // FileOutputStream hängt sonst an das alte Bild an
    juce::FileOutputStream stream (folder.getChildFile ("KeyEditor_Tonart.png"));
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
