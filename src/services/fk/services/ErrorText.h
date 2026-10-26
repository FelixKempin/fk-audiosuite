#pragma once

#include "fk/core/Error.h"

#include <juce_core/juce_core.h>

namespace fk::services
{

/// Wandelt eine Fehlermeldung (UTF-8 in std::string) sicher in einen juce::String.
/// juce::String (const char*) nimmt nur ASCII und warnt sonst; Umlaute brauchen fromUTF8.
[[nodiscard]] inline juce::String errorText (const core::Error& error)
{
    return juce::String::fromUTF8 (error.message.c_str(), static_cast<int> (error.message.size()));
}

} // namespace fk::services
