#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestEnvironment.h"

using fk::services::GridResolution;
using fk::tests::TestEnvironment;

TEST_CASE ("TimelineService: Standard ist 120 BPM im 4/4", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Tempo").has_value());

    REQUIRE (env.timelineService.getTempoBpm() == Catch::Approx (120.0));
    REQUIRE (env.timelineService.getTimeSignature().numerator == 4);
    REQUIRE (env.timelineService.getTimeSignature().denominator == 4);
}

TEST_CASE ("TimelineService: formatBarsBeats zählt ab Takt 1 Schlag 1", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Takte").has_value());

    // 120 BPM: ein Schlag = 0,5 s, ein 4/4-Takt = 2 s.
    REQUIRE (env.timelineService.formatBarsBeats (0.0) == "001.01.000");
    REQUIRE (env.timelineService.formatBarsBeats (0.5) == "001.02.000");
    REQUIRE (env.timelineService.formatBarsBeats (2.0) == "002.01.000");
    REQUIRE (env.timelineService.formatBarsBeats (2.25) == "002.01.240");
}

TEST_CASE ("TimelineService: snapSeconds rastet auf Schlag und Takt", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Snap").has_value());

    REQUIRE (env.timelineService.snapSeconds (0.6, GridResolution::beat()) == Catch::Approx (0.5));
    REQUIRE (env.timelineService.snapSeconds (0.76, GridResolution::beat()) == Catch::Approx (1.0));
    REQUIRE (env.timelineService.snapSeconds (2.9, GridResolution::bar()) == Catch::Approx (2.0));
    REQUIRE (env.timelineService.snapSeconds (0.13, GridResolution::of ({ 16 })) == Catch::Approx (0.125));
}

TEST_CASE ("TimelineService: Tempoänderung verschiebt das Raster", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Tempowechsel").has_value());

    env.timelineService.setTempoBpm (60.0);

    REQUIRE (env.timelineService.getTempoBpm() == Catch::Approx (60.0));
    REQUIRE (env.timelineService.formatBarsBeats (4.0) == "002.01.000");

    const auto lines = env.timelineService.getGridLines (0.0, 4.0, GridResolution::beat());
    REQUIRE (lines.size() == 5);
    REQUIRE (lines[0].isBarStart);
    REQUIRE (lines[0].barNumber == 1);
    REQUIRE_FALSE (lines[1].isBarStart);
    REQUIRE (lines[4].isBarStart);
    REQUIRE (lines[4].barNumber == 2);
}

TEST_CASE ("TimelineService: Taktart 6/8 macht den Takt drei Viertel lang", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "SechsAchtel").has_value());

    env.timelineService.setTimeSignature ({ 6, 8 });

    // 120 BPM: Viertel = 0,5 s; 6/8 = drei Viertel = 1,5 s pro Takt.
    REQUIRE (env.timelineService.snapSeconds (1.4, GridResolution::bar()) == Catch::Approx (1.5));
    REQUIRE (env.timelineService.formatBarsBeats (1.5) == "002.01.000");
}

TEST_CASE ("TimelineService: Loop und Metronom werden im Projekt gehalten", "[services][timeline]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Loop").has_value());

    env.timelineService.setLoopRange ({ 3.0, 1.0 }); // verdreht angegeben, wird sortiert
    env.timelineService.setLooping (true);
    env.timelineService.setMetronomeEnabled (true);

    const auto loop = env.timelineService.getLoopRange();
    REQUIRE (loop.startSeconds == Catch::Approx (1.0));
    REQUIRE (loop.endSeconds == Catch::Approx (3.0));
    REQUIRE (env.timelineService.isLooping());
    REQUIRE (env.timelineService.isMetronomeEnabled());
}

TEST_CASE ("TimelineService: Raster mit Notenwerten, Triolen, Sekunden und Samples", "[services][a4]")
{
    TestEnvironment env;
    REQUIRE (env.projectService.createProject (env.tempFolder, "Raster").has_value());
    env.timelineService.setTempoBpm (120.0); // eine Viertel = 0,5 s

    using fk::services::NoteValue;
    const auto eighthTriplet = GridResolution::of ({ 8, NoteValue::Feel::triplet }); // 1/3 Viertel = 1/6 s
    const auto dottedEighth = GridResolution::of ({ 8, NoteValue::Feel::dotted });   // 0,75 Viertel = 0,375 s

    REQUIRE (env.timelineService.snapSeconds (0.2, eighthTriplet) == Catch::Approx (1.0 / 6.0));
    REQUIRE (env.timelineService.snapSeconds (0.4, dottedEighth) == Catch::Approx (0.375));
    REQUIRE (env.timelineService.snapSeconds (1.4, GridResolution::seconds()) == Catch::Approx (1.0));

    const auto sampleSnapped = env.timelineService.snapSeconds (0.123456789, GridResolution::samples());
    REQUIRE (std::abs (sampleSnapped - 0.123456789) < 1.0 / 8000.0);

    REQUIRE (env.timelineService.gridStepSeconds (0.0, eighthTriplet) == Catch::Approx (1.0 / 6.0));
    REQUIRE (env.timelineService.gridStepSeconds (3.0, GridResolution::bar()) == Catch::Approx (2.0));
    REQUIRE (env.timelineService.gridStepSeconds (3.0, GridResolution::seconds()) == Catch::Approx (1.0));

    // Ausdünnen: 1/32 sind 1/16 s; mit 0,2 s Mindestabstand bleibt nur jede vierte Linie und alle Taktanfänge.
    const auto all = env.timelineService.getGridLines (0.0, 4.0, GridResolution::of ({ 32 }));
    const auto thinned = env.timelineService.getGridLines (0.0, 4.0, GridResolution::of ({ 32 }), 0.2);
    REQUIRE (all.size() == 65);
    REQUIRE (thinned.size() < 25);
    REQUIRE (std::ranges::count_if (thinned, [] (const auto& line) { return line.isBarStart; }) == 3);

    // Zeitbasierte Raster zeichnen Zählzeiten (das Lineal bleibt musikalisch).
    REQUIRE (env.timelineService.getGridLines (0.0, 2.0, GridResolution::seconds()).size() == 5);
    REQUIRE (GridResolution::of ({ 8, NoteValue::Feel::triplet }).shortName() == "1/8T");
}
