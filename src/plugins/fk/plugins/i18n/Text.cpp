#include "fk/plugins/i18n/Text.h"

namespace fk::plugins::i18n
{

juce::String format (const juce::String& text, std::initializer_list<Placeholder> values)
{
    auto result = text;

    for (const auto& value : values)
        result = result.replace ("{" + juce::String (value.name) + "}", value.value);

    return result;
}

juce::String plural (int count, const char* one, const char* other)
{
    // Englisch und Deutsch: Einzahl genau bei 1.
    const auto text = juce::translate (count == 1 ? one : other);
    return format (text, { { "count", juce::String (count) } });
}

} // namespace fk::plugins::i18n
