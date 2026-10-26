#pragma once

#include <juce_core/juce_core.h>

namespace fk::services
{

/// Art einer Controller-Lane im Key Editor. CC-Nummern 0..127 sind direkt die Art; Pitch-Bend,
/// Aftertouch (Kanaldruck) und polyphoner Aftertouch bekommen eigene Werte über 127.
namespace controllerTypes
{
constexpr int modWheel = 1;
constexpr int sustain = 64;
constexpr int sostenuto = 66;
constexpr int soft = 67;
constexpr int pitchBend = 0x1005;      ///< Wert 0..16383, Mitte 8192 (wie Tracktion)
constexpr int aftertouch = 0x1007;     ///< Kanaldruck 0..127
constexpr int polyAftertouch = 0x1004; ///< je Note 0..127, Note in metadata
[[nodiscard]] bool isPitchBend (int type) noexcept;
[[nodiscard]] int maxValue (int type) noexcept;
[[nodiscard]] int defaultValue (int type) noexcept;
[[nodiscard]] juce::String name (int type);
} // namespace controllerTypes

} // namespace fk::services
