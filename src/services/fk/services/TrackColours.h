#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace fk::services
{

/// Die Spurfarben-Palette (16 Farben, gut unterscheidbar auf dunklem Grund). Liegt in den Services,
/// weil neue Spuren beim Anlegen eine Farbe bekommen; die Oberfläche bietet dieselbe zur Auswahl an.
inline const std::array<juce::Colour, 16>& trackPalette()
{
    static const std::array<juce::Colour, 16> palette {
        juce::Colour (0xff4f8cff), juce::Colour (0xff30c56a), juce::Colour (0xfff5a623), juce::Colour (0xffe5484d),
        juce::Colour (0xffa06cff), juce::Colour (0xff2bc4c4), juce::Colour (0xffe06fc0), juce::Colour (0xffc0d040),
        juce::Colour (0xff6fa8ff), juce::Colour (0xff5fd68a), juce::Colour (0xfff7bf5a), juce::Colour (0xffef7076),
        juce::Colour (0xffb98cff), juce::Colour (0xff5ad3d3), juce::Colour (0xffe994d0), juce::Colour (0xffd0dc70)
    };
    return palette;
}

/// Farbe Nummer index, zyklisch.
inline juce::Colour paletteColour (int index)
{
    const auto& palette = trackPalette();
    return palette[static_cast<std::size_t> (juce::jmax (0, index)) % palette.size()];
}

} // namespace fk::services
