#pragma once

#include "fk/services/SignatureService.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Hintergrund des Notenrasters: Zeilen je Tonhöhe (skalenfremde und schwarze Tasten dunkler,
/// Oktavgrenzen als Linie), senkrechte Rasterlinien (Raster, Schlag, Takt) und das Clip-Ende.
/// Liegt als Kind hinter den Noten und reagiert nicht auf die Maus.
class NoteGridBackground final : public juce::Component
{
public:
    /// Was die Linien bestimmt; ändert sich nur bei Zoom, Raster, Taktart oder Clip-Länge.
    struct Layout
    {
        int rowHeight = 12;
        int numNotes = 128;
        double gridBeats = 0.25;
        double pixelsPerBeat = 64.0;
        double beatsPerBar = 4.0;
        double clipLengthBeats = 0.0; ///< 0 = kein Clip: nur Zeilen
    };

    NoteGridBackground();

    void setLayout (Layout newLayout);
    void setKey (services::KeyPointInfo newKey);

    /// Schwarze Taste (Cis, Dis, Fis, Gis, Ais): dunklere Zeile, schwarze Klaviertaste.
    [[nodiscard]] static bool isBlackKey (int noteNumber);

    void paint (juce::Graphics& g) override;

private:
    void paintRows (juce::Graphics& g) const;
    void paintGridLines (juce::Graphics& g) const;

    Layout layout;
    services::KeyPointInfo key;
};

} // namespace fk::ui
