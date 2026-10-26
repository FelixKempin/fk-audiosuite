#include "fk/services/SignatureService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::ChordKind;
using fk::services::ScaleKind;
using fk::services::SignatureService;
using fk::tests::TestEnvironment;

TEST_CASE ("SignatureService: Tempopunkte als Sprung und Rampe", "[services][signature]")
{
    TestEnvironment env;
    SignatureService signature { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Tempo").has_value());

    REQUIRE (signature.getTempoPoints().size() == 1);
    REQUIRE (signature.getTempoPoints().front().bpm == Catch::Approx (120.0));

    // Bei 120 BPM liegt Schlag 8 bei 4 s. Rampe von 120 auf 60 zwischen Schlag 0 und 8.
    const auto inserted = signature.insertTempoPoint (4.0, 60.0, false);
    REQUIRE (inserted.has_value());
    REQUIRE (*inserted == 1);
    REQUIRE (signature.setTempoPoint (0, 120.0, true).has_value());

    auto points = signature.getTempoPoints();
    REQUIRE (points.size() == 2);
    REQUIRE (points[0].ramp);
    REQUIRE_FALSE (points[1].ramp);
    REQUIRE (points[1].beat == Catch::Approx (8.0));

    // In der Rampe ist das Tempo auf halbem Weg zwischen beiden; am Ende gilt der zweite Punkt.
    const auto midway = signature.getTempoAt (points[1].seconds * 0.5);
    REQUIRE (midway < 119.0);
    REQUIRE (midway > 61.0);
    REQUIRE (signature.getTempoAt (points[1].seconds + 1.0) == Catch::Approx (60.0));

    // Sprung statt Rampe: davor konstant 120.
    REQUIRE (signature.setTempoPoint (0, 120.0, false).has_value());
    REQUIRE (signature.getTempoAt (points[1].seconds * 0.5) == Catch::Approx (120.0));

    REQUIRE_FALSE (signature.removeTempoPoint (0).has_value()); // der erste bleibt
    REQUIRE (signature.removeTempoPoint (1).has_value());
    REQUIRE (signature.getTempoPoints().size() == 1);

    env.projectService.undo();
    REQUIRE (signature.getTempoPoints().size() == 2);
}

TEST_CASE ("SignatureService: Taktartwechsel landen auf dem Taktanfang", "[services][signature]")
{
    TestEnvironment env;
    SignatureService signature { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Taktart").has_value());

    // 120 BPM, 4/4: Takt 3 beginnt bei 4 s. Einfügen bei 5 s (mitten in Takt 3) rückt auf 4 s.
    const auto inserted = signature.insertTimeSignature (5.0, { 3, 4 });
    REQUIRE (inserted.has_value());

    auto points = signature.getTimeSignaturePoints();
    REQUIRE (points.size() == 2);
    REQUIRE (points[1].bar == 3);
    REQUIRE (points[1].seconds == Catch::Approx (4.0));
    REQUIRE (points[1].signature.numerator == 3);

    // Ab Takt 3 sind Takte 1,5 s lang: Takt 4 beginnt bei 5,5 s.
    REQUIRE (env.timelineService.barLengthSeconds (4.5) == Catch::Approx (1.5));
    REQUIRE (env.timelineService.formatBarsBeats (5.5).startsWith ("004.01"));

    REQUIRE_FALSE (signature.insertTimeSignature (0.0, { 0, 4 }).has_value());
    REQUIRE_FALSE (signature.removeTimeSignature (0).has_value());
    REQUIRE (signature.removeTimeSignature (1).has_value());
    REQUIRE (signature.getTimeSignaturePoints().size() == 1);
}

TEST_CASE ("SignatureService: Tonarten am Raster, Projekttonart zuerst", "[services][signature]")
{
    TestEnvironment env;
    SignatureService signature { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Tonart").has_value());

    REQUIRE (signature.getKeyPoints().size() == 1);
    REQUIRE (signature.setKey (0, 9, ScaleKind::minor).has_value());
    REQUIRE (signature.getProjectKey().name == "A Minor");

    const auto inserted = signature.insertKey (8.0, 2, ScaleKind::dorian); // Schlag 16
    REQUIRE (inserted.has_value());
    REQUIRE (signature.getKeyPoints().size() == 2);
    REQUIRE (signature.getKeyAt (2.0).name == "A Minor");
    REQUIRE (signature.getKeyAt (9.0).name == "D Dorian");
    REQUIRE (signature.getKeyAt (9.0).beat == Catch::Approx (16.0));

    REQUIRE (signature.moveKey (1, 4.0).has_value());
    REQUIRE (signature.getKeyAt (5.0).name == "D Dorian");
    REQUIRE_FALSE (signature.removeKey (0).has_value());
    REQUIRE (signature.removeKey (1).has_value());
    REQUIRE (signature.getKeyPoints().size() == 1);
}

TEST_CASE ("SignatureService: Skalentöne und Akkorde nach Stufen", "[services][signature]")
{
    // Reine Musiktheorie, ohne Projekt.
    REQUIRE (SignatureService::getScalePitchClasses (0, ScaleKind::major) == std::vector<int> { 0, 2, 4, 5, 7, 9, 11 });
    REQUIRE (SignatureService::isInScale (64, 0, ScaleKind::major));       // E in C-Dur
    REQUIRE_FALSE (SignatureService::isInScale (63, 0, ScaleKind::major)); // Es nicht
    REQUIRE (SignatureService::isInScale (63, 0, ScaleKind::minor));       // Es in C-Moll

    // C-Dur: I = C-Dur-Dreiklang, V7 = G7, ii = Dm, vii° = Bdim (Tracktion schreibt B für H).
    REQUIRE (SignatureService::getChordNotes (0, ScaleKind::major, 1, ChordKind::triad, 0, 4) ==
             std::vector<int> { 60, 64, 67 });
    REQUIRE (SignatureService::getChordNotes (0, ScaleKind::major, 5, ChordKind::seventh, 0, 4) ==
             std::vector<int> { 67, 71, 74, 77 });
    REQUIRE (SignatureService::getChordSymbol (0, ScaleKind::major, 2, ChordKind::triad) == "Dm");
    REQUIRE (SignatureService::getChordSymbol (0, ScaleKind::major, 1, ChordKind::triad) == "C");
    REQUIRE (SignatureService::getChordSymbol (0, ScaleKind::major, 5, ChordKind::seventh) == "G7");
    REQUIRE (SignatureService::getChordSymbol (0, ScaleKind::major, 1, ChordKind::seventh) == "Cmaj7");
    REQUIRE (SignatureService::getDegreeName (0, ScaleKind::major, 2, ChordKind::triad) == "ii");
    REQUIRE (SignatureService::getDegreeName (0, ScaleKind::major, 5, ChordKind::triad) == "V");
    REQUIRE (SignatureService::getDegreeName (0, ScaleKind::major, 7, ChordKind::triad) ==
             juce::String::fromUTF8 ("vii°"));

    // Erste Umkehrung: der Grundton wandert eine Oktave nach oben.
    REQUIRE (SignatureService::getChordNotes (0, ScaleKind::major, 1, ChordKind::triad, 1, 4) ==
             std::vector<int> { 64, 67, 72 });

    // A-Moll, Stufe I: Am.
    REQUIRE (SignatureService::getChordNotes (9, ScaleKind::minor, 1, ChordKind::triad, 0, 3) ==
             std::vector<int> { 57, 60, 64 });
    REQUIRE (SignatureService::getKeyName (9, ScaleKind::minor) == "A Minor");
}
