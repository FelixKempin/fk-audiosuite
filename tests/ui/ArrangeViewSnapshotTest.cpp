#include "fk/services/AutomationService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MixerService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/FkLookAndFeel.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("ArrangeView: Schnappschuss mit Automationsspur als PNG (FK_UI_SNAPSHOT_DIR)", "[ui][snapshot]")
{
    // Werkzeug wie bei den Plugin-Oberflächen: zeichnet die Projektzone ohne laufende DAW.
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

    if (directory.isEmpty())
        SKIP ("FK_UI_SNAPSHOT_DIR nicht gesetzt");

    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    REQUIRE (env.projectService.createProject (env.tempFolder, "Schnappschuss").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* keys = *env.trackService.createAudioTrack ("Keys", 2);
    (void)keys;

    const auto wav = env.createSilentWav ("take", 5.0);
    const auto clipId = *env.clipService.importAudioFile (*vocals, wav, 1.0);
    env.clipService.beginChange ("Fades");
    REQUIRE (env.clipService.setFades (clipId, 0.8, 1.5).has_value());
    REQUIRE (env.clipService.setGainDb (clipId, -3.0f).has_value());
    REQUIRE (env.clipService.addTakeFromFile (clipId, env.createSilentWav ("take2", 5.0)).has_value());
    REQUIRE (env.clipService.addTakeFromFile (clipId, env.createSilentWav ("take3", 5.0)).has_value());
    REQUIRE (env.clipService.setShowingTakes (clipId, true).has_value());

    REQUIRE (signatureService.insertTempoPoint (4.0, 90.0, false).has_value());
    REQUIRE (signatureService.setTempoPoint (0, 120.0, true).has_value());
    REQUIRE (signatureService.insertTimeSignature (8.0, { 3, 4 }).has_value());
    REQUIRE (signatureService.setKey (0, 9, fk::services::ScaleKind::minor).has_value());
    REQUIRE (signatureService.insertKey (6.0, 0, fk::services::ScaleKind::major).has_value());

    const auto parameters = automation.getParameters (*vocals);
    REQUIRE (parameters.size() >= 2);
    const auto& pan = parameters[1];
    automation.addLane (*vocals, pan.id);
    automation.addLane (*vocals, parameters[0].id);
    REQUIRE (automation.addPoint (*vocals, pan.id, 1.0, -1.0f).has_value());
    REQUIRE (automation.addPoint (*vocals, pan.id, 4.0, 1.0f).has_value());
    REQUIRE (automation.addPoint (*vocals, pan.id, 7.0, 0.0f).has_value());

    fk::services::MarkerService markers { env.projectService };
    fk::ui::ArrangeView view ({ env.trackService,
                                env.clipService,
                                midiClips,
                                transport,
                                env.timelineService,
                                waveforms,
                                mixer,
                                automation,
                                signatureService,
                                markers });
    REQUIRE (markers.addMarker (0.0, "Intro").has_value());
    REQUIRE (markers.addMarker (4.0, "Strophe").has_value());
    view.setSize (900, 440);
    view.setAutomationShown (vocals, true);
    TestEnvironment::pumpMessages (100);

    const juce::File folder (directory);
    REQUIRE (folder.createDirectory());
    const auto image = view.createComponentSnapshot (view.getLocalBounds());
    folder.getChildFile ("ArrangeView_Automation.png").deleteFile(); // FileOutputStream hängt sonst an das alte Bild an
    juce::FileOutputStream stream (folder.getChildFile ("ArrangeView_Automation.png"));
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
