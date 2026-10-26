#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace fk::services::midiChannelRouting
{

namespace te = tracktion;

/// Die Patch Bay einer Spur, die alles auf den Kanal der Spur legt (Rack-MIDI-Spuren und MIDI-Spuren
/// an externen Geräten); nullptr, wenn die Spur keine hat.
inline te::MidiPatchBayPlugin* findPatchBay (te::AudioTrack& track)
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (auto* patchBay = dynamic_cast<te::MidiPatchBayPlugin*> (plugin))
            return patchBay;

    return nullptr;
}

/// Alles, was ankommt (Clips und Live-Spiel auf jedem Kanal), auf genau einen Kanal legen.
inline void routeAllTo (te::MidiPatchBayPlugin& patchBay, int midiChannel)
{
    te::MidiPatchBayPlugin::Mappings mappings;
    mappings.map[0] = 0;

    for (int source = 1; source <= 16; ++source)
        mappings.map[source] = static_cast<char> (midiChannel);

    patchBay.setMappings (mappings);
}

/// Setzt eine Patch Bay an den Anfang der Spur und legt alles auf midiChannel.
inline void insertPatchBay (te::Edit& edit, te::AudioTrack& track, int midiChannel)
{
    auto plugin = edit.getPluginCache().createNewPlugin (te::MidiPatchBayPlugin::create());
    track.pluginList.insertPlugin (plugin, 0, nullptr);

    if (auto* patchBay = dynamic_cast<te::MidiPatchBayPlugin*> (plugin.get()))
        routeAllTo (*patchBay, midiChannel);
}

} // namespace fk::services::midiChannelRouting
