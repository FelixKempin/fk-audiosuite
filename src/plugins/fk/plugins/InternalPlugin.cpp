#include "fk/plugins/InternalPlugin.h"

#include "fk/plugins/ui/PanelEditor.h"

namespace fk::plugins
{

InternalPlugin::InternalPlugin (Info info, juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout)
    : juce::AudioPluginInstance (busesFor (info))
    , parameters (*this, nullptr, "PARAMETERS", std::move (parameterLayout))
    , pluginInfo (std::move (info))
{
}

juce::AudioProcessor::BusesProperties InternalPlugin::busesFor (const Info& info)
{
    BusesProperties buses;

    if (! info.isInstrument)
        buses = buses.withInput ("Input", juce::AudioChannelSet::stereo(), true);

    if (info.hasSidechain)
        buses = buses.withInput ("Sidechain", juce::AudioChannelSet::stereo(), true);

    buses = buses.withOutput ("Output", juce::AudioChannelSet::stereo(), true);

    // Weitere Stereo-Ausgänge "Out 3+4", "Out 5+6" ... für Multi-Out im Rack.
    for (int pair = 1; pair < info.outputPairs; ++pair)
        buses = buses.withOutput ("Out " + juce::String (pair * 2 + 1) + "+" + juce::String (pair * 2 + 2),
                                  juce::AudioChannelSet::stereo(),
                                  true);

    return buses;
}

juce::PluginDescription InternalPlugin::makeDescription (const Info& info)
{
    juce::PluginDescription description;
    description.name = info.name;
    description.descriptiveName = info.name;
    description.pluginFormatName = internalFormatName;
    description.category = info.category;
    description.manufacturerName = "FK Audio Suite";
    description.version = "1.0";
    description.fileOrIdentifier = "fk:" + juce::String (info.uniqueId);
    description.uniqueId = info.uniqueId;
    description.deprecatedUid = info.uniqueId;
    description.isInstrument = info.isInstrument;
    description.numInputChannels = info.isInstrument ? 0 : 2;
    description.numOutputChannels = 2;
    return description;
}

void InternalPlugin::fillInPluginDescription (juce::PluginDescription& description) const
{
    description = makeDescription (pluginInfo);
}

const juce::String InternalPlugin::getName() const
{
    return pluginInfo.name;
}

bool InternalPlugin::acceptsMidi() const
{
    return pluginInfo.isInstrument || pluginInfo.isMidiEffect;
}

bool InternalPlugin::producesMidi() const
{
    return pluginInfo.isMidiEffect;
}

double InternalPlugin::getTailLengthSeconds() const
{
    return 0.0;
}

bool InternalPlugin::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* InternalPlugin::createEditor()
{
    // JUCE verlangt hier einen rohen, besessenen Zeiger (Framework-Vertrag, keine Stilwahl).
    return new ui::PanelEditor (*this, parameters, describePanel());
}

ui::PanelDescription InternalPlugin::describePanel()
{
    // Ohne eigene Beschreibung: jeder Parameter einmal, in Reihen zu sechs, in Layout-Reihenfolge.
    constexpr int controlsPerRow = 6;
    ui::PanelDescription description { .title = pluginInfo.name };

    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            if (description.sections.empty() ||
                static_cast<int> (description.sections.back().controls.size()) >= controlsPerRow)
                description.sections.push_back ({ .title = TRANS ("Parameters") });

            description.sections.back().controls.push_back (ui::knob (ranged->getParameterID()));
        }
    }

    return description;
}

juce::AudioProcessorValueTreeState& InternalPlugin::getParameterState() noexcept
{
    return parameters;
}

int InternalPlugin::getNumPrograms()
{
    return 1;
}

int InternalPlugin::getCurrentProgram()
{
    return 0;
}

void InternalPlugin::setCurrentProgram (int) {}

const juce::String InternalPlugin::getProgramName (int)
{
    return {};
}

void InternalPlugin::changeProgramName (int, const juce::String&) {}

void InternalPlugin::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void InternalPlugin::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

bool InternalPlugin::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Instrumente haben keinen Eingang; Effekte Stereo, dazu optional eine Stereo-Sidechain (oder abgeschaltet).
    if (! pluginInfo.isInstrument && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (int bus = 1; bus < layouts.inputBuses.size(); ++bus)
        if (! layouts.inputBuses[bus].isDisabled() && layouts.inputBuses[bus] != juce::AudioChannelSet::stereo())
            return false;

    // Zusatzausgänge: Stereo oder abgeschaltet (auf normalen Spuren läuft nur der Hauptausgang).
    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
        if (! layouts.outputBuses[bus].isDisabled() && layouts.outputBuses[bus] != juce::AudioChannelSet::stereo())
            return false;

    return true;
}

std::optional<juce::AudioBuffer<float>> InternalPlugin::sidechainOf (juce::AudioBuffer<float>& buffer)
{
    if (! pluginInfo.hasSidechain || getBusCount (true) < 2)
        return std::nullopt;

    auto* bus = getBus (true, 1);

    if (bus == nullptr || ! bus->isEnabled() || bus->getNumberOfChannels() < 2)
        return std::nullopt;

    auto sidechain = getBusBuffer (buffer, true, 1);

    if (sidechain.getNumChannels() < 2)
        return std::nullopt;

    return sidechain;
}

std::atomic<float>& InternalPlugin::parameter (const juce::String& parameterId)
{
    auto* value = parameters.getRawParameterValue (parameterId);
    jassert (value != nullptr); // Tippfehler in der Parameter-ID: ein Programmierfehler
    return *value;
}

} // namespace fk::plugins
