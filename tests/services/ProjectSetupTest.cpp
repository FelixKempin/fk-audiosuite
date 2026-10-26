#include "fk/services/DiagnosticsService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/TemplateService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::ProjectSettings;
using fk::services::ProjectSettingsService;
using fk::services::RecordFormat;
using fk::tests::TestEnvironment;

TEST_CASE ("ProjectSettingsService: Einstellungen überleben Speichern und Öffnen", "[services][a1]")
{
    TestEnvironment env;
    ProjectSettingsService settingsService (env.engineContext, env.projectService);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Einstellungen").has_value());

    // Neues Projekt: Rate des Geräts (oder 48 kHz ohne Gerät), WAV mit 24 Bit.
    const auto defaults = settingsService.getSettings();
    REQUIRE (defaults.recordFormat == RecordFormat::wav);
    REQUIRE (defaults.bitDepth == 24);
    REQUIRE (settingsService.deviceMatchesProject());

    REQUIRE (settingsService.setSettings ({ 96000.0, RecordFormat::flac, 16 }).has_value());
    REQUIRE (env.projectService.saveProject().has_value());
    const auto editFile = env.projectService.getEditFile();
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile).has_value());

    const auto loaded = settingsService.getSettings();
    REQUIRE (loaded.sampleRate == 96000.0);
    REQUIRE (loaded.recordFormat == RecordFormat::flac);
    REQUIRE (loaded.bitDepth == 16);

    // Format und Bittiefe landen auf allen Eingängen.
    for (auto* input : env.engineContext.getEngine().getDeviceManager().getWaveInputDevices())
    {
        REQUIRE (input->getBitDepth() == 16);
        REQUIRE_FALSE (input->getAvailableBitDepths().contains (32)); // FLAC kennt kein 32 Bit, WAV schon
    }

    // Zurück auf die Vorgabe, damit andere Tests mit WAV aufnehmen (Eingänge sind engine-weit).
    REQUIRE (settingsService.setSettings ({ 48000.0, RecordFormat::wav, 24 }).has_value());
}

TEST_CASE ("ProjectSettingsService: ungültige Bittiefe wird abgelehnt", "[services][a1]")
{
    TestEnvironment env;
    ProjectSettingsService settingsService (env.engineContext, env.projectService);
    REQUIRE_FALSE (settingsService.setSettings ({ 48000.0, RecordFormat::wav, 24 }).has_value()); // kein Projekt

    REQUIRE (env.projectService.createProject (env.tempFolder, "Bits").has_value());
    REQUIRE_FALSE (settingsService.setSettings ({ 48000.0, RecordFormat::flac, 32 }).has_value());
    REQUIRE_FALSE (settingsService.setSettings ({ 0.0, RecordFormat::wav, 24 }).has_value());
    REQUIRE_FALSE (settingsService.setDeviceSampleRate (12345.0).has_value());
}

TEST_CASE ("TemplateService: Vorlage ohne Audio-Clips, neues Projekt daraus", "[services][a1]")
{
    TestEnvironment env;
    fk::services::TemplateService templates (env.projectService, env.tempFolder.getChildFile ("Vorlagen"));
    REQUIRE (templates.getTemplates().empty());
    REQUIRE_FALSE (templates.saveAsTemplate ("Band").has_value()); // kein Projekt

    REQUIRE (env.projectService.createProject (env.tempFolder, "Quelle").has_value());
    auto* guitar = *env.trackService.createAudioTrack ("Gitarre");
    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    REQUIRE (env.clipService.importAudioFile (*guitar, env.createSilentWav ("riff", 1.0), 0.0).has_value());

    const auto saved = templates.saveAsTemplate ("Band / Live"); // Schrägstrich wird zu legalem Namen
    REQUIRE (saved.has_value());
    REQUIRE (templates.getTemplates().size() == 1);

    REQUIRE (env.projectService.createProjectFromTemplate (env.tempFolder, "Neu", *saved).has_value());
    REQUIRE (env.projectService.getProjectName() == "Neu");
    REQUIRE (env.projectService.getProjectFolder().getChildFile ("Audio").isDirectory());

    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 2);
    REQUIRE (env.trackService.getTrackInfo (*tracks[0]).name == "Gitarre");
    REQUIRE (env.trackService.getClips (*tracks[0]).empty());

    REQUIRE (templates.deleteTemplate (*saved).has_value());
    REQUIRE (templates.getTemplates().empty());
    REQUIRE_FALSE (templates.deleteTemplate (env.projectService.getEditFile()).has_value()); // keine Vorlage
}

TEST_CASE ("DiagnosticsService: Bericht enthält Systembeschreibung und die neuesten Logs", "[services][a1]")
{
    TestEnvironment env;
    const fk::services::DiagnosticsFolders folders { env.tempFolder.getChildFile ("Logs"),
                                                     env.tempFolder.getChildFile ("Haenger"),
                                                     env.tempFolder.getChildFile ("Absturz") };
    REQUIRE (folders.logs.createDirectory());

    for (int i = 0; i < fk::services::DiagnosticsService::maxFilesPerFolder + 2; ++i)
        REQUIRE (folders.logs.getChildFile ("log" + juce::String (i) + ".txt").replaceWithText ("Zeile"));

    REQUIRE (env.projectService.createProject (env.tempFolder, "Diagnose").has_value());
    fk::services::DiagnosticsService diagnostics (env.engineContext, env.projectService, folders);

    const auto text = diagnostics.describeSystem ("9.9.9");
    REQUIRE (text.contains ("FK Audio Suite 9.9.9"));
    REQUIRE (text.contains ("Diagnose.tracktionedit"));

    const auto zip = env.tempFolder.getChildFile ("bericht.zip");
    REQUIRE (diagnostics.exportReport (zip, "9.9.9").has_value());

    juce::ZipFile archive (zip);
    REQUIRE (archive.getIndexOfFileName ("System.txt") >= 0);
    REQUIRE (archive.getNumEntries() == 1 + fk::services::DiagnosticsService::maxFilesPerFolder);
}
