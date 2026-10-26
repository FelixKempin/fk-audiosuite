#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace fk::tests::i18n
{

/// Eine Zeile `"Quelltext" = "Übersetzung"` einer Tabelle.
struct TableEntry
{
    juce::String key;
    juce::String value;
    int line = 0;
};

struct ParsedTable
{
    juce::String name; ///< für Meldungen, z. B. "src/plugins/lang/de.txt"
    std::vector<TableEntry> entries;
    juce::StringArray problems; ///< doppelte Schlüssel, leere Übersetzungen, unlesbare Zeilen
};

/// Ergebnis eines Abgleichs Quelltexte gegen Tabelle. Sauber heißt: jeder Text ist übersetzt,
/// keine Übersetzung ist verwaist, kein Platzhalter fehlt oder ist dazugekommen.
struct AuditReport
{
    juce::StringArray missing;  ///< im Code, nicht in der Tabelle
    juce::StringArray unused;   ///< in der Tabelle, nicht im Code
    juce::StringArray problems; ///< Tabellenfehler, Platzhalter, Doppelungen über Tabellen

    [[nodiscard]] bool isClean() const;
    [[nodiscard]] juce::String describe() const;
};

/// Prüfwerkzeug der Übersetzungen (ADR 0010), kein Teil der ausgelieferten Programme. Liest das
/// JUCE-Tabellenformat genau wie juce::LocalisedStrings und findet Quelltexte über die Markierungen
/// TRANS ("..."), TRANS (juce::String::fromUTF8 ("...")), NEEDS_TRANS ("...") und
/// plural (n, "...", "..."). Kommentare werden vorher entfernt.
class TranslationAudit final
{
public:
    [[nodiscard]] static ParsedTable parseTable (const juce::String& text, const juce::String& name);
    [[nodiscard]] static ParsedTable loadTable (const juce::File& file);

    [[nodiscard]] static juce::StringArray extractKeys (const juce::String& sourceText);

    /// Alle .h/.cpp unter den Ordnern (rekursiv).
    [[nodiscard]] static juce::StringArray collectSourceKeys (const juce::Array<juce::File>& directories);

    /// Zeichenkettenwerte der genannten Felder in allen .json unter dem Ordner, in jeder Tiefe
    /// (z. B. "description" in Amp-Datensätzen). Diese Texte übersetzt der Code per juce::translate.
    [[nodiscard]] static juce::StringArray collectJsonKeys (const juce::File& directory, const juce::StringArray& fields);

    /// Abgleich: usedKeys gegen die eigene Tabelle; inherited sind Tabellen, die vorher gesetzt werden
    /// (Fundament), deren Einträge zählen als vorhanden und dürfen in own nicht noch einmal stehen.
    [[nodiscard]] static AuditReport audit (const juce::StringArray& usedKeys,
                                            const ParsedTable& own,
                                            const std::vector<ParsedTable>& inherited = {});

    /// Platzhalter `{name}` eines Textes, sortiert.
    [[nodiscard]] static juce::StringArray placeholdersOf (const juce::String& text);

    [[nodiscard]] static juce::String stripComments (const juce::String& sourceText);
};

} // namespace fk::tests::i18n
