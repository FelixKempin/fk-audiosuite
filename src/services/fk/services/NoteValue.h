#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace fk::services
{

/// Ein Notenwert wie in Cubase: Bruchteil einer ganzen Note (1/1 bis 1/128), gerade, punktiert
/// oder triolisch. Dient im Key Editor als Raster (Quantisierung, Einrasten) und als Länge
/// neuer Noten. Rechnet in Schlägen, ein Schlag = eine Viertelnote.
struct NoteValue
{
    enum class Feel
    {
        straight, ///< gerade
        dotted,   ///< punktiert: anderthalbfache Länge
        triplet   ///< triolisch: drei in der Zeit von zwei
    };

    int denominator = 16; ///< 1, 2, 4, ..., 128
    Feel feel = Feel::straight;

    /// Länge in Schlägen: 1/4 = 1, 1/16 = 0,25, 1/8 punktiert = 0,75, 1/8 triolisch = 1/3.
    [[nodiscard]] double beats() const noexcept;

    /// Anzeigename: "1/16", "1/8 punktiert", "1/4 triolisch".
    [[nodiscard]] juce::String name() const;

    /// Kurzname für enge Auswahllisten: "1/16", "1/8.", "1/4T".
    [[nodiscard]] juce::String shortName() const;

    /// Alle Werte von 1/1 bis 1/128, je Nenner gerade, punktiert, triolisch.
    [[nodiscard]] static std::vector<NoteValue> all();

    bool operator== (const NoteValue&) const = default;
};

} // namespace fk::services
