#include "fk/ui/NoteGridBackground.h"

#include "fk/ui/Theme.h"

#include <cmath>

namespace fk::ui
{

namespace
{
constexpr int minGridLineSpacing = 4; // dichtere Rasterlinien werden ausgedünnt
} // namespace

NoteGridBackground::NoteGridBackground()
{
    setInterceptsMouseClicks (false, false);
}

void NoteGridBackground::setLayout (Layout newLayout)
{
    layout = newLayout;
    repaint();
}

void NoteGridBackground::setKey (services::KeyPointInfo newKey)
{
    key = newKey;
    repaint();
}

bool NoteGridBackground::isBlackKey (int noteNumber)
{
    switch (noteNumber % 12)
    {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
            return true;
        default:
            return false;
    }
}

void NoteGridBackground::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundInput);
    paintRows (g);

    if (layout.clipLengthBeats <= 0.0)
        return;

    paintGridLines (g);

    g.setColour (theme::borderStrong);
    g.drawVerticalLine (static_cast<int> (std::round (layout.clipLengthBeats * layout.pixelsPerBeat)),
                        0.0f,
                        static_cast<float> (getHeight()));
}

void NoteGridBackground::paintRows (juce::Graphics& g) const
{
    // Skalenfremde Töne dunkler, schwarze Tasten etwas dunkler: So sieht man die Tonart im Raster.
    // Nur die Zeilen im neu zu zeichnenden Bereich (die Fläche ist 1536 Pixel hoch).
    const auto clip = g.getClipBounds();
    const int first = juce::jmax (0, layout.numNotes - 1 - clip.getBottom() / layout.rowHeight);
    const int last = juce::jmin (layout.numNotes - 1, layout.numNotes - 1 - clip.getY() / layout.rowHeight);

    for (int note = first; note <= last; ++note)
    {
        const int y = (layout.numNotes - 1 - note) * layout.rowHeight;

        if (! services::SignatureService::isInScale (note, key.rootNote, key.scale))
        {
            g.setColour (theme::backgroundBase.darker (0.35f));
            g.fillRect (clip.getX(), y, clip.getWidth(), layout.rowHeight);
        }
        else if (isBlackKey (note))
        {
            g.setColour (theme::backgroundBase);
            g.fillRect (clip.getX(), y, clip.getWidth(), layout.rowHeight);
        }

        if (note % 12 == 0)
        {
            g.setColour (theme::gridBar);
            g.drawHorizontalLine (
                y + layout.rowHeight - 1, static_cast<float> (clip.getX()), static_cast<float> (clip.getRight()));
        }
    }
}

void NoteGridBackground::paintGridLines (juce::Graphics& g) const
{
    // Raster fein, Schläge mittel, Takte kräftig. Bei feinem Raster und kleinem Zoom nur jede n-te
    // Linie, damit das Raster nicht zur Fläche wird; nur Linien im neu zu zeichnenden Bereich.
    const auto clip = g.getClipBounds();
    const auto stride =
        juce::jmax (1, static_cast<int> (std::ceil (minGridLineSpacing / (layout.gridBeats * layout.pixelsPerBeat))));
    const auto numLines = static_cast<int> (std::floor (layout.clipLengthBeats / layout.gridBeats + 1.0e-6));
    const auto firstVisible = static_cast<int> (clip.getX() / (layout.gridBeats * layout.pixelsPerBeat));

    for (int line = juce::jmax (0, firstVisible - firstVisible % stride); line <= numLines; line += stride)
    {
        const double beat = line * layout.gridBeats;
        const int x = static_cast<int> (std::round (beat * layout.pixelsPerBeat));

        if (x > clip.getRight())
            break;

        const bool isBar = std::abs (std::remainder (beat, layout.beatsPerBar)) < 1.0e-6;
        const bool isBeat = std::abs (std::remainder (beat, 1.0)) < 1.0e-6;
        g.setColour (isBar ? theme::gridBar.brighter (0.4f) : isBeat ? theme::gridBar : theme::gridBeat);
        g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));
    }
}

} // namespace fk::ui
