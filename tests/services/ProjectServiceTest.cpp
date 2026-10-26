#include "fk/services/AutosaveService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("ProjectService: createProject legt Ordner, Audio/ und Edit-Datei an", "[services]")
{
    TestEnvironment env;

    const auto result = env.projectService.createProject (env.tempFolder, "Demo");

    REQUIRE (result.has_value());
    REQUIRE (env.projectService.hasProject());
    REQUIRE (env.projectService.getProjectName() == "Demo");
    REQUIRE (env.tempFolder.getChildFile ("Demo/Demo.tracktionedit").existsAsFile());
    REQUIRE (env.tempFolder.getChildFile ("Demo/Audio").isDirectory());
}

TEST_CASE ("ProjectService: createProject lehnt leeren Namen und vorhandenen Ordner ab", "[services]")
{
    TestEnvironment env;

    const auto emptyName = env.projectService.createProject (env.tempFolder, "   ");
    REQUIRE_FALSE (emptyName.has_value());
    REQUIRE (emptyName.error().code == fk::core::ErrorCode::invalidArgument);

    REQUIRE (env.projectService.createProject (env.tempFolder, "Doppelt").has_value());
    const auto duplicate = env.projectService.createProject (env.tempFolder, "Doppelt");
    REQUIRE_FALSE (duplicate.has_value());
}

TEST_CASE ("TrackService: Spuren überleben Speichern und erneutes Öffnen", "[services]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Spuren").has_value());

    const auto created = env.trackService.createAudioTrack ("Gitarre");
    REQUIRE (created.has_value());
    REQUIRE (env.trackService.getAudioTracks().size() == 1);
    REQUIRE (env.trackService.getTrackInfo (**created).name == "Gitarre");

    REQUIRE (env.projectService.saveProject().has_value());
    const auto editFile = env.tempFolder.getChildFile ("Spuren/Spuren.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.trackService.getAudioTracks().empty());

    REQUIRE (env.projectService.openProject (editFile).has_value());
    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 1);
    REQUIRE (env.trackService.getTrackInfo (*tracks.front()).name == "Gitarre");
}

TEST_CASE ("ProjectService: Undo macht das Anlegen einer Spur rückgängig", "[services]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Undo").has_value());

    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    REQUIRE (env.trackService.getAudioTracks().size() == 1);

    env.projectService.undo();
    REQUIRE (env.trackService.getAudioTracks().empty());

    env.projectService.redo();
    REQUIRE (env.trackService.getAudioTracks().size() == 1);
}

TEST_CASE ("ProjectService: saveProjectAs kopiert Audio und lässt Clips in den neuen Ordner zeigen", "[services][a1]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Original").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gitarre");
    REQUIRE (env.clipService.importAudioFile (*track, env.createSilentWav ("riff", 1.0), 0.0).has_value());

    REQUIRE (env.projectService.saveProjectAs (env.tempFolder, "Kopie").has_value());

    const auto newFolder = env.tempFolder.getChildFile ("Kopie");
    REQUIRE (env.projectService.getProjectName() == "Kopie");
    REQUIRE (newFolder.getChildFile ("Kopie.tracktionedit").existsAsFile());
    REQUIRE (env.tempFolder.getChildFile ("Original/Original.tracktionedit").existsAsFile());

    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 1);
    const auto clips = env.trackService.getClips (*tracks.front());
    REQUIRE (clips.size() == 1);
    REQUIRE (clips.front().audioFile.existsAsFile());
    REQUIRE (clips.front().audioFile.isAChildOf (newFolder));

    // Ein vorhandener Zielordner wird nie überschrieben.
    REQUIRE_FALSE (env.projectService.saveProjectAs (env.tempFolder, "Original").has_value());
}

TEST_CASE ("ProjectService: Speichern legt nur bei Änderungen ein Backup an und begrenzt die Anzahl", "[services][a1]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Backups").has_value());
    TestEnvironment::pumpMessages (300); // Tracktion setzt "geändert" 200 ms nach dem Laden zurück
    const auto editFile = env.projectService.getEditFile();

    REQUIRE (env.projectService.saveProject().has_value());
    REQUIRE (fk::services::ProjectService::getBackups (editFile).empty());

    for (int i = 0; i < fk::services::ProjectService::maxBackups + 2; ++i)
    {
        REQUIRE (env.trackService.createAudioTrack ("Spur " + juce::String (i)).has_value());
        TestEnvironment::pumpMessages (50); // Tracktion setzt "geändert" asynchron
        REQUIRE (env.projectService.isModified());
        REQUIRE (env.projectService.saveProject().has_value());
        REQUIRE_FALSE (env.projectService.isModified());
    }

    REQUIRE (fk::services::ProjectService::getBackups (editFile).size() ==
             static_cast<std::size_t> (fk::services::ProjectService::maxBackups));
}

TEST_CASE ("ProjectService: automatische Sicherung und Wiederherstellung nach Absturz", "[services][a1]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Absturz").has_value());
    TestEnvironment::pumpMessages (300); // Tracktion setzt "geändert" 200 ms nach dem Laden zurück
    const auto editFile = env.projectService.getEditFile();

    REQUIRE (env.trackService.createAudioTrack ("Gesang").has_value());
    TestEnvironment::pumpMessages (50); // Tracktion setzt "geändert" asynchron
    REQUIRE (env.projectService.writeAutosave().has_value());
    REQUIRE (env.projectService.isModified()); // Autosave ist kein Speichern

    // Die gespeicherte Fassung künstlich älter machen; Dateizeiten sind nicht überall fein aufgelöst.
    editFile.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (1));
    const auto autosave = fk::services::ProjectService::findNewerAutosave (editFile);
    REQUIRE (autosave.has_value());

    // Absturz nachstellen: Schließen löscht die Sicherung, also vorher beiseitelegen und zurücklegen.
    const auto kept = env.tempFolder.getChildFile ("kept.tracktionedit");
    REQUIRE (autosave->copyFileTo (kept));
    env.projectService.closeProject();
    REQUIRE_FALSE (autosave->existsAsFile());
    REQUIRE (kept.moveFileTo (*autosave));
    editFile.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (1));

    REQUIRE (env.projectService.restoreBackup (editFile, *autosave).has_value());
    REQUIRE (env.trackService.getAudioTracks().size() == 1);
    REQUIRE_FALSE (autosave->existsAsFile());
    REQUIRE (fk::services::ProjectService::getBackups (editFile).size() == 1); // "vor Wiederherstellung"
}

TEST_CASE ("ProjectService: archiveAutosave legt eine verworfene Sicherung zu den Backups", "[services][a1]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Archiv").has_value());
    TestEnvironment::pumpMessages (300); // Tracktion setzt "geändert" 200 ms nach dem Laden zurück
    const auto editFile = env.projectService.getEditFile();
    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    TestEnvironment::pumpMessages (50); // Tracktion setzt "geändert" asynchron
    REQUIRE (env.projectService.writeAutosave().has_value());
    editFile.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (1));
    REQUIRE (fk::services::ProjectService::findNewerAutosave (editFile).has_value());

    fk::services::ProjectService::archiveAutosave (editFile);

    REQUIRE_FALSE (fk::services::ProjectService::findNewerAutosave (editFile).has_value());
    REQUIRE (fk::services::ProjectService::getBackups (editFile).size() == 1);
}

TEST_CASE ("ProjectService: zuletzt geöffnete Projekte, neuestes zuerst", "[services][a1]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Erstes").has_value());
    const auto first = env.projectService.getEditFile();
    REQUIRE (env.projectService.createProject (env.tempFolder, "Zweites").has_value());

    auto recent = env.projectService.getRecentProjects();
    REQUIRE (recent.size() >= 2);
    REQUIRE (recent[0] == env.projectService.getEditFile());
    REQUIRE (recent[1] == first);

    REQUIRE (env.projectService.openProject (first).has_value());
    recent = env.projectService.getRecentProjects();
    REQUIRE (recent[0] == first);
}

TEST_CASE ("AutosaveService: sichert nur bei Änderungen", "[services][a1]")
{
    TestEnvironment env;
    fk::services::AutosaveService autosave (env.projectService, env.engineContext);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Takt").has_value());
    TestEnvironment::pumpMessages (300); // Tracktion setzt "geändert" 200 ms nach dem Laden zurück

    autosave.autosaveNow();
    REQUIRE_FALSE (autosave.getLastAutosaveTime().toMilliseconds() > 0);

    REQUIRE (env.trackService.createAudioTrack ("Keys").has_value());
    TestEnvironment::pumpMessages (50); // Tracktion setzt "geändert" asynchron
    autosave.autosaveNow();
    REQUIRE (autosave.getLastAutosaveTime().toMilliseconds() > 0);
    REQUIRE (env.projectService.getProjectFolder().getChildFile ("Backups/Takt.autosave.tracktionedit").existsAsFile());
}

TEST_CASE ("AutosaveService: Intervall wird gespeichert, 0 schaltet aus", "[services][a1]")
{
    TestEnvironment env;

    {
        fk::services::AutosaveService autosave (env.projectService, env.engineContext);
        autosave.setInterval (std::chrono::seconds (0));
    }

    {
        fk::services::AutosaveService autosave (env.projectService, env.engineContext);
        REQUIRE (autosave.getInterval() == std::chrono::seconds (0));
        autosave.setInterval (std::chrono::minutes (2));
    }

    fk::services::AutosaveService autosave (env.projectService, env.engineContext);
    REQUIRE (autosave.getInterval() == std::chrono::minutes (2));
    autosave.setInterval (fk::services::AutosaveService::defaultInterval); // Testeinstellungen zurücksetzen
}
