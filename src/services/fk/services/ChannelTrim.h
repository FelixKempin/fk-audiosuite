#pragma once

#include "fk/plugins/InternalPlugin.h"
#include "fk/plugins/TrimProcessor.h"

#include <tracktion_engine/tracktion_engine.h>

namespace fk::services::channelTrim
{

namespace te = tracktion;

/// Das unsichtbare Trim-Plugin des Kanalzugs? Es gehört zum Mixer, nicht zu den Inserts.
[[nodiscard]] inline bool isChannelTrim (const juce::PluginDescription& description)
{
    return description.pluginFormatName == plugins::internalFormatName &&
           description.uniqueId == plugins::TrimProcessor::info().uniqueId;
}

[[nodiscard]] inline bool isChannelTrim (const te::Plugin& plugin)
{
    const auto* external = dynamic_cast<const te::ExternalPlugin*> (&plugin);
    return external != nullptr && isChannelTrim (external->desc);
}

} // namespace fk::services::channelTrim
