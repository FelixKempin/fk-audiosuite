#include "fk/plugins/i18n/Language.h"

namespace fk::plugins::i18n
{

juce::String toCode (Language language)
{
    switch (language)
    {
        case Language::english:
            return "en";
        case Language::german:
            return "de";
    }

    return "en";
}

std::optional<Language> fromCode (const juce::String& code)
{
    const auto prefix = code.trim().toLowerCase().substring (0, 2);

    for (const auto language : allLanguages)
        if (toCode (language) == prefix)
            return language;

    return std::nullopt;
}

juce::String nativeName (Language language)
{
    switch (language)
    {
        case Language::english:
            return "English";
        case Language::german:
            return "Deutsch";
    }

    return "English";
}

} // namespace fk::plugins::i18n
