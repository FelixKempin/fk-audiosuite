#pragma once

#include <juce_core/juce_core.h>

namespace fk::services
{

/// Wie die zweite Positionsanzeige die Zeit darstellt (die erste zeigt immer Takte).
enum class TimeDisplay
{
    seconds,  ///< 1:23.456 (Minuten:Sekunden.Millisekunden)
    timecode, ///< 00:01:23:11 (Stunden:Minuten:Sekunden:Frames)
    samples   ///< 3 937 536
};

/// Formatiert eine Position. sampleRate nur für Samples, framesPerSecond nur für Timecode.
[[nodiscard]] juce::String formatTime (double seconds, TimeDisplay display, double sampleRate, int framesPerSecond);

/// Anzeigename, z. B. für Menüs.
[[nodiscard]] juce::String getTimeDisplayName (TimeDisplay display);

} // namespace fk::services
