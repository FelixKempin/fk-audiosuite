#include "fk/services/ProjectSettingsService.h"
#include "fk/services/TimeFormat.h"
#include "fk/services/TransportService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::TimeDisplay;
using fk::tests::TestEnvironment;

TEST_CASE ("formatTime: Sekunden, Timecode und Samples", "[services][a3]")
{
    using fk::services::formatTime;
    REQUIRE (formatTime (83.456, TimeDisplay::seconds, 48000.0, 25) == "1:23.456");
    REQUIRE (formatTime (0.0, TimeDisplay::seconds, 48000.0, 25) == "0:00.000");
    REQUIRE (formatTime (3723.48, TimeDisplay::timecode, 48000.0, 25) == "01:02:03:12");
    REQUIRE (formatTime (1.0 - 1.0e-6, TimeDisplay::timecode, 48000.0, 30) == "00:00:00:29");
    REQUIRE (formatTime (2.0, TimeDisplay::samples, 48000.0, 25) == juce::String::fromUTF8 ("96 000"));
    REQUIRE (formatTime (0.01, TimeDisplay::samples, 44100.0, 25) == "441");
    REQUIRE (formatTime (-1.0, TimeDisplay::seconds, 48000.0, 25) == "0:00.000"); // nie negativ
}

TEST_CASE ("TimelineService: nächster und voriger Taktanfang", "[services][a3]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Takte").has_value());
    env.timelineService.setTempoBpm (120.0); // 4/4: ein Takt = 2 s

    REQUIRE (env.timelineService.getNextBarSeconds (0.0) == Catch::Approx (2.0));
    REQUIRE (env.timelineService.getNextBarSeconds (2.5) == Catch::Approx (4.0));
    REQUIRE (env.timelineService.getPreviousBarSeconds (4.0) == Catch::Approx (2.0));  // genau drauf: davor
    REQUIRE (env.timelineService.getPreviousBarSeconds (4.02) == Catch::Approx (2.0)); // knapp dahinter: davor
    REQUIRE (env.timelineService.getPreviousBarSeconds (5.0) == Catch::Approx (4.0));
    REQUIRE (env.timelineService.getPreviousBarSeconds (0.5) == Catch::Approx (0.0));
}

TEST_CASE ("TransportService: Punch schließt Loop aus, Pre-/Post-Roll im Projekt", "[services][a3]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Punch").has_value());

    env.timelineService.setLoopRange ({ 4.0, 8.0 });
    env.timelineService.setLooping (true);
    transport.setPunchEnabled (true);
    REQUIRE (transport.isPunchEnabled());
    REQUIRE_FALSE (env.timelineService.isLooping());

    transport.setPreRollSeconds (1.5);
    transport.setPostRollSeconds (2.0);
    REQUIRE (transport.getPreRollSeconds() == Catch::Approx (1.5));
    REQUIRE (transport.getPostRollSeconds() == Catch::Approx (2.0));

    // Aufnahme mit Punch beginnt vor dem linken Locator (ohne scharfe Spur nimmt Tracktion nichts auf).
    transport.setPositionSeconds (20.0);
    transport.record();
    REQUIRE (transport.getPositionSeconds() == Catch::Approx (2.5).margin (0.2));
    transport.stop();

    REQUIRE (env.projectService.saveProject().has_value());
    const auto editFile = env.projectService.getEditFile();
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile).has_value());
    REQUIRE (transport.isPunchEnabled());
    REQUIRE (transport.getPreRollSeconds() == Catch::Approx (1.5));
    REQUIRE (transport.getPostRollSeconds() == Catch::Approx (2.0));
}

TEST_CASE ("ProjectSettingsService: Aufnahmemodi landen auf den Eingängen", "[services][a3]")
{
    TestEnvironment env;
    fk::services::ProjectSettingsService settingsService (env.engineContext, env.projectService);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Modi").has_value());

    auto settings = settingsService.getSettings();
    REQUIRE (settings.audioRecordMode == fk::services::AudioRecordMode::keepTakes);
    REQUIRE (settings.midiRecordMode == fk::services::MidiRecordMode::merge);
    REQUIRE (settings.timecodeFps == 25);

    settings.audioRecordMode = fk::services::AudioRecordMode::replace;
    settings.midiRecordMode = fk::services::MidiRecordMode::replace;
    settings.timecodeFps = 30;
    REQUIRE (settingsService.setSettings (settings).has_value());

    auto& devices = env.engineContext.getEngine().getDeviceManager();
    const auto replaceMode = tracktion::WaveInputDevice::getMergeModes()[1];

    for (auto* input : devices.getWaveInputDevices())
        REQUIRE (input->getMergeMode() == replaceMode);

    for (auto& input : devices.getMidiInDevices())
    {
        REQUIRE (input->replaceExistingClips);
        REQUIRE_FALSE (input->mergeRecordings);
    }

    settings.timecodeFps = 29;
    REQUIRE_FALSE (settingsService.setSettings (settings).has_value());

    // Vorgaben zurück: die Eingänge sind engine-weit, andere Tests erwarten das Standardverhalten.
    REQUIRE (settingsService.setSettings (fk::services::ProjectSettings {}).has_value());
}
