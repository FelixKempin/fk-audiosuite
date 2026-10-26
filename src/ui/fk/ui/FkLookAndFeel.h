#pragma once

#include "fk/plugins/ui/PluginLookAndFeel.h"

namespace fk::ui
{

/// Stil der DAW: derselbe wie in den Plugin-Oberflächen (plugins::ui::PluginLookAndFeel), damit
/// Host und Plugin ein Bild ergeben. Hier kommen nur DAW-eigene Ergänzungen dazu.
class FkLookAndFeel final : public plugins::ui::PluginLookAndFeel
{
public:
    FkLookAndFeel() = default;
};

} // namespace fk::ui
