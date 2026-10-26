#pragma once

#include <juce_graphics/juce_graphics.h>

namespace fk::plugins::ui::theme
{

// Design-Tokens aus UI_CONCEPT.md §3, einzige Stelle mit Farbwerten für Plugin-Oberflächen.
// Die DAW (ui/Theme.h) übernimmt dieselben Werte, damit Plugin und Host ein Bild ergeben.
// Komponenten greifen nur über diese Namen zu, nie über eigene Farbwerte.

inline const juce::Colour backgroundBase { 0xff1a1b1e };
inline const juce::Colour backgroundPanel { 0xff222327 };
inline const juce::Colour backgroundElevated { 0xff2a2b30 };
inline const juce::Colour backgroundInput { 0xff151618 };

inline const juce::Colour borderSubtle { 0xff2e2f34 };
inline const juce::Colour borderStrong { 0xff3c3d43 };

inline const juce::Colour textPrimary { 0xffe8e8ea };
inline const juce::Colour textSecondary { 0xff9a9ba3 };
inline const juce::Colour textDisabled { 0xff5c5d64 };

inline const juce::Colour accent { 0xff4f8cff };
inline const juce::Colour accentSoft { 0x334f8cff };
inline const juce::Colour record { 0xffe5484d };
inline const juce::Colour solo { 0xfff5a623 };
inline const juce::Colour mute { 0xff8e8e93 };

inline const juce::Colour meterLow { 0xff30c56a };
inline const juce::Colour meterMid { 0xffe0c341 };
inline const juce::Colour meterHigh { 0xfff08a24 };
inline const juce::Colour meterClip { 0xffe5484d };

inline const juce::Colour gridBar { 0xff3a3b41 };
inline const juce::Colour gridBeat { 0xff2c2d32 };

// Abstände auf dem 4-px-Raster.
constexpr int spacingSmall = 4;
constexpr int spacingMedium = 8;
constexpr int spacingLarge = 16;

// Maße der Bausteine. Alle Plugins teilen sie, damit Reglerreihen bündig sind.
constexpr int knobWidth = 72;
constexpr int knobDiameter = 52;
constexpr int controlLabelHeight = 16;
constexpr int controlValueHeight = 18;
constexpr int controlHeight = controlLabelHeight + knobDiameter + spacingSmall * 2 + controlValueHeight;
constexpr int choiceWidth = 104;
constexpr int switchWidth = 64;
constexpr int meterWidth = 36;
constexpr int panelTitleHeight = 22;
constexpr int editorHeaderHeight = 32;

constexpr float cornerRadius = 4.0f;

} // namespace fk::plugins::ui::theme
