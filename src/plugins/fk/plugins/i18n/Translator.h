#pragma once

#include "fk/plugins/i18n/Language.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <span>
#include <string_view>

namespace fk::plugins::i18n
{

/// Eine eingebettete Übersetzungstabelle im JUCE-Format (`"Save" = "Speichern"`), UTF-8.
struct TranslationTable
{
    Language language = Language::german;
    std::string_view text;
};

/// Setzt die Übersetzung für diese Binärdatei. JUCE hält die Zuordnung je Binärdatei: In einem
/// fremden Host hat jedes VST3 seine eigene, ein FK-Plugin beeinflusst also kein anderes. Texte
/// laufen über TRANS (...) bzw. juce::translate; fehlt ein Eintrag, bleibt der englische Quelltext.
///
/// Nur im Message-Thread aufrufen, nie im Audio-Thread (JUCE sperrt dabei kurz einen SpinLock).
class Translator final
{
public:
    /// Führt alle Tabellen der Sprache zusammen (spätere überschreiben frühere) und setzt sie.
    /// Englisch setzt keine Tabelle: Der Quelltext ist die englische Fassung.
    static void apply (Language language, std::span<const TranslationTable> tables);

    /// Dasselbe ohne Setzen, für Tests: nullptr bei Englisch oder ohne passende Tabelle.
    [[nodiscard]] static std::unique_ptr<juce::LocalisedStrings> build (Language language,
                                                                        std::span<const TranslationTable> tables);
};

} // namespace fk::plugins::i18n
