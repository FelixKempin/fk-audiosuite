#include "fk/services/TrackColours.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
std::vector<juce::String> trackNames (TestEnvironment& env)
{
    std::vector<juce::String> names;

    for (auto* track : env.trackService.getAudioTracks())
        names.push_back (env.trackService.getTrackInfo (*track).name);

    return names;
}
} // namespace

TEST_CASE ("TrackService: Spuren umsortieren, mit Undo", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Sortieren").has_value());
    auto* a = *env.trackService.createAudioTrack ("A");
    REQUIRE (env.trackService.createAudioTrack ("B").has_value());
    auto* c = *env.trackService.createAudioTrack ("C");

    REQUIRE (env.trackService.moveTrack (*c, 0).has_value());
    REQUIRE (trackNames (env) == std::vector<juce::String> { "C", "A", "B" });

    REQUIRE (env.trackService.moveTrack (*a, 2).has_value());
    REQUIRE (trackNames (env) == std::vector<juce::String> { "C", "B", "A" });

    REQUIRE (env.trackService.moveTrack (*a, 99).has_value()); // wird begrenzt
    REQUIRE (trackNames (env) == std::vector<juce::String> { "C", "B", "A" });

    env.projectService.undo();
    REQUIRE (trackNames (env) == std::vector<juce::String> { "C", "A", "B" });
}

TEST_CASE ("TrackService: neue Spuren haben feste Farben, die beim Umsortieren mitwandern", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Farben").has_value());
    auto* first = *env.trackService.createAudioTrack ("Eins");
    auto* second = *env.trackService.createAudioTrack ("Zwei");

    const auto firstColour = env.trackService.getTrackInfo (*first).colour;
    REQUIRE (firstColour == fk::services::paletteColour (0));
    REQUIRE (env.trackService.getTrackInfo (*second).colour == fk::services::paletteColour (1));

    REQUIRE (env.trackService.moveTrack (*first, 1).has_value());
    REQUIRE (env.trackService.getTrackInfo (*first).colour == firstColour);

    env.trackService.setTrackColour (*first, juce::Colours::orange);
    REQUIRE (env.trackService.getTrackInfo (*first).colour == juce::Colours::orange);
    env.projectService.undo();
    REQUIRE (env.trackService.getTrackInfo (*first).colour == firstColour);
}

TEST_CASE ("TrackService: Spur duplizieren legt eine Kopie mit Clips direkt darunter an", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Duplizieren").has_value());
    auto* guitar = *env.trackService.createAudioTrack ("Gitarre", 1);
    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    REQUIRE (env.clipService.importAudioFile (*guitar, env.createSilentWav ("riff", 1.0), 2.0).has_value());

    const auto copy = env.trackService.duplicateTrack (*guitar);
    REQUIRE (copy.has_value());
    REQUIRE (trackNames (env) == std::vector<juce::String> { "Gitarre", "Gitarre copy", "Bass" });

    const auto copyInfo = env.trackService.getTrackInfo (**copy);
    REQUIRE (copyInfo.numChannels == 1);

    const auto originalClips = env.trackService.getClips (*guitar);
    const auto copiedClips = env.trackService.getClips (**copy);
    REQUIRE (copiedClips.size() == 1);
    REQUIRE (copiedClips.front().startSeconds == originalClips.front().startSeconds);
    REQUIRE (copiedClips.front().id != originalClips.front().id); // eigene Kennungen

    env.projectService.undo();
    REQUIRE (env.trackService.getAudioTracks().size() == 2);
}

TEST_CASE ("TrackService: ausgeblendete Spuren fehlen in getVisibleTracks", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Ausblenden").has_value());
    auto* keys = *env.trackService.createAudioTrack ("Keys");
    REQUIRE (env.trackService.createAudioTrack ("Drums").has_value());

    env.trackService.setHidden (*keys, true);
    REQUIRE (env.trackService.getTrackInfo (*keys).hidden);
    REQUIRE (env.trackService.getAudioTracks().size() == 2);
    REQUIRE (env.trackService.getVisibleTracks().size() == 1);

    REQUIRE (env.projectService.saveProject().has_value());
    const auto editFile = env.projectService.getEditFile();
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile).has_value());
    REQUIRE (env.trackService.getVisibleTracks().size() == 1);
}

namespace
{
/// Spurliste als lesbare Zeilen: "[Ordner]" für Ordner, Einrückung mit Punkten.
std::vector<juce::String> listing (TestEnvironment& env)
{
    std::vector<juce::String> lines;

    for (const auto& entry : env.trackService.getTrackList())
    {
        const auto name = entry.folder != nullptr ? "[" + env.trackService.getFolderInfo (*entry.folder).name + "]"
                                                  : env.trackService.getTrackInfo (*entry.track).name;
        lines.push_back (juce::String::repeatedString (".", entry.depth) + name);
    }

    return lines;
}
} // namespace

TEST_CASE ("TrackService: Ordnerspur, Spuren hinein und heraus, Zuklappen", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Ordner").has_value());
    auto* kick = *env.trackService.createAudioTrack ("Kick");
    auto* snare = *env.trackService.createAudioTrack ("Snare");
    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    auto* drums = *env.trackService.createFolderTrack ("Drums");

    REQUIRE (listing (env) == std::vector<juce::String> { "Kick", "Snare", "Bass", "[Drums]" });

    REQUIRE (env.trackService.moveIntoFolder (*kick, drums).has_value());
    REQUIRE (env.trackService.moveIntoFolder (*snare, drums).has_value());
    REQUIRE (listing (env) == std::vector<juce::String> { "Bass", "[Drums]", ".Kick", ".Snare" });
    REQUIRE (env.trackService.getFolderInfo (*drums).numTracks == 2);
    REQUIRE (env.trackService.getParentFolder (*kick) == drums);
    REQUIRE (env.trackService.getAudioTracks().size() == 3); // Spuren im Ordner zählen mit

    // Ordner nach oben: Inhalt wandert mit.
    REQUIRE (env.trackService.moveTrackBefore (*drums, env.trackService.getAudioTracks().front()).has_value());
    REQUIRE (listing (env) == std::vector<juce::String> { "[Drums]", ".Kick", ".Snare", "Bass" });

    REQUIRE (env.trackService.moveWithinFolder (*snare, -1).has_value());
    REQUIRE (listing (env) == std::vector<juce::String> { "[Drums]", ".Snare", ".Kick", "Bass" });

    env.trackService.setFolderCollapsed (*drums, true);
    REQUIRE (listing (env) == std::vector<juce::String> { "[Drums]", "Bass" });
    env.trackService.setFolderCollapsed (*drums, false);

    REQUIRE (env.trackService.moveIntoFolder (*kick, nullptr).has_value());
    REQUIRE (listing (env) == std::vector<juce::String> { "[Drums]", ".Snare", "Kick", "Bass" });

    // Ordner nicht in sich selbst.
    auto* inner = *env.trackService.createFolderTrack ("Innen");
    REQUIRE (env.trackService.moveIntoFolder (*inner, drums).has_value());
    REQUIRE_FALSE (env.trackService.moveIntoFolder (*drums, inner).has_value());
}

TEST_CASE ("TrackService: Ordner-Mute wirkt auf den Inhalt, Ordner auflösen behält die Spuren", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Ordner2").has_value());
    auto* gtr = *env.trackService.createAudioTrack ("Gitarre");
    auto* folder = *env.trackService.createFolderTrack ("Gitarren");
    REQUIRE (env.trackService.moveIntoFolder (*gtr, folder).has_value());

    env.trackService.setFolderMuted (*folder, true);
    REQUIRE (env.trackService.getFolderInfo (*folder).muted);
    REQUIRE (gtr->isMuted (true)); // die Engine schaltet die Spur im Ordner stumm
    env.trackService.setFolderMuted (*folder, false);

    REQUIRE (env.trackService.deleteFolder (*folder, true).has_value());
    REQUIRE (env.trackService.getFolders().empty());
    REQUIRE (listing (env) == std::vector<juce::String> { "Gitarre" });

    env.projectService.undo();
    REQUIRE (listing (env) == std::vector<juce::String> { "[Gitarren]", ".Gitarre" });

    auto* restored = env.trackService.getFolders().front();
    REQUIRE (env.trackService.deleteFolder (*restored, false).has_value());
    REQUIRE (env.trackService.getAudioTracks().empty());
}

TEST_CASE ("TrackService: Ordner überleben Speichern und Öffnen", "[services][a2]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Ordner3").has_value());
    auto* vox = *env.trackService.createAudioTrack ("Vox");
    auto* folder = *env.trackService.createFolderTrack ("Gesang");
    REQUIRE (env.trackService.moveIntoFolder (*vox, folder).has_value());
    env.trackService.setFolderCollapsed (*folder, true);
    REQUIRE (env.projectService.saveProject().has_value());

    const auto editFile = env.projectService.getEditFile();
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile).has_value());
    REQUIRE (listing (env) == std::vector<juce::String> { "[Gesang]" });
    REQUIRE (env.trackService.getFolderInfo (*env.trackService.getFolders().front()).collapsed);
}
