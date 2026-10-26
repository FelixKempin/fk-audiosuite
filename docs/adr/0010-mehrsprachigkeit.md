# ADR 0010: Mehrsprachigkeit über JUCE-Übersetzungstabellen, Englisch als Quelltext

Datum: 2026-10-20 · Status: angenommen

## Kontext
DAW und Produkt-Plugins zeigen bisher fest deutschen Text (FEATURES.md, Lokalisierung). Englisch soll Standard werden, Deutsch vollständig bleiben, weitere Sprachen ohne Codeumbau möglich sein. Die Plugins laufen auch in fremden Hosts (Cubase, Reaper), also ohne die DAW; ein Plugin darf die Sprache eines anderen nicht verstellen. Bestehende Projekte, Presets und Automationen dürfen durch die Sprache nicht brechen.

## Entscheidung
1. **JUCE `LocalisedStrings`**, kein eigenes System. Texte stehen im Code auf **Englisch** und laufen durch `TRANS ("...")` (JUCE-Makro für `juce::translate`). Der englische Satz ist der Schlüssel, wie bei gettext auf der Website. Fehlt eine Übersetzung, erscheint der englische Quelltext, nie ein leerer Text.
2. **Tabellen im JUCE-Format** (`"Save" = "Speichern"`), je Ebene eine Datei, eingebettet als Binärdaten (`fk_add_translations`, `cmake/FkTranslations.cmake`): Fundament `src/plugins/lang/de.txt`, DAW `resources/lang/de.txt`, in eigenständigen Plugins eigene Tabellen. Ein Produkt setzt Fundament, common und eigene Tabelle zusammen; eine neue Sprache ist eine Datei je Ebene und ein Eintrag in `i18n::Language`.
3. **Zuordnung je Binärdatei.** `LocalisedStrings::setCurrentMappings` ist statisch je Binärdatei; jedes VST3 bringt seine eigene Kopie von JUCE mit. Innerhalb einer Binärdatei gilt eine Sprache für alle Instanzen.
4. **Eine Sprachwahl für alle FK-Programme** in `~/.config/FKAudioSuite/common.json` (`%APPDATA%` unter Windows), Feld `language`. Ohne Wahl gilt die Systemsprache, wenn sie unterstützt ist, sonst Englisch; kein Einrichtungsdialog. Die DAW setzt die Sprache beim Start, ein Wechsel gilt ab dem nächsten Start. Plugins setzen sie in `createPluginFilter` (vor den Parameternamen) und vor jedem neuen Fenster.
5. **Platzhalter und Plural** über `i18n::format` (`{name}`) und `i18n::plural (n, "{count} track", "{count} tracks")`; Sätze werden nie aus übersetzten Stücken zusammengesetzt.
6. **Nie übersetzt:** Parameter-, Plugin-, Produkt- und Preset-Kennungen, JSON-Schlüssel, Dateiformate, API-Felder, Produktnamen. Gespeichert werden nur Kennungen und Indizes, nie Anzeigetexte.
7. **Prüfung als Test:** `tests/i18n/TranslationAudit` findet alle `TRANS`/`NEEDS_TRANS`/`plural`-Texte und gleicht sie mit der Tabelle ab (fehlend, verwaist, doppelt, Platzhalter). Beide Repos prüfen damit; ein fehlender Eintrag lässt den Testlauf scheitern.

## Folgen
- Die Hilfsfunktion `utf8()` für Umlaute entfällt nach und nach: Englische Literale sind ASCII.
- Englische Texte zu ändern heißt, die deutsche Zeile mitzuziehen; der Test erzwingt es.
- Hosts zeigen Parameternamen in der Sprache, mit der das Plugin geladen wurde, bis es neu geladen wird.
- Tests laufen in Englisch (Prozessoren direkt erzeugt, die DAW-Sprache wird nur in `Application` gesetzt).
- Kommentare und Doku bleiben deutsch (ADR 0006).
