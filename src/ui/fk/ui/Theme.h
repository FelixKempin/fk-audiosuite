#pragma once

#include "fk/plugins/ui/PluginTheme.h"

#include <juce_graphics/juce_graphics.h>

namespace fk::ui::theme
{

// Design-Tokens aus UI_CONCEPT.md §3. Komponenten greifen nur über diese Namen zu.
// Später aus resources/theme/dark.json geladen.
// TODO(felix): Theme aus JSON laden, sobald ein Light Theme dazukommt (P2).

// Farben sind dieselben wie in den Plugin-Oberflächen (plugins/ui/PluginTheme.h), hier nur
// unter dem gewohnten Namen, damit Host und Plugin ein Bild ergeben.
using plugins::ui::theme::accent;
using plugins::ui::theme::accentSoft;
using plugins::ui::theme::backgroundBase;
using plugins::ui::theme::backgroundElevated;
using plugins::ui::theme::backgroundInput;
using plugins::ui::theme::backgroundPanel;
using plugins::ui::theme::borderStrong;
using plugins::ui::theme::borderSubtle;
using plugins::ui::theme::gridBar;
using plugins::ui::theme::gridBeat;
using plugins::ui::theme::meterClip;
using plugins::ui::theme::meterHigh;
using plugins::ui::theme::meterLow;
using plugins::ui::theme::meterMid;
using plugins::ui::theme::mute;
using plugins::ui::theme::record;
using plugins::ui::theme::solo;
using plugins::ui::theme::textDisabled;
using plugins::ui::theme::textPrimary;
using plugins::ui::theme::textSecondary;

// Abstände auf dem 4-px-Raster.
constexpr int spacingSmall = 4;
constexpr int spacingMedium = 8;
constexpr int spacingLarge = 16;

constexpr int trackHeight = 72;
constexpr int automationLaneHeight = 60;
constexpr int trackHeaderWidth = 220;
constexpr int folderRowHeight = 34; ///< Ordnerzeile im Arrangement
constexpr int folderIndent = 14;
constexpr double minGridSpacingPixels =
    5.0; ///< feinere Rasterlinien werden ausgedünnt (kein Strichcode)    ///< Einrückung je Ordnerebene im Spurkopf
constexpr int rulerHeight = 24;
constexpr int transportBarHeight = 48;
constexpr int menuBarHeight = 24;
constexpr int toolbarHeight = 40;
constexpr int channelStripWidth = 104;
constexpr int inspectorWidth = 240;
constexpr int mixerHeight = 360;

/// Spurfarbe nach Index: 16 gesättigte, nicht grelle Farben, rotierend (UI_CONCEPT.md §3).
[[nodiscard]] juce::Colour trackColourForIndex (int index);

} // namespace fk::ui::theme
