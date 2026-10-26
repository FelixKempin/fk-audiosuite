#include "fk/services/MarkerService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("MarkerService: setzen, sortieren, umbenennen, verschieben, löschen", "[services][a2]")
{
    TestEnvironment env;
    fk::services::MarkerService markers (env.projectService);
    REQUIRE_FALSE (markers.addMarker (1.0).has_value()); // kein Projekt

    REQUIRE (env.projectService.createProject (env.tempFolder, "Marker").has_value());
    const auto chorus = *markers.addMarker (8.0, "Refrain");
    const auto verse = *markers.addMarker (2.0);

    auto list = markers.getMarkers();
    REQUIRE (list.size() == 2);
    REQUIRE (list[0].id == verse);
    REQUIRE (list[0].name.startsWith ("Marker "));
    REQUIRE (list[1].name == "Refrain");
    REQUIRE (list[1].startSeconds == Catch::Approx (8.0));

    REQUIRE (markers.renameMarker (verse, "Strophe").has_value());
    REQUIRE_FALSE (markers.renameMarker (verse, "  ").has_value());
    REQUIRE (markers.moveMarker (chorus, 1.0).has_value());
    list = markers.getMarkers();
    REQUIRE (list[0].name == "Refrain");
    REQUIRE (list[1].name == "Strophe");

    REQUIRE (markers.deleteMarker (chorus).has_value());
    REQUIRE (markers.getMarkers().size() == 1);
    env.projectService.undo();
    REQUIRE (markers.getMarkers().size() == 2);
    REQUIRE_FALSE (markers.deleteMarker ("gibt-es-nicht").has_value());
}

TEST_CASE ("MarkerService: nächster und voriger Marker, Abschnitt", "[services][a2]")
{
    TestEnvironment env;
    fk::services::MarkerService markers (env.projectService);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Springen").has_value());
    const auto first = *markers.addMarker (2.0);
    REQUIRE (markers.addMarker (6.0).has_value());

    REQUIRE (markers.getNextMarkerSeconds (0.0) == Catch::Approx (2.0));
    REQUIRE (markers.getNextMarkerSeconds (2.0) == Catch::Approx (6.0)); // genau auf einem: weiter
    REQUIRE_FALSE (markers.getNextMarkerSeconds (6.0).has_value());

    REQUIRE (markers.getPreviousMarkerSeconds (7.0) == Catch::Approx (6.0));
    REQUIRE (markers.getPreviousMarkerSeconds (6.1) == Catch::Approx (2.0)); // knapp dahinter: zum davor
    REQUIRE_FALSE (markers.getPreviousMarkerSeconds (1.0).has_value());

    const auto section = markers.getSection (first);
    REQUIRE (section.has_value());
    REQUIRE (section->first == Catch::Approx (2.0));
    REQUIRE (section->second == Catch::Approx (6.0));
}

TEST_CASE ("MarkerService: Marker überleben Speichern und Öffnen", "[services][a2]")
{
    TestEnvironment env;
    fk::services::MarkerService markers (env.projectService);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Gespeichert").has_value());
    REQUIRE (markers.addMarker (4.0, "Bridge").has_value());
    REQUIRE (env.projectService.saveProject().has_value());

    const auto editFile = env.projectService.getEditFile();
    env.projectService.closeProject();
    REQUIRE (markers.getMarkers().empty());
    REQUIRE (env.projectService.openProject (editFile).has_value());

    const auto list = markers.getMarkers();
    REQUIRE (list.size() == 1);
    REQUIRE (list[0].name == "Bridge");
}
