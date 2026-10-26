#pragma once

namespace fk::ui
{

/// Abbildung Zeit ↔ Pixel für Lineal, Spuren und Positionszeiger. Ein kleiner Werttyp,
/// den alle Zeitachsen-Komponenten teilen, damit sie garantiert dieselbe Skala zeichnen.
/// → LEARNING.md §1 (Werttypen)
struct TimeMapping
{
    double pixelsPerSecond = 40.0;
    double firstVisibleSecond = 0.0;

    [[nodiscard]] int secondsToX (double seconds) const noexcept
    {
        return static_cast<int> ((seconds - firstVisibleSecond) * pixelsPerSecond);
    }

    [[nodiscard]] double xToSeconds (int x) const noexcept
    {
        return firstVisibleSecond + static_cast<double> (x) / pixelsPerSecond;
    }
};

} // namespace fk::ui
