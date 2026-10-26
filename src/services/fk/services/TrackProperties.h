#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace fk::services::trackProperties
{

// Eigene Eigenschaften im Spur-Zustand von Tracktion. Tracktion ignoriert unbekannte Properties,
// speichert sie aber mit und führt sie durch Undo. Einmal definiert, überall dieselben Namen.
inline const juce::Identifier channels ("fkChannels"); ///< 1 = Mono, 2 = Stereo
inline const juce::Identifier group ("fkGroup");       ///< true = Gruppenspur
inline const juce::Identifier fx ("fkFx");             ///< true = FX-Spur (Aux-Return)
inline const juce::Identifier fxBus ("fkFxBus");       ///< Bus-Nummer der FX-Spur (0, 1, 2 ...)
inline const juce::Identifier bake ("fkBake");         ///< Art des Bakes: "track", "inserts", "group" (ADR 0009)
inline const juce::Identifier rack ("fkRack");               ///< Rack-Instrument, zu dem die Spur gehört (Rack-ID)
inline const juce::Identifier rackRole ("fkRackRole");       ///< "midi" (spielt das Rack) oder "output" (ein Ausgang)
inline const juce::Identifier rackChannel ("fkRackChannel"); ///< MIDI-Kanal 1..16 einer MIDI-Spur am Rack
inline const juce::Identifier rackOutput ("fkRackOutput");   ///< Stereo-Ausgang 0, 1, 2 ... eines Ausgangskanals
inline const juce::Identifier externalMidi ("fkExternalMidi"); ///< true = MIDI-Spur an einem externen Gerät
inline const juce::Identifier midiProgram ("fkMidiProgram");   ///< Programm 0..127 (-1 = keins), externe MIDI-Spur
inline const juce::Identifier midiBank ("fkMidiBank");         ///< Bank 0..16383 (-1 = keine), externe MIDI-Spur
inline const juce::Identifier drumMap ("fkDrumMap");           ///< Drum-Map der Spur; leer = automatisch, "-" = keine
inline const juce::Identifier samplerRegions ("fkSamplerRegions"); ///< Sampler-Spur: Samples und Noten als JSON
inline const juce::Identifier
    automationLanes ("fkAutomationLanes"); ///< Parameter-IDs der Automationsspuren, "|"-getrennt, "-" = keiner

} // namespace fk::services::trackProperties
