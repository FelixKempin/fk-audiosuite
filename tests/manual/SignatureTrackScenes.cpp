#include "fk/services/SignatureService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/MarkerTrack.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/SignatureTrack.h"
#include "fk/ui/Theme.h"

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <vector>

// Bilder für docs/manual/signature-track.md. Ändert sich die Signaturspur (Lagen, Punkte, Beschriftung),
// hier die Szene anpassen und tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::findChild;
using fk::tests::manual::findDirectChild;
using fk::tests::manual::ProjectWindowScene;
using fk::tests::manual::saveShot;
using fk::tests::manual::stripCallout;

namespace
{
/// Lagennamen sind gezeichnet, keine Komponenten (kein Tooltip): Rechteck des Namens in Lage index
/// (0 Tempo, 1 Taktart, 2 Tonart), in Koordinaten von root. Rechts bleibt im Spurkopf Platz für die
/// Nummer, damit sie nicht über dem ersten Punkt der Lage liegt.
juce::Rectangle<int> laneNameArea (const juce::Rectangle<int>& signatureArea, int index)
{
    constexpr int roomForBadge = 40;
    return { signatureArea.getX() + fk::ui::theme::spacingSmall,
             signatureArea.getY() + index * fk::ui::SignatureTrack::laneHeight + 1,
             fk::ui::theme::trackHeaderWidth - fk::ui::theme::spacingSmall - roomForBadge,
             fk::ui::SignatureTrack::laneHeight - 2 };
}

/// Senkrechte Markierung (Nummer über dem Bild) für einen Punkt der Lage index an x.
Callout pointCallout (int number, const juce::Rectangle<int>& signatureArea, int index, int x)
{
    return { number,
             { x - 3, signatureArea.getY() + index * fk::ui::SignatureTrack::laneHeight + 2, 7, fk::ui::SignatureTrack::laneHeight - 4 },
             Callout::Placement::above };
}
} // namespace

TEST_CASE ("Handbuch: Signaturspur mit Rampe, Sprung, Taktart- und Tonartwechsel", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    // Zustand wie in signature-track.md §2 beschrieben (Positionen in Viertelnoten ab Projektanfang):
    //   Tempo: 120 BPM als Rampe bis 90 BPM auf Takt 3 (Schlag 8), dort Sprung; 130 BPM ab Takt 6 (Schlag 19).
    //   Taktart: 4/4, ab Takt 5 (Schlag 16) 3/4.  Tonart: A Minor, ab Takt 7 (Schlag 22) C Major.
    // Alles vor dem Fenster einstellen (spätere Projektänderungen können Rückfragen auslösen).
    ProjectWindowScene scene;
    auto& signature = scene.services.signature;
    const auto& timeline = scene.env.timelineService;

    REQUIRE (signature.insertTempoPoint (timeline.beatsToSeconds (8.0), 90.0, false).has_value());
    REQUIRE (signature.setTempoPoint (0, 120.0, true).has_value());
    REQUIRE (signature.insertTimeSignature (timeline.beatsToSeconds (16.0), { 3, 4 }).has_value());
    REQUIRE (signature.insertTempoPoint (timeline.beatsToSeconds (19.0), 130.0, false).has_value());
    REQUIRE (signature.setKey (0, 9, fk::services::ScaleKind::minor).has_value());
    REQUIRE (signature.insertKey (timeline.beatsToSeconds (22.0), 0, fk::services::ScaleKind::major).has_value());

    const auto tempoPoints = signature.getTempoPoints();
    const auto timeSignatures = signature.getTimeSignaturePoints();
    const auto keys = signature.getKeyPoints();
    REQUIRE (tempoPoints.size() == 3);
    REQUIRE (timeSignatures.size() == 2);
    REQUIRE (keys.size() == 2);
    CHECK (timeSignatures[1].bar == 5);

    auto& window = scene.open (1000, 760);
    scene.zoomToProject();
    TestEnvironment::pumpMessages (200);

    auto& arrange = findDirectChild<fk::ui::ArrangeView> (window);
    auto& signatureTrack = findChild<fk::ui::SignatureTrack> (arrange);
    auto& markerTrack = findChild<fk::ui::MarkerTrack> (arrange);
    const auto signatureArea = boundsIn (window, signatureTrack);
    const auto markerArea = boundsIn (window, markerTrack);
    const auto arrangeArea = boundsIn (window, arrange);
    const auto mapping = arrange.getTimeMapping();
    const auto xAt = [&mapping, &signatureArea] (double seconds)
    { return signatureArea.getX() + fk::ui::theme::trackHeaderWidth + mapping.secondsToX (seconds); };

    // Lineal (über der Signaturspur), Signatur- und Markerspur und ein Streifen der ersten Spur.
    const int rulerTop = signatureArea.getY() - fk::ui::theme::rulerHeight;
    const auto shotArea = juce::Rectangle<int>::leftTopRightBottom (
        arrangeArea.getX(), rulerTop, arrangeArea.getRight(), juce::jmin (arrangeArea.getBottom(), markerArea.getBottom() + 40));

    if (xAt (keys[1].seconds) > shotArea.getRight() - 20)
        WARN ("Tonartwechsel liegt rechts außerhalb des Bildes (Zoom?): Nummer 7 fehlt");

    // Nummern wie in signature-track.md §2: 1–3 Lagen, 4 Rampe, 5 Sprung, 6 Taktart 3/4, 7 Tonart C Major, 8 Markerspur.
    saveShot (window,
              "signature-track/overview",
              { Callout { 1, laneNameArea (signatureArea, 0), Callout::Placement::inside },
                Callout { 2, laneNameArea (signatureArea, 1), Callout::Placement::inside },
                Callout { 3, laneNameArea (signatureArea, 2), Callout::Placement::inside },
                pointCallout (4, signatureArea, 0, xAt (tempoPoints[0].seconds)),
                pointCallout (5, signatureArea, 0, xAt (tempoPoints[1].seconds)),
                pointCallout (6, signatureArea, 1, xAt (timeSignatures[1].seconds)),
                pointCallout (7, signatureArea, 2, xAt (keys[1].seconds)),
                stripCallout (8, markerArea) },
              shotArea);
}
