# ADR 0002: JUCE 8 und Tracktion Engine als Basis

Datum: 2026-10-08 · Status: angenommen

## Kontext
Eine DAW-Engine von Grund auf (Graph, Latenzausgleich, Recording, Rendering, Undo, Plugin-Hosting) ist Jahre Arbeit. Alternativen: Ardour forken (GTK, eigene Konventionen, schwer anpassbar), eigene Engine auf JUCE (realistisch 2+ Jahre bis Parität), Tracktion Engine (fertige GPL-Engine, kommerziell im Produkt Waveform im Einsatz).

## Entscheidung
- **JUCE 8** für GUI, Audio-/MIDI-Geräte, Plugin-Formate, Dateiformate.
- **Tracktion Engine** als DAW-Modell und Audio-Graph. Ihr `te::Edit` ist unser Projektmodell. Wir duplizieren kein Datenmodell.
- **sfizz** als Sampler-Engine für SFZ-Instrumente.
- Beide Bibliotheken als Git-Submodule, auf einen Commit gepinnt; Aktualisierung bewusst und getestet.

## Folgen
- Wir übernehmen Tracktions Begriffe (Edit, Track, Clip, Plugin, TransportControl) und Konventionen.
- Wir sind an Tracktions Entwicklungstempo gebunden; Fehler dort melden oder lokal patchen (Patches in `external/patches/`).
- Lizenz: beide Bibliotheken GPLv3 oder kommerziell → siehe ADR 0003.
- Der UI-Code darf Tracktion nicht direkt verwenden (ADR 0004), damit die Abhängigkeit beherrschbar bleibt.
