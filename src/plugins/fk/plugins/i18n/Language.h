#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>

namespace fk::plugins::i18n
{

/// Oberflächensprache aller FK-Programme (ADR 0010). Englisch ist Standard und Quelltext im Code;
/// jede weitere Sprache ist eine Übersetzungstabelle (lang/<code>.txt). Eine neue Sprache braucht
/// einen Eintrag hier und je eine Tabelle, sonst keinen Code.
enum class Language
{
    english,
    german
};

inline constexpr std::array<Language, 2> allLanguages { Language::english, Language::german };

/// ISO-639-1-Code: "en", "de". So steht die Sprache in common.json und in Website-Adressen.
[[nodiscard]] juce::String toCode (Language language);

/// Deutet "de", "de_DE", "de-AT" oder "DE"; unbekannt ergibt nullopt.
[[nodiscard]] std::optional<Language> fromCode (const juce::String& code);

/// Name der Sprache in ihr selbst ("Deutsch"), für die Sprachwahl; wird nie übersetzt.
[[nodiscard]] juce::String nativeName (Language language);

} // namespace fk::plugins::i18n
