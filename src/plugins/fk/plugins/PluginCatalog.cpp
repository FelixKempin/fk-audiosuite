#include "fk/plugins/PluginCatalog.h"

#include "fk/plugins/CompressorProcessor.h"
#include "fk/plugins/DelayProcessor.h"
#include "fk/plugins/DistortionProcessor.h"
#include "fk/plugins/EqProcessor.h"
#include "fk/plugins/GainProcessor.h"
#include "fk/plugins/GateProcessor.h"
#include "fk/plugins/InternalPlugin.h"
#include "fk/plugins/LimiterProcessor.h"
#include "fk/plugins/MidiEffectProcessors.h"
#include "fk/plugins/ReverbProcessor.h"
#include "fk/plugins/SamplerProcessor.h"
#include "fk/plugins/SubtractiveSynthProcessor.h"
#include "fk/plugins/TrimProcessor.h"

#include <functional>

namespace fk::plugins
{

namespace
{
/// Ein Eintrag: Beschreibung plus Fabrikfunktion. Neue Plugins kommen nur hier dazu.
struct Entry
{
    InternalPlugin::Info info;
    std::function<std::unique_ptr<juce::AudioPluginInstance>()> create;
};

const std::vector<Entry>& entries()
{
    static const std::vector<Entry> table {
        { GainProcessor::info(), [] { return std::make_unique<GainProcessor>(); } },
        { EqProcessor::info(), [] { return std::make_unique<EqProcessor>(); } },
        { CompressorProcessor::info(), [] { return std::make_unique<CompressorProcessor>(); } },
        { LimiterProcessor::info(), [] { return std::make_unique<LimiterProcessor>(); } },
        { DistortionProcessor::info(), [] { return std::make_unique<DistortionProcessor>(); } },
        { GateProcessor::info(), [] { return std::make_unique<GateProcessor>(); } },
        { DelayProcessor::info(), [] { return std::make_unique<DelayProcessor>(); } },
        { ReverbProcessor::info(), [] { return std::make_unique<ReverbProcessor>(); } },
        { TrimProcessor::info(), [] { return std::make_unique<TrimProcessor>(); } }, // nur für den Kanalzug
        { SubtractiveSynthProcessor::info(), [] { return std::make_unique<SubtractiveSynthProcessor>(); } },
        { SamplerProcessor::info(), [] { return std::make_unique<SamplerProcessor>(); } },
        { TransposeProcessor::info(), [] { return std::make_unique<TransposeProcessor>(); } },
        { ChordProcessor::info(), [] { return std::make_unique<ChordProcessor>(); } },
        { ArpeggiatorProcessor::info(), [] { return std::make_unique<ArpeggiatorProcessor>(); } },
    };

    return table;
}
} // namespace

std::vector<juce::PluginDescription> PluginCatalog::getDescriptions()
{
    std::vector<juce::PluginDescription> descriptions;

    for (const auto& entry : entries())
        descriptions.push_back (InternalPlugin::makeDescription (entry.info));

    return descriptions;
}

std::unique_ptr<juce::AudioPluginInstance> PluginCatalog::createInstance (const juce::PluginDescription& description)
{
    if (! isInternal (description))
        return nullptr;

    for (const auto& entry : entries())
        if (entry.info.uniqueId == description.uniqueId)
            return entry.create();

    return nullptr;
}

bool PluginCatalog::isInternal (const juce::PluginDescription& description)
{
    return description.pluginFormatName == internalFormatName;
}

} // namespace fk::plugins
