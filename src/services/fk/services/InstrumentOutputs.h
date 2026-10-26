#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace fk::services::instrumentOutputs
{

namespace te = tracktion;

/// Auf einer normalen Instrumentenspur spielt nur der Hauptausgang, wie in Cubase: Zusatzausgänge
/// (Multi-Out) sind still, bis man das Instrument als Rack-Instrument anlegt und sie dort aktiviert.
/// Tracktion schaltet beim Erzeugen alle Ausgänge eines Plugins ein; das nehmen wir hier zurück.
/// Die Einstellung wird mit dem Projekt gespeichert.
inline void useMainOutputOnly (te::Plugin& plugin)
{
    auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin);

    if (external == nullptr)
        return;

    auto* instance = external->getAudioPluginInstance();

    if (instance == nullptr || instance->getBusCount (false) < 2)
        return;

    auto layout = instance->getBusesLayout();

    for (int bus = 1; bus < layout.outputBuses.size(); ++bus)
        layout.outputBuses.getReference (bus) = juce::AudioChannelSet::disabled();

    external->setBusesLayout (layout);
}

} // namespace fk::services::instrumentOutputs
