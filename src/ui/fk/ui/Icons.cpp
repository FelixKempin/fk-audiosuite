#include "fk/ui/Icons.h"

namespace fk::ui
{

namespace
{
constexpr float stroke = 0.09f;

// Hilfen: Alle Koordinaten im Einheitsquadrat, damit die Symbole beliebig skalierbar sind.
void line (juce::Path& path, float x1, float y1, float x2, float y2)
{
    juce::Path segment;
    segment.addLineSegment ({ x1, y1, x2, y2 }, stroke);
    path.addPath (segment);
}

void rect (juce::Path& path, float x, float y, float w, float h, bool filled)
{
    if (filled)
        path.addRectangle (x, y, w, h);
    else
    {
        juce::Path outline;
        outline.addRectangle (x, y, w, h);
        juce::PathStrokeType (stroke).createStrokedPath (outline, outline);
        path.addPath (outline);
    }
}

void triangleRight (juce::Path& path, float x, float y, float w, float h)
{
    path.addTriangle (x, y, x + w, y + h * 0.5f, x, y + h);
}

void circleOutline (juce::Path& path, float cx, float cy, float r)
{
    juce::Path ring;
    ring.addEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
    juce::PathStrokeType (stroke).createStrokedPath (ring, ring);
    path.addPath (ring);
}

void arrowHead (juce::Path& path, float tipX, float tipY, float dirX, float dirY)
{
    // Pfeilspitze als gefülltes Dreieck in Richtung (dirX, dirY), normiert.
    const float length = std::sqrt (dirX * dirX + dirY * dirY);
    const float ux = dirX / length, uy = dirY / length;
    const float size = 0.22f;
    path.addTriangle (tipX,
                      tipY,
                      tipX - ux * size - uy * size * 0.6f,
                      tipY - uy * size + ux * size * 0.6f,
                      tipX - ux * size + uy * size * 0.6f,
                      tipY - uy * size - ux * size * 0.6f);
}
} // namespace

juce::Path makeIconPath (Icon icon)
{
    juce::Path p;

    switch (icon)
    {
        case Icon::newProject: // Blatt mit Plus
            line (p, 0.25f, 0.1f, 0.6f, 0.1f);
            line (p, 0.6f, 0.1f, 0.8f, 0.3f);
            line (p, 0.8f, 0.3f, 0.8f, 0.9f);
            line (p, 0.8f, 0.9f, 0.25f, 0.9f);
            line (p, 0.25f, 0.9f, 0.25f, 0.1f);
            line (p, 0.52f, 0.45f, 0.52f, 0.75f);
            line (p, 0.37f, 0.6f, 0.67f, 0.6f);
            break;

        case Icon::open: // Ordner
            line (p, 0.1f, 0.25f, 0.4f, 0.25f);
            line (p, 0.4f, 0.25f, 0.5f, 0.35f);
            line (p, 0.5f, 0.35f, 0.9f, 0.35f);
            line (p, 0.9f, 0.35f, 0.9f, 0.8f);
            line (p, 0.9f, 0.8f, 0.1f, 0.8f);
            line (p, 0.1f, 0.8f, 0.1f, 0.25f);
            break;

        case Icon::save: // Diskette
            rect (p, 0.15f, 0.15f, 0.7f, 0.7f, false);
            rect (p, 0.3f, 0.15f, 0.4f, 0.22f, true);
            rect (p, 0.3f, 0.6f, 0.4f, 0.25f, false);
            break;

        case Icon::exportFile: // Pfeil aus einer Box heraus nach rechts oben
            line (p, 0.15f, 0.35f, 0.15f, 0.85f);
            line (p, 0.15f, 0.85f, 0.65f, 0.85f);
            line (p, 0.65f, 0.85f, 0.65f, 0.6f);
            line (p, 0.4f, 0.55f, 0.82f, 0.13f);
            arrowHead (p, 0.86f, 0.1f, 0.42f, -0.42f);
            break;

        case Icon::addAudioTrack: // Wellenform mit Plus
            line (p, 0.08f, 0.5f, 0.2f, 0.5f);
            line (p, 0.2f, 0.5f, 0.28f, 0.25f);
            line (p, 0.28f, 0.25f, 0.38f, 0.75f);
            line (p, 0.38f, 0.75f, 0.46f, 0.4f);
            line (p, 0.46f, 0.4f, 0.52f, 0.5f);
            line (p, 0.52f, 0.5f, 0.6f, 0.5f);
            line (p, 0.78f, 0.35f, 0.78f, 0.65f);
            line (p, 0.63f, 0.5f, 0.93f, 0.5f);
            break;

        case Icon::addInstrumentTrack: // Klaviertasten
            rect (p, 0.1f, 0.2f, 0.8f, 0.6f, false);
            line (p, 0.37f, 0.2f, 0.37f, 0.8f);
            line (p, 0.63f, 0.2f, 0.63f, 0.8f);
            rect (p, 0.3f, 0.2f, 0.14f, 0.35f, true);
            rect (p, 0.56f, 0.2f, 0.14f, 0.35f, true);
            break;

        case Icon::addGroupTrack: // Drei gestapelte Ebenen
            line (p, 0.15f, 0.35f, 0.5f, 0.18f);
            line (p, 0.5f, 0.18f, 0.85f, 0.35f);
            line (p, 0.85f, 0.35f, 0.5f, 0.52f);
            line (p, 0.5f, 0.52f, 0.15f, 0.35f);
            line (p, 0.15f, 0.55f, 0.5f, 0.72f);
            line (p, 0.5f, 0.72f, 0.85f, 0.55f);
            line (p, 0.15f, 0.72f, 0.5f, 0.89f);
            line (p, 0.5f, 0.89f, 0.85f, 0.72f);
            break;

        case Icon::addFxTrack: // Hall: drei auslaufende Bögen
        {
            for (int i = 0; i < 3; ++i)
            {
                juce::Path arc;
                const float r = 0.18f + static_cast<float> (i) * 0.14f;
                arc.addCentredArc (0.2f,
                                   0.5f,
                                   r,
                                   r,
                                   0.0f,
                                   juce::MathConstants<float>::pi * 0.15f,
                                   juce::MathConstants<float>::pi * 0.85f,
                                   true);
                juce::PathStrokeType (stroke).createStrokedPath (arc, arc);
                p.addPath (arc);
            }
            p.addEllipse (0.12f, 0.42f, 0.16f, 0.16f);
            break;
        }

        case Icon::undo: // Bogen nach links mit Pfeil
        {
            juce::Path arc;
            arc.addCentredArc (0.5f,
                               0.55f,
                               0.32f,
                               0.32f,
                               0.0f,
                               juce::MathConstants<float>::pi * 0.1f,
                               juce::MathConstants<float>::pi * 1.55f,
                               true);
            juce::PathStrokeType (stroke).createStrokedPath (arc, arc);
            p.addPath (arc);
            arrowHead (p, 0.14f, 0.42f, -0.2f, 0.8f);
            break;
        }

        case Icon::redo:
        {
            juce::Path arc;
            arc.addCentredArc (0.5f,
                               0.55f,
                               0.32f,
                               0.32f,
                               0.0f,
                               -juce::MathConstants<float>::pi * 1.55f,
                               -juce::MathConstants<float>::pi * 0.1f,
                               true);
            juce::PathStrokeType (stroke).createStrokedPath (arc, arc);
            p.addPath (arc);
            arrowHead (p, 0.86f, 0.42f, 0.2f, 0.8f);
            break;
        }

        case Icon::audioSettings: // Zahnrad (vereinfacht: Ring + Zacken)
            circleOutline (p, 0.5f, 0.5f, 0.2f);
            for (int i = 0; i < 8; ++i)
            {
                const float angle = static_cast<float> (i) * juce::MathConstants<float>::twoPi / 8.0f;
                line (p,
                      0.5f + std::cos (angle) * 0.3f,
                      0.5f + std::sin (angle) * 0.3f,
                      0.5f + std::cos (angle) * 0.44f,
                      0.5f + std::sin (angle) * 0.44f);
            }
            break;

        case Icon::plugins: // Stecker
            rect (p, 0.3f, 0.35f, 0.4f, 0.35f, false);
            line (p, 0.38f, 0.12f, 0.38f, 0.35f);
            line (p, 0.62f, 0.12f, 0.62f, 0.35f);
            line (p, 0.5f, 0.7f, 0.5f, 0.9f);
            break;

        case Icon::toolSelect: // Mauszeiger
            p.addTriangle (0.25f, 0.12f, 0.25f, 0.78f, 0.45f, 0.6f);
            p.addTriangle (0.25f, 0.12f, 0.45f, 0.6f, 0.75f, 0.55f);
            line (p, 0.45f, 0.6f, 0.6f, 0.9f);
            break;

        case Icon::toolSplit: // Schere
            circleOutline (p, 0.28f, 0.72f, 0.12f);
            circleOutline (p, 0.72f, 0.72f, 0.12f);
            line (p, 0.36f, 0.62f, 0.78f, 0.12f);
            line (p, 0.64f, 0.62f, 0.22f, 0.12f);
            break;

        case Icon::toolErase: // Radierer
        {
            juce::Path body;
            body.addRoundedRectangle (0.15f, 0.35f, 0.7f, 0.3f, 0.06f);
            body.applyTransform (juce::AffineTransform::rotation (-0.6f, 0.5f, 0.5f));
            juce::PathStrokeType (stroke).createStrokedPath (body, body);
            p.addPath (body);
            line (p, 0.2f, 0.88f, 0.8f, 0.88f);
            break;
        }

        case Icon::toolDraw: // Stift, diagonal
            line (p, 0.2f, 0.8f, 0.7f, 0.3f);
            line (p, 0.3f, 0.9f, 0.8f, 0.4f);
            line (p, 0.2f, 0.8f, 0.3f, 0.9f);
            line (p, 0.7f, 0.3f, 0.8f, 0.4f);
            p.addTriangle (0.14f, 0.9f, 0.2f, 0.8f, 0.3f, 0.9f);
            break;

        case Icon::keyEditor: // Pianorolle: Tasten links, Balken rechts
            rect (p, 0.1f, 0.15f, 0.2f, 0.7f, false);
            line (p, 0.1f, 0.38f, 0.3f, 0.38f);
            line (p, 0.1f, 0.62f, 0.3f, 0.62f);
            rect (p, 0.4f, 0.2f, 0.3f, 0.12f, true);
            rect (p, 0.55f, 0.44f, 0.35f, 0.12f, true);
            rect (p, 0.4f, 0.68f, 0.45f, 0.12f, true);
            break;

        case Icon::snap: // Magnet
        {
            juce::Path arc;
            arc.addCentredArc (0.5f,
                               0.45f,
                               0.3f,
                               0.3f,
                               0.0f,
                               juce::MathConstants<float>::pi * 0.5f,
                               juce::MathConstants<float>::pi * 1.5f,
                               true);
            juce::PathStrokeType (stroke * 2.2f).createStrokedPath (arc, arc);
            p.addPath (arc);
            rect (p, 0.12f, 0.45f, 0.18f, 0.4f, true);
            rect (p, 0.7f, 0.45f, 0.18f, 0.4f, true);
            break;
        }

        case Icon::inspector: // Fenster mit linker Spalte
            rect (p, 0.1f, 0.15f, 0.8f, 0.7f, false);
            rect (p, 0.1f, 0.15f, 0.28f, 0.7f, true);
            break;

        case Icon::lowerZone: // Fenster mit unterer Zeile
            rect (p, 0.1f, 0.15f, 0.8f, 0.7f, false);
            rect (p, 0.1f, 0.58f, 0.8f, 0.27f, true);
            break;

        case Icon::mixer: // Drei Fader
            for (int i = 0; i < 3; ++i)
            {
                const float x = 0.25f + static_cast<float> (i) * 0.25f;
                line (p, x, 0.12f, x, 0.88f);
                rect (p, x - 0.09f, 0.3f + static_cast<float> (i) * 0.15f, 0.18f, 0.12f, true);
            }
            break;

        case Icon::keyboard: // Klaviatur
            rect (p, 0.1f, 0.25f, 0.8f, 0.5f, false);
            for (int i = 1; i < 5; ++i)
                line (p, 0.1f + static_cast<float> (i) * 0.16f, 0.25f, 0.1f + static_cast<float> (i) * 0.16f, 0.75f);
            break;

        case Icon::returnToStart:
            rect (p, 0.18f, 0.25f, 0.1f, 0.5f, true);
            p.addTriangle (0.78f, 0.25f, 0.78f, 0.75f, 0.32f, 0.5f);
            break;

        case Icon::play:
            triangleRight (p, 0.28f, 0.2f, 0.5f, 0.6f);
            break;

        case Icon::stop:
            rect (p, 0.25f, 0.25f, 0.5f, 0.5f, true);
            break;

        case Icon::record:
            p.addEllipse (0.25f, 0.25f, 0.5f, 0.5f);
            break;

        case Icon::loop: // Zwei gegenläufige Bögen mit Pfeilen
        {
            juce::Path arcs;
            arcs.addCentredArc (0.5f,
                                0.5f,
                                0.32f,
                                0.26f,
                                0.0f,
                                juce::MathConstants<float>::pi * 1.15f,
                                juce::MathConstants<float>::pi * 1.95f,
                                true);
            arcs.addCentredArc (0.5f,
                                0.5f,
                                0.32f,
                                0.26f,
                                0.0f,
                                juce::MathConstants<float>::pi * 0.15f,
                                juce::MathConstants<float>::pi * 0.95f,
                                true);
            juce::PathStrokeType (stroke).createStrokedPath (arcs, arcs);
            p.addPath (arcs);
            arrowHead (p, 0.82f, 0.4f, 0.1f, 0.8f);
            arrowHead (p, 0.18f, 0.6f, -0.1f, -0.8f);
            break;
        }

        case Icon::automation: // Kurve mit zwei Punkten
        case Icon::automationRead:
        case Icon::automationWrite:
        {
            line (p, 0.1f, 0.8f, 0.38f, 0.3f);
            line (p, 0.38f, 0.3f, 0.62f, 0.62f);
            line (p, 0.62f, 0.62f, 0.9f, 0.2f);
            p.addEllipse (0.3f, 0.22f, 0.16f, 0.16f);
            p.addEllipse (0.54f, 0.54f, 0.16f, 0.16f);

            if (icon == Icon::automationRead) // kleines Play-Dreieck unten rechts
                p.addTriangle (0.68f, 0.72f, 0.68f, 0.98f, 0.92f, 0.85f);
            else if (icon == Icon::automationWrite) // Aufnahmepunkt unten rechts
                p.addEllipse (0.68f, 0.72f, 0.26f, 0.26f);
            break;
        }

        case Icon::tracksSmaller: // Drei enge Zeilen
            rect (p, 0.15f, 0.3f, 0.7f, 0.1f, true);
            rect (p, 0.15f, 0.45f, 0.7f, 0.1f, true);
            rect (p, 0.15f, 0.6f, 0.7f, 0.1f, true);
            break;

        case Icon::tracksLarger: // Zwei hohe Zeilen
            rect (p, 0.15f, 0.15f, 0.7f, 0.3f, true);
            rect (p, 0.15f, 0.55f, 0.7f, 0.3f, true);
            break;

        case Icon::plus:
            rect (p, 0.44f, 0.15f, 0.12f, 0.7f, true);
            rect (p, 0.15f, 0.44f, 0.7f, 0.12f, true);
            break;

        case Icon::minus:
            rect (p, 0.15f, 0.44f, 0.7f, 0.12f, true);
            break;

        case Icon::cut: // Schere, leicht gekippt gegenüber dem Schneidewerkzeug
            circleOutline (p, 0.25f, 0.75f, 0.11f);
            circleOutline (p, 0.55f, 0.85f, 0.11f);
            line (p, 0.32f, 0.66f, 0.85f, 0.15f);
            line (p, 0.6f, 0.75f, 0.3f, 0.15f);
            break;

        case Icon::copy: // zwei Blätter
            rect (p, 0.15f, 0.15f, 0.5f, 0.55f, false);
            rect (p, 0.35f, 0.32f, 0.5f, 0.55f, false);
            break;

        case Icon::paste: // Klemmbrett mit Klammer
            rect (p, 0.2f, 0.22f, 0.6f, 0.68f, false);
            rect (p, 0.35f, 0.12f, 0.3f, 0.16f, true);
            line (p, 0.32f, 0.5f, 0.68f, 0.5f);
            line (p, 0.32f, 0.66f, 0.6f, 0.66f);
            break;

        case Icon::duplicate: // Block und Kopie dahinter mit Plus
            rect (p, 0.12f, 0.3f, 0.36f, 0.4f, true);
            rect (p, 0.55f, 0.3f, 0.36f, 0.4f, false);
            rect (p, 0.69f, 0.42f, 0.08f, 0.16f, true);
            rect (p, 0.65f, 0.46f, 0.16f, 0.08f, true);
            break;

        case Icon::glue: // zwei Blöcke, ein Band verbindet sie
            rect (p, 0.1f, 0.3f, 0.36f, 0.4f, true);
            rect (p, 0.54f, 0.3f, 0.36f, 0.4f, true);
            rect (p, 0.4f, 0.42f, 0.2f, 0.16f, true);
            line (p, 0.5f, 0.18f, 0.5f, 0.82f);
            break;

        case Icon::octaveDown: // Pfeil nach links über Tasten
            arrowHead (p, 0.15f, 0.3f, -1.0f, 0.0f);
            line (p, 0.3f, 0.3f, 0.85f, 0.3f);
            rect (p, 0.15f, 0.55f, 0.7f, 0.3f, false);
            line (p, 0.38f, 0.55f, 0.38f, 0.85f);
            line (p, 0.62f, 0.55f, 0.62f, 0.85f);
            break;

        case Icon::octaveUp: // Pfeil nach rechts über Tasten
            arrowHead (p, 0.85f, 0.3f, 1.0f, 0.0f);
            line (p, 0.15f, 0.3f, 0.7f, 0.3f);
            rect (p, 0.15f, 0.55f, 0.7f, 0.3f, false);
            line (p, 0.38f, 0.55f, 0.38f, 0.85f);
            line (p, 0.62f, 0.55f, 0.62f, 0.85f);
            break;

        case Icon::openPlugin: // Fenster mit Titelleiste und einem Regler
            rect (p, 0.12f, 0.15f, 0.76f, 0.7f, false);
            rect (p, 0.12f, 0.15f, 0.76f, 0.14f, true);
            circleOutline (p, 0.5f, 0.6f, 0.14f);
            line (p, 0.5f, 0.6f, 0.58f, 0.5f);
            break;

        case Icon::midiLearn: // DIN-Buchse (Kreis mit Pins) und Pfeil darauf
            circleOutline (p, 0.38f, 0.55f, 0.28f);
            p.addEllipse (0.3f, 0.47f, 0.07f, 0.07f);
            p.addEllipse (0.4f, 0.4f, 0.07f, 0.07f);
            p.addEllipse (0.5f, 0.47f, 0.07f, 0.07f);
            line (p, 0.9f, 0.12f, 0.62f, 0.4f);
            arrowHead (p, 0.6f, 0.42f, -1.0f, 1.0f);
            break;

        case Icon::bake: // Backofen: Gehäuse, Sichtfenster, zwei Drehknöpfe oben
            rect (p, 0.12f, 0.18f, 0.76f, 0.7f, false);
            rect (p, 0.26f, 0.42f, 0.48f, 0.32f, false);
            p.addEllipse (0.24f, 0.25f, 0.1f, 0.1f);
            p.addEllipse (0.42f, 0.25f, 0.1f, 0.1f);
            line (p, 0.62f, 0.3f, 0.78f, 0.3f);
            break;

        case Icon::locked: // Vorhängeschloss: Bügel und gefüllter Körper
        {
            juce::Path shackle;
            shackle.addCentredArc (0.5f,
                                   0.42f,
                                   0.2f,
                                   0.22f,
                                   0.0f,
                                   -juce::MathConstants<float>::halfPi,
                                   juce::MathConstants<float>::halfPi,
                                   true);
            juce::PathStrokeType (stroke).createStrokedPath (shackle, shackle);
            p.addPath (shackle);
            line (p, 0.3f, 0.42f, 0.3f, 0.5f);
            line (p, 0.7f, 0.42f, 0.7f, 0.5f);
            rect (p, 0.2f, 0.5f, 0.6f, 0.4f, true);
            break;
        }

        case Icon::speaker: // Lautsprecher: Korb und Membran, zwei Schallbögen
        {
            p.addRectangle (0.12f, 0.38f, 0.16f, 0.24f);
            p.addTriangle (0.28f, 0.38f, 0.52f, 0.16f, 0.52f, 0.84f);
            p.addTriangle (0.28f, 0.38f, 0.52f, 0.84f, 0.28f, 0.62f);
            juce::Path waves;
            waves.addCentredArc (0.52f, 0.5f, 0.16f, 0.16f, 0.0f, 0.6f, 2.54f, true);
            waves.startNewSubPath (0.52f + 0.3f * std::sin (0.6f), 0.5f - 0.3f * std::cos (0.6f));
            waves.addCentredArc (0.52f, 0.5f, 0.3f, 0.3f, 0.0f, 0.6f, 2.54f, false);
            juce::PathStrokeType (stroke).createStrokedPath (waves, waves);
            p.addPath (waves);
            break;
        }

        case Icon::saveAs: // Diskette links unten, Plus rechts oben
            rect (p, 0.1f, 0.3f, 0.56f, 0.58f, false);
            rect (p, 0.22f, 0.3f, 0.32f, 0.18f, true);
            line (p, 0.8f, 0.08f, 0.8f, 0.4f);
            line (p, 0.64f, 0.24f, 0.96f, 0.24f);
            break;

        case Icon::recent: // Uhr mit zwei Zeigern
            circleOutline (p, 0.5f, 0.5f, 0.36f);
            line (p, 0.5f, 0.5f, 0.5f, 0.27f);
            line (p, 0.5f, 0.5f, 0.68f, 0.6f);
            break;

        case Icon::settings: // drei waagrechte Schieberegler mit Knopf an verschiedenen Stellen
            for (const auto& [y, knob] : { std::pair { 0.25f, 0.32f }, { 0.5f, 0.66f }, { 0.75f, 0.44f } })
            {
                line (p, 0.12f, y, 0.88f, y);
                p.addEllipse (knob - 0.08f, y - 0.08f, 0.16f, 0.16f);
            }
            break;

        case Icon::eye: // Auge: Mandelform aus zwei Bögen, Pupille
        {
            juce::Path outline;
            outline.startNewSubPath (0.08f, 0.5f);
            outline.quadraticTo (0.5f, 0.1f, 0.92f, 0.5f);
            outline.quadraticTo (0.5f, 0.9f, 0.08f, 0.5f);
            outline.closeSubPath();
            juce::PathStrokeType (stroke).createStrokedPath (outline, outline);
            p.addPath (outline);
            p.addEllipse (0.38f, 0.38f, 0.24f, 0.24f);
            break;
        }

        case Icon::markerAdd: // Fahne, Plus rechts unten
            line (p, 0.25f, 0.12f, 0.25f, 0.88f);
            p.addTriangle (0.27f, 0.12f, 0.7f, 0.28f, 0.27f, 0.44f);
            line (p, 0.72f, 0.56f, 0.72f, 0.92f);
            line (p, 0.54f, 0.74f, 0.9f, 0.74f);
            break;

        case Icon::markerPrevious: // Pfeil nach links, dahinter die Fahne
            line (p, 0.6f, 0.12f, 0.6f, 0.88f);
            p.addTriangle (0.62f, 0.12f, 0.92f, 0.26f, 0.62f, 0.4f);
            line (p, 0.1f, 0.62f, 0.5f, 0.62f);
            arrowHead (p, 0.08f, 0.62f, -1.0f, 0.0f);
            break;

        case Icon::markerNext: // Fahne, davor der Pfeil nach rechts
            line (p, 0.2f, 0.12f, 0.2f, 0.88f);
            p.addTriangle (0.22f, 0.12f, 0.52f, 0.26f, 0.22f, 0.4f);
            line (p, 0.3f, 0.62f, 0.86f, 0.62f);
            arrowHead (p, 0.92f, 0.62f, 1.0f, 0.0f);
            break;

        case Icon::folder: // Ordner mit Reiter
        {
            juce::Path shape;
            shape.startNewSubPath (0.1f, 0.25f);
            shape.lineTo (0.4f, 0.25f);
            shape.lineTo (0.48f, 0.35f);
            shape.lineTo (0.9f, 0.35f);
            shape.lineTo (0.9f, 0.82f);
            shape.lineTo (0.1f, 0.82f);
            shape.closeSubPath();
            juce::PathStrokeType (stroke).createStrokedPath (shape, shape);
            p.addPath (shape);
            break;
        }

        case Icon::expanded: // Dreieck nach unten
            p.addTriangle (0.22f, 0.32f, 0.78f, 0.32f, 0.5f, 0.72f);
            break;

        case Icon::collapsed: // Dreieck nach rechts
            p.addTriangle (0.32f, 0.22f, 0.72f, 0.5f, 0.32f, 0.78f);
            break;

        case Icon::rewind:
            p.addTriangle (0.48f, 0.25f, 0.48f, 0.75f, 0.12f, 0.5f);
            p.addTriangle (0.86f, 0.25f, 0.86f, 0.75f, 0.5f, 0.5f);
            break;

        case Icon::fastForward:
            p.addTriangle (0.14f, 0.25f, 0.14f, 0.75f, 0.5f, 0.5f);
            p.addTriangle (0.52f, 0.25f, 0.52f, 0.75f, 0.88f, 0.5f);
            break;

        case Icon::punch: // Bereich in Klammern mit Aufnahmepunkt
            line (p, 0.2f, 0.2f, 0.2f, 0.8f);
            line (p, 0.2f, 0.2f, 0.3f, 0.2f);
            line (p, 0.2f, 0.8f, 0.3f, 0.8f);
            line (p, 0.8f, 0.2f, 0.8f, 0.8f);
            line (p, 0.8f, 0.2f, 0.7f, 0.2f);
            line (p, 0.8f, 0.8f, 0.7f, 0.8f);
            p.addEllipse (0.37f, 0.37f, 0.26f, 0.26f);
            break;

        case Icon::preRoll: // Pfeil von links an eine Klammer
            line (p, 0.1f, 0.5f, 0.55f, 0.5f);
            arrowHead (p, 0.62f, 0.5f, 1.0f, 0.0f);
            line (p, 0.78f, 0.2f, 0.78f, 0.8f);
            line (p, 0.78f, 0.2f, 0.9f, 0.2f);
            line (p, 0.78f, 0.8f, 0.9f, 0.8f);
            break;

        case Icon::postRoll: // Klammer, Pfeil nach rechts heraus
            line (p, 0.22f, 0.2f, 0.22f, 0.8f);
            line (p, 0.22f, 0.2f, 0.1f, 0.2f);
            line (p, 0.22f, 0.8f, 0.1f, 0.8f);
            line (p, 0.38f, 0.5f, 0.82f, 0.5f);
            arrowHead (p, 0.9f, 0.5f, 1.0f, 0.0f);
            break;

        case Icon::toolMute: // Lautsprecher, schräg durchgestrichen
            p.addRectangle (0.14f, 0.38f, 0.14f, 0.24f);
            p.addTriangle (0.28f, 0.38f, 0.5f, 0.18f, 0.5f, 0.82f);
            p.addTriangle (0.28f, 0.38f, 0.5f, 0.82f, 0.28f, 0.62f);
            line (p, 0.6f, 0.3f, 0.9f, 0.7f);
            line (p, 0.9f, 0.3f, 0.6f, 0.7f);
            break;

        case Icon::toolZoom: // Lupe
            circleOutline (p, 0.42f, 0.42f, 0.26f);
            line (p, 0.61f, 0.61f, 0.88f, 0.88f);
            break;

        case Icon::toolGlue: // Tube, Tropfen darunter
            rect (p, 0.32f, 0.12f, 0.36f, 0.42f, false);
            line (p, 0.5f, 0.54f, 0.5f, 0.66f);
            p.addEllipse (0.42f, 0.72f, 0.16f, 0.16f);
            break;

        case Icon::nudgeLeft: // senkrechte Linie, Pfeil nach links davor
            line (p, 0.75f, 0.2f, 0.75f, 0.8f);
            line (p, 0.3f, 0.5f, 0.62f, 0.5f);
            arrowHead (p, 0.2f, 0.5f, -1.0f, 0.0f);
            break;

        case Icon::nudgeRight:
            line (p, 0.25f, 0.2f, 0.25f, 0.8f);
            line (p, 0.38f, 0.5f, 0.7f, 0.5f);
            arrowHead (p, 0.8f, 0.5f, 1.0f, 0.0f);
            break;

        case Icon::normalize: // obere und untere Grenzlinie, Spitzen reichen genau heran
            line (p, 0.1f, 0.15f, 0.9f, 0.15f);
            line (p, 0.1f, 0.85f, 0.9f, 0.85f);
            line (p, 0.25f, 0.5f, 0.25f, 0.5f);
            line (p, 0.35f, 0.3f, 0.35f, 0.7f);
            line (p, 0.5f, 0.15f, 0.5f, 0.85f);
            line (p, 0.65f, 0.3f, 0.65f, 0.7f);
            line (p, 0.75f, 0.42f, 0.75f, 0.58f);
            break;

        case Icon::reverse: // Bogen von rechts oben nach links, Pfeilspitze links
        {
            juce::Path arc;
            arc.addCentredArc (0.5f, 0.55f, 0.3f, 0.3f, 0.0f, 1.2f, -1.2f, true);
            juce::PathStrokeType (stroke).createStrokedPath (arc, arc);
            p.addPath (arc);
            arrowHead (p, 0.2f, 0.42f, -0.4f, 0.6f);
            break;
        }

        case Icon::phase: // Kreis mit Schrägstrich (Phasenzeichen)
            circleOutline (p, 0.5f, 0.5f, 0.3f);
            line (p, 0.18f, 0.82f, 0.82f, 0.18f);
            break;

        case Icon::toolRange: // Bereich: zwei senkrechte Grenzen, dazwischen ein flaches Rechteck
            line (p, 0.15f, 0.15f, 0.15f, 0.85f);
            line (p, 0.85f, 0.15f, 0.85f, 0.85f);
            p.addRectangle (0.15f, 0.38f, 0.7f, 0.24f);
            break;

        case Icon::stretch: // zwei Begrenzungen, Doppelpfeil dazwischen
            line (p, 0.1f, 0.2f, 0.1f, 0.8f);
            line (p, 0.9f, 0.2f, 0.9f, 0.8f);
            line (p, 0.3f, 0.5f, 0.7f, 0.5f);
            arrowHead (p, 0.22f, 0.5f, -1.0f, 0.0f);
            arrowHead (p, 0.78f, 0.5f, 1.0f, 0.0f);
            break;

        case Icon::silence: // Welle, in der Mitte flach
            line (p, 0.08f, 0.5f, 0.92f, 0.5f);
            line (p, 0.15f, 0.3f, 0.15f, 0.7f);
            line (p, 0.25f, 0.2f, 0.25f, 0.8f);
            line (p, 0.75f, 0.2f, 0.75f, 0.8f);
            line (p, 0.85f, 0.3f, 0.85f, 0.7f);
            break;

        case Icon::gain: // drei Balken, Pfeil nach oben
            line (p, 0.2f, 0.6f, 0.2f, 0.85f);
            line (p, 0.4f, 0.45f, 0.4f, 0.85f);
            line (p, 0.6f, 0.3f, 0.6f, 0.85f);
            line (p, 0.82f, 0.75f, 0.82f, 0.25f);
            arrowHead (p, 0.82f, 0.12f, 0.0f, -1.0f);
            break;

        case Icon::fadeIn:
            p.addTriangle (0.1f, 0.85f, 0.9f, 0.85f, 0.9f, 0.15f);
            break;

        case Icon::fadeOut:
            p.addTriangle (0.1f, 0.15f, 0.1f, 0.85f, 0.9f, 0.85f);
            break;

        case Icon::zoomFit: // Rahmen, Pfeile nach links und rechts außen
            rect (p, 0.3f, 0.3f, 0.4f, 0.4f, false);
            arrowHead (p, 0.08f, 0.5f, -1.0f, 0.0f);
            arrowHead (p, 0.92f, 0.5f, 1.0f, 0.0f);
            break;

        case Icon::connections: // Buchse links oben, Buchse rechts unten, Kabel dazwischen
            rect (p, 0.08f, 0.15f, 0.24f, 0.24f, false);
            rect (p, 0.68f, 0.61f, 0.24f, 0.24f, false);
            line (p, 0.32f, 0.27f, 0.5f, 0.27f);
            line (p, 0.5f, 0.27f, 0.5f, 0.73f);
            line (p, 0.5f, 0.73f, 0.68f, 0.73f);
            break;

        case Icon::history: // drei Zeilen, links ein Pfeil zurück
            line (p, 0.42f, 0.25f, 0.9f, 0.25f);
            line (p, 0.42f, 0.5f, 0.9f, 0.5f);
            line (p, 0.42f, 0.75f, 0.9f, 0.75f);
            line (p, 0.28f, 0.75f, 0.28f, 0.3f);
            arrowHead (p, 0.28f, 0.16f, 0.0f, -1.0f);
            break;

        case Icon::overview: // breiter Rahmen, darin das sichtbare Fenster
            rect (p, 0.06f, 0.3f, 0.88f, 0.4f, false);
            rect (p, 0.38f, 0.36f, 0.22f, 0.28f, true);
            break;

        case Icon::functions: // Zauberstab mit drei Funken
            line (p, 0.15f, 0.85f, 0.62f, 0.38f);
            line (p, 0.78f, 0.1f, 0.78f, 0.3f);
            line (p, 0.68f, 0.2f, 0.88f, 0.2f);
            line (p, 0.9f, 0.45f, 0.9f, 0.55f);
            line (p, 0.45f, 0.12f, 0.55f, 0.12f);
            break;

        case Icon::retrospective: // Aufnahmepunkt rechts, Pfeil zurück links
            p.addEllipse (0.5f, 0.3f, 0.4f, 0.4f);
            line (p, 0.42f, 0.5f, 0.18f, 0.5f);
            arrowHead (p, 0.08f, 0.5f, -1.0f, 0.0f);
            break;

        case Icon::star: // fünfzackiger Stern
            p.addStar ({ 0.5f, 0.52f }, 5, 0.2f, 0.45f, 0.0f);
            break;

        case Icon::trash: // Papierkorb
            rect (p, 0.25f, 0.3f, 0.5f, 0.6f, false);
            line (p, 0.15f, 0.3f, 0.85f, 0.3f);
            rect (p, 0.4f, 0.12f, 0.2f, 0.12f, true);
            line (p, 0.42f, 0.42f, 0.42f, 0.78f);
            line (p, 0.58f, 0.42f, 0.58f, 0.78f);
            break;

        case Icon::metronome:
            p.addTriangle (0.5f, 0.12f, 0.2f, 0.88f, 0.8f, 0.88f);
            {
                juce::Path inner;
                inner.addTriangle (0.5f, 0.3f, 0.32f, 0.78f, 0.68f, 0.78f);
                p.setUsingNonZeroWinding (false);
                p.addPath (inner);
            }
            line (p, 0.5f, 0.75f, 0.7f, 0.3f);
            break;
    }

    return p;
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    auto path = makeIconPath (icon);
    const auto target = area.reduced (area.getWidth() * 0.15f);
    path.applyTransform (
        juce::AffineTransform::scale (target.getWidth(), target.getHeight()).translated (target.getX(), target.getY()));
    g.setColour (colour);
    g.fillPath (path);
}

std::unique_ptr<juce::Drawable> makeIconDrawable (Icon icon, juce::Colour colour)
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath (makeIconPath (icon));
    drawable->setFill (colour);
    return drawable;
}

} // namespace fk::ui
