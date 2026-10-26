#include "fk/ui/NoteDrag.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using fk::services::EditableNote;
using fk::ui::NoteDrag;

namespace
{
// 64 Pixel je Schlag, Raster Sechzehntel (0,25), Zeilen 12 Pixel hoch.
const NoteDrag::Scale scale { 0.25, 64.0, 12, 128 };
const std::vector<EditableNote> twoNotes { { 0, 60, 1.0, 1.0, 100 }, { 1, 64, 2.0, 0.5, 80 } };
} // namespace

TEST_CASE ("NoteDrag: Verschieben rastet die gegriffene Note ein, alle gehen mit", "[ui][keyeditor]")
{
    NoteDrag drag;
    drag.beginEdit (NoteDrag::Mode::move, twoNotes, 0, false);

    // 70 Pixel nach rechts (1,09 Schläge → eingerastet 1,0), zwei Zeilen nach oben (+2 Halbtöne).
    drag.update ({ 70, -24 }, {}, scale);
    REQUIRE (drag.hasMoved());
    REQUIRE (drag.getPreviews().at (0).startBeats == Catch::Approx (2.0));
    REQUIRE (drag.getPreviews().at (0).noteNumber == 62);
    REQUIRE (drag.getPreviews().at (1).startBeats == Catch::Approx (3.0));
    REQUIRE (drag.getPreviews().at (1).noteNumber == 66);

    // Weit nach links: keine Note vor den Clip-Anfang, der Abstand zwischen ihnen bleibt.
    drag.update ({ -1000, 0 }, {}, scale);
    REQUIRE (drag.getPreviews().at (0).startBeats == Catch::Approx (0.0));
    REQUIRE (drag.getPreviews().at (1).startBeats == Catch::Approx (1.0));
}

TEST_CASE ("NoteDrag: Trimmen am Anfang und am Ende, Anschlag ziehen", "[ui][keyeditor]")
{
    NoteDrag drag;
    drag.beginEdit (NoteDrag::Mode::trimStart, twoNotes, 0, false);
    drag.update ({ 16, 0 }, {}, scale); // eine Sechzehntel später, Ende bleibt
    REQUIRE (drag.getPreviews().at (0).startBeats == Catch::Approx (1.25));
    REQUIRE (drag.getPreviews().at (0).lengthBeats == Catch::Approx (0.75));

    drag.beginEdit (NoteDrag::Mode::trimEnd, twoNotes, 0, false);
    drag.update ({ 32, 0 }, {}, scale); // eine Achtel länger
    REQUIRE (drag.getPreviews().at (0).lengthBeats == Catch::Approx (1.5));
    drag.update ({ -500, 0 }, {}, scale); // nie kürzer als ein Rasterschritt
    REQUIRE (drag.getPreviews().at (0).lengthBeats == Catch::Approx (0.25));

    drag.beginEdit (NoteDrag::Mode::velocity, twoNotes, 0, false);
    drag.update ({ 0, -20 }, {}, scale); // 20 Pixel hoch = +10
    REQUIRE (drag.getPreviews().at (0).velocity == 110);
    REQUIRE (drag.getPreviews().at (1).velocity == 90);
    drag.update ({ 0, -1000 }, {}, scale);
    REQUIRE (drag.getPreviews().at (0).velocity == 127);
}

TEST_CASE ("NoteDrag: Stift zieht die Länge auf, Klick behält die eingestellte Länge", "[ui][keyeditor]")
{
    NoteDrag drag;
    drag.beginDraw ({ -1, 60, 1.0, 0.5, 100 });
    REQUIRE_FALSE (drag.hasMoved());
    drag.update ({ 128, 0 }, { 64 * 3, 0 }, scale); // Maus bei Schlag 3
    REQUIRE (drag.getDrawn().lengthBeats == Catch::Approx (2.0));
    drag.update ({ 0, 0 }, { 64, 0 }, scale); // zurück: Länge fällt nie unter die eingestellte
    REQUIRE (drag.getDrawn().lengthBeats == Catch::Approx (0.5));
}
