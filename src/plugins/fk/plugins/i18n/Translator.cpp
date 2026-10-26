#include "fk/plugins/i18n/Translator.h"

namespace fk::plugins::i18n
{

std::unique_ptr<juce::LocalisedStrings> Translator::build (Language language,
                                                            std::span<const TranslationTable> tables)
{
    if (language == Language::english)
        return nullptr;

    // Ein Text statt LocalisedStrings::addStrings: addStrings verlangt gleiche Kopfzeilen
    // (language:, countries:) in jeder Tabelle; zusammengehängt gewinnt einfach die letzte Zeile.
    juce::String merged;
    for (const auto& table : tables)
        if (table.language == language)
            merged << juce::String::fromUTF8 (table.text.data(), static_cast<int> (table.text.size())) << "\n";

    if (merged.isEmpty())
        return nullptr;

    return std::make_unique<juce::LocalisedStrings> (merged, false);
}

void Translator::apply (Language language, std::span<const TranslationTable> tables)
{
    // JUCE übernimmt den Besitz (Framework-Vertrag von setCurrentMappings); nullptr = Quelltext.
    juce::LocalisedStrings::setCurrentMappings (build (language, tables).release());
}

} // namespace fk::plugins::i18n
