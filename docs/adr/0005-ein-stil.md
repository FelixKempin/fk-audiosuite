# ADR 0005: Genau ein Code-Stil, durch Werkzeuge erzwungen

Datum: 2026-10-08 · Status: angenommen

## Kontext
C++ erlaubt für fast alles mehrere Schreibweisen. Der Entwickler lernt die Sprache am Projekt und will von Anfang an einen sauberen, konsistenten Stil, nicht einen Zoo von Ansätzen.

## Entscheidung
- `CODING_GUIDELINES.md` legt für jedes Thema einen Weg fest. Alternativen werden dort nicht aufgeführt.
- Formatierung: JUCE-Stil (Allman, 4 Leerzeichen), erzwungen durch `.clang-format`.
- Namensgebung: JUCE-Stil (camelCase, kein Member-Präfix), geprüft durch `.clang-tidy`.
- Warnungen sind Fehler. Tests mit Catch2.
- Abweichungen erfordern erst eine Änderung der Richtlinie (mit ADR), dann des Codes.

## Folgen
- Unser Code sieht aus wie JUCE/Tracktion-Code, der Entwickler liest beides mit demselben Auge.
- Manche in der C++-Welt beliebten Techniken (Template-Metaprogrammierung, Ausnahmen, `m_`-Präfixe) kommen bewusst nicht vor.
