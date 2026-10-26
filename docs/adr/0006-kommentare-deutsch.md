# ADR 0006: Kommentare auf Deutsch, Bezeichner auf Englisch

Datum: 2026-10-08 · Status: angenommen

## Kontext
Das Projekt ist ein Lernprojekt eines deutschsprachigen Entwicklers und soll später Open Source mit internationaler Zielgruppe werden. Gemischte Sprachen im Code gelten als unsauber; verständliche Erklärungen in der Muttersprache beschleunigen das Lernen deutlich.

## Entscheidung
- Bezeichner (Klassen, Funktionen, Variablen, Dateien) ausschließlich Englisch.
- Kommentare, Doxygen-Beschreibungen, Commit-Nachrichten und Dokumentation auf Deutsch.
- Lernstoff, der über das Verständnis der konkreten Codestelle hinausgeht, steht in `LEARNING.md`, nicht im Code.
- Vor dem ersten öffentlichen Release wird entschieden, ob Doxygen-Kommentare der öffentlichen API ins Englische übersetzt werden (geschätzter Aufwand: 2–3 Tage bei 0.5).

## Folgen
- Externe Beiträge sind anfangs auf deutschsprachige Entwickler beschränkt. Akzeptiert.
- Die Doxygen-Kommentare bleiben knapp, damit eine spätere Übersetzung günstig ist.
