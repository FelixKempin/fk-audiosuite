# ADR 0004: Schichtenarchitektur mit Services; UI sieht Tracktion nicht

Datum: 2026-10-08 · Status: angenommen

## Kontext
Ein DAW-UI hat Dutzende Komponenten, die alle am Projektmodell hängen. Wenn jede Komponente direkt Tracktion-Methoden aufruft, ist Logik überall verstreut, nicht testbar, und jede Tracktion-Änderung trifft jede Komponente. Gegenstück: ein vollständiger Wrapper um Tracktion wäre eine Parallelwelt mit doppeltem Aufwand.

## Entscheidung
- Module mit strikter Abhängigkeitsrichtung: `app → ui → services → engine/library/control → plugins → dsp → core` (siehe ARCHITECTURE.md §3).
- **Services** kapseln Anwendungsfälle und sind die einzige Stelle, die Tracktion aufruft.
- **UI** erhält Services per Konstruktor-Injektion und reagiert auf deren Listener. UI darf Tracktion-Typen als undurchsichtige Handles halten, aber keine Tracktion-Methoden aufrufen.
- Kein vollständiger Wrapper: Tracktions Modell ist unser Modell.
- `dsp` ist frei von JUCE-GUI und Tracktion und vollständig getestet.

## Folgen
- Services sind ohne UI testbar; UI ist mit Attrappen-Services entwickelbar.
- Geringer Mehraufwand pro Feature (ein Service-Aufruf statt direktem Tracktion-Zugriff), dafür eine Stelle für Validierung, Undo-Gruppierung und Fehlerbehandlung.
- Die Regel wird per clang-tidy-Include-Check und CMake-Sichtbarkeit erzwungen.
