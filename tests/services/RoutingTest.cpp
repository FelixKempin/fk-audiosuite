#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

#include <algorithm>

using fk::services::TrackKind;
using fk::tests::TestEnvironment;

TEST_CASE ("TrackService: Mono- und Stereospuren bekommen passende Eingänge", "[services][routing]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Kanaele").has_value());

    auto* mono = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* stereo = *env.trackService.createAudioTrack ("Keys", 2);

    REQUIRE (env.trackService.getTrackInfo (*mono).numChannels == 1);
    REQUIRE (env.trackService.getTrackInfo (*stereo).numChannels == 2);
    REQUIRE (env.trackService.getTrackInfo (*mono).kind == TrackKind::audio);

    for (const auto& input : env.trackService.getAvailableAudioInputs (*mono))
        REQUIRE (input.numChannels == 1);

    for (const auto& input : env.trackService.getAvailableAudioInputs (*stereo))
        REQUIRE (input.numChannels == 2);

    // Ob überhaupt Eingänge da sind, hängt vom Audiogerät der Testmaschine ab (reines Ausgabegerät
    // → keine). Geprüft wird deshalb nur die Filterung, nicht die Anzahl.
}

TEST_CASE ("TrackService: Gruppen routen in Gruppen und Master, Schleifen werden abgelehnt", "[services][routing]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Gruppen").has_value());

    auto* drums = *env.trackService.createAudioTrack ("Kick", 1);
    auto* drumGroup = *env.trackService.createGroupTrack ("Drums");
    auto* bandGroup = *env.trackService.createGroupTrack ("Band");

    REQUIRE (env.trackService.getTrackInfo (*drumGroup).kind == TrackKind::group);
    REQUIRE (env.trackService.getTrackInfo (*drums).outputId == "master");

    // Kick → Drums → Band → Master
    REQUIRE (env.trackService.setOutput (*drums, env.trackService.getTrackId (*drumGroup)).has_value());
    REQUIRE (env.trackService.setOutput (*drumGroup, env.trackService.getTrackId (*bandGroup)).has_value());

    REQUIRE (env.trackService.getTrackInfo (*drums).outputName == "Drums");
    REQUIRE (env.trackService.getTrackInfo (*drumGroup).outputName == "Band");
    REQUIRE (env.trackService.getTrackInfo (*bandGroup).outputName == "Master");

    // Band → Drums wäre eine Schleife (Drums mündet schon in Band).
    const auto loop = env.trackService.setOutput (*bandGroup, env.trackService.getTrackId (*drumGroup));
    REQUIRE_FALSE (loop.has_value());
    REQUIRE (loop.error().code == fk::core::ErrorCode::invalidArgument);

    // Die Ausgangsliste von Band bietet Drums deshalb gar nicht erst an.
    bool offersDrums = false;
    for (const auto& output : env.trackService.getAvailableOutputs (*bandGroup))
        offersDrums = offersDrums || output.name == "Drums";
    REQUIRE_FALSE (offersDrums);

    // Zurück auf Master, und eine normale Spur ist nie ein Ziel.
    REQUIRE (env.trackService.setOutput (*drums, "master").has_value());
    REQUIRE (env.trackService.getTrackInfo (*drums).outputId == "master");
    REQUIRE_FALSE (env.trackService.setOutput (*drumGroup, env.trackService.getTrackId (*drums)).has_value());
}

TEST_CASE ("TrackService: Spurart und Kanalzahl überleben Speichern und Öffnen", "[services][routing]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Persist").has_value());
    auto* mono = *env.trackService.createAudioTrack ("Mono", 1);
    auto* group = *env.trackService.createGroupTrack ("Gruppe");
    REQUIRE (env.trackService.setOutput (*mono, env.trackService.getTrackId (*group)).has_value());
    REQUIRE (env.projectService.saveProject().has_value());

    const auto file = env.tempFolder.getChildFile ("Persist/Persist.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());

    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 2);
    REQUIRE (env.trackService.getTrackInfo (*tracks[0]).numChannels == 1);
    REQUIRE (env.trackService.getTrackInfo (*tracks[0]).outputName == "Gruppe");
    REQUIRE (env.trackService.getTrackInfo (*tracks[1]).kind == TrackKind::group);
}

TEST_CASE ("TrackService: Direct Out auf ein Ausgangspaar des Interfaces", "[services][routing]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "DirectOut").has_value());
    auto* track = *env.trackService.createAudioTrack ("Click", 1);

    // Unbekannte Geräte werden abgelehnt, die Spur bleibt am Master.
    REQUIRE_FALSE (env.trackService.setOutput (*track, "device:gibt-es-nicht").has_value());
    REQUIRE (env.trackService.getOutputId (*track) == "master");

    // Ob es neben dem Master-Paar weitere Paare gibt, hängt vom Audiogerät der Testmaschine ab.
    juce::String directOut;
    for (const auto& output : env.trackService.getAvailableOutputs (*track))
        if (output.id.startsWith ("device:"))
            directOut = output.id;

    if (directOut.isEmpty())
    {
        WARN ("Audiogerät hat nur ein Ausgangspaar, Direct Out nicht prüfbar");
        return;
    }

    REQUIRE (env.trackService.setOutput (*track, directOut).has_value());
    REQUIRE (env.trackService.getOutputId (*track) == directOut);
    REQUIRE (env.trackService.getTrackInfo (*track).outputName != "Master");

    REQUIRE (env.trackService.setOutput (*track, "master").has_value());
    REQUIRE (env.trackService.getOutputId (*track) == "master");
}

TEST_CASE ("TrackService: eigene Namen für Anschlüsse erscheinen in den Auswahllisten", "[services][routing]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Aliase").has_value());
    REQUIRE_FALSE (env.trackService.setPortAlias ("gibt-es-nicht", "X").has_value());

    const auto ports = env.trackService.getStudioPorts();
    const auto input = std::ranges::find_if (ports, [] (const auto& port) { return port.isInput; });

    if (input == ports.end())
    {
        WARN ("Audiogerät ohne Eingänge, eigene Namen nicht prüfbar");
        return;
    }

    const auto previousAlias = input->alias;
    REQUIRE (env.trackService.setPortAlias (input->id, "Gitarren-DI").has_value());

    bool shown = false;
    for (const auto& available : env.trackService.getAvailableAudioInputs())
        shown = shown || (available.id == input->id && available.name == "Gitarren-DI");
    REQUIRE (shown);

    // Die ID bleibt, nur der Anzeigename ändert sich; leer setzt den Treibernamen zurück.
    REQUIRE (env.trackService.setPortAlias (input->id, previousAlias).has_value());
    for (const auto& port : env.trackService.getStudioPorts())
        if (port.id == input->id)
            REQUIRE (port.alias == previousAlias);
}
