#pragma once

#include <juce_core/juce_core.h>

#include <initializer_list>

namespace fk::plugins::i18n
{

/// Ein benannter Platzhalter in einem übersetzten Text: `{name}`.
struct Placeholder
{
    const char* name;
    juce::String value;
};

/// Setzt Werte in einen (bereits übersetzten) Text ein:
/// `format (TRANS ("Track {name} deleted"), { { "name", track } })`. Die Übersetzung darf die
/// Platzhalter umstellen; Sätze werden nie aus Bruchstücken zusammengesetzt (CODING_GUIDELINES §15).
[[nodiscard]] juce::String format (const juce::String& text, std::initializer_list<Placeholder> values);

/// Wählt Einzahl oder Mehrzahl, übersetzt und setzt `{count}` ein:
/// `plural (n, "{count} track", "{count} tracks")`. Beide Formen stehen in der Tabelle. Sprachen mit
/// mehr Formen bekommen hier eine Regel je Sprache, die Aufrufer bleiben gleich.
[[nodiscard]] juce::String plural (int count, const char* one, const char* other);

} // namespace fk::plugins::i18n
