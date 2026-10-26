# ADR 0008: DAW bleibt GPL, eigene Plugins getrennt und nicht öffentlich

Datum: 2026-10-09 · Status: angenommen · öffentliche Fassung vom 2026-10-25

## Kontext
Neben der DAW entstehen eigenständige Plugins (z. B. FK Rig), die verkauft werden. Das Repo steht unter GPLv3 (ADR 0003).

## Entscheidung
1. **Dieses Repo (GPLv3) ist die DAW** mit `core`, `dsp`, `plugins` (Plugin-Fundament und die mitgelieferten Effekte und Instrumente), `engine`, `services`, `ui`, `app`. Kostenlos, ohne Umsatzgrenze.
2. **Die eigenständigen Plugins liegen in einem eigenen, nicht öffentlichen Repo.** Es nutzt `core`, `dsp` und `plugins` dieses Repos unter der Lizenz des Rechteinhabers (alle Beiträge liegen bei ihm, ADR 0003 §2) und verlinkt nie Tracktion oder eine GPL-Fremdbibliothek.
3. **Die DAW lädt diese Plugins wie jedes VST3** über den Plugin-Scan. Mitgelieferte Lite-Ausgaben liegen als separate Dateien im Paket; nichts davon wird in die DAW eingelinkt.
4. **Lite und Pro** sind Laufzeitzustand (`plugins::edition::Edition`, `FeatureGate`), kein `#ifdef`. Die Lizenzprüfung selbst gehört zu den Plugins und liegt nicht in diesem Repo.

## Folgen
- `plugins` bleibt frei von Tracktion, damit es ohne DAW baut (`FK_BUILD_PLUGINS`).
- Code, den nur die eigenständigen Plugins brauchen, gehört nicht in dieses Repo.
- THIRD_PARTY.md führt pro Bibliothek, ob sie in geschlossenen Plugins eingelinkt werden darf.
