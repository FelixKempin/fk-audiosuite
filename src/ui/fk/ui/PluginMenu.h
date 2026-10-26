#pragma once

#include "fk/services/PluginService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace fk::ui
{

/// Menü zur Plugin-Auswahl, gleich in Inspector, MixConsole und beim Instrument: Favoriten oben, dann
/// je Kategorie ein Untermenü, zuletzt alle nach Hersteller. Ausgeblendete Plugins liefert der Service
/// gar nicht erst. Menü-ID = Index in plugins + firstId.
[[nodiscard]] juce::PopupMenu buildPluginMenu (const std::vector<services::PluginInfo>& plugins, int firstId = 1);

} // namespace fk::ui
