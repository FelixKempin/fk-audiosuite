# ADR 0003: GPLv3 jetzt, kommerzielle Option offen halten

Datum: 2026-10-08 · Status: angenommen

## Kontext
Ziel: Open Source, später Verkauf (z. B. 10 USD pro Lizenz). JUCE, Tracktion Engine, Surge, Vital u. a. sind GPLv3 oder kommerziell lizenziert. ASIO-SDK und VST3-SDK (proprietäre Variante) sind nicht GPL-kompatibel verteilbar. Verkauf von GPL-Software ist erlaubt (Modell Ardour: Quellcode frei, Binaries kostenpflichtig).

## Entscheidung
1. Das Projekt steht unter **GPLv3**. Damit sind JUCE und Tracktion Engine kostenlos nutzbar.
2. **Alle eigenen Beiträge bleiben beim Projektinhaber** (kein CLA-Problem, solange nur eine Person beiträgt; bei externen Beiträgen wird ein CLA eingeführt, der eine spätere Doppellizenzierung erlaubt).
3. Fremde GPL-Plugins werden **nur als separate Binaries** gebündelt, nie eingelinkt. Damit bleibt der Wechsel auf eine kommerzielle Lizenz möglich.
4. Lizenz-kritische SDKs (ASIO) werden **nicht** mitgeliefert. ASIO ist eine Build-Option mit lokal installiertem SDK (gängige Praxis bei Audacity, Ardour). *Überholt 2026-10-25*: Steinberg stellt die ASIO-Header seit 2025 auch unter GPLv3, JUCE 8 bündelt sie. ASIO ist unter Windows immer an (DAW unter GPLv3, Produkt-Plugins unter der Steinberg-ASIO-Lizenz, `docs/THIRD_PARTY.md`).
5. Verkauf: Binaries und Support kostenpflichtig, Quellcode frei. Entscheidung über Closed-Source-Variante mit kommerziellen JUCE/Tracktion-Lizenzen erst, wenn Umsatz das rechtfertigt (JUCE kostenlos bis 50 000 USD Jahresumsatz).

## Folgen
- Jede Fremdbibliothek wird in `docs/THIRD_PARTY.md` mit Lizenz geführt (ab M0).
- Content-Bibliotheken nur mit Lizenzen, die kommerzielle Weitergabe erlauben (CC0, CC BY, CC Sampling Plus). Philharmonia-Samples sind ausgeschlossen.
- Für Windows bedeutete das zunächst WASAPI als Standard-Treiber; seit 2026-10-25 gibt es ASIO in jedem Build (Punkt 4).
