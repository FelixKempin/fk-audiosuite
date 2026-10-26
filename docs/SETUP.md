# Entwicklungsumgebung einrichten (Ubuntu 24.04)

## Werkzeuge und Bibliotheken

Stand der Maschine am 2026-10-08: GCC 13.3, Git und pkg-config vorhanden. Es fehlen CMake, Ninja, clang-format/clang-tidy und die JUCE-Systembibliotheken.

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build clang-format clang-tidy gdb \
  libasound2-dev libjack-jackd2-dev pipewire-jack \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
  libgl1-mesa-dev libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev \
  ladspa-sdk lv2-dev
```

## PipeWire als JACK-Server aktivieren

JUCE lädt zur Laufzeit `libjack.so.0`. Auf Ubuntu zeigt die Bibliothek standardmäßig auf das klassische JACK2, das nicht läuft; im Log steht dann `jack server is not running`, und JUCE fällt auf ALSA zurück (funktioniert, aber ohne Routing im PipeWire-Graphen). Zwei Möglichkeiten, die PipeWire-Variante zu nutzen:

**Dauerhaft** (empfohlen, einmalig mit Root):

```bash
sudo cp /usr/share/doc/pipewire/examples/ld.so.conf.d/pipewire-jack-x86_64-linux-gnu.conf /etc/ld.so.conf.d/
sudo ldconfig
```

**Pro Start** ohne Root:

```bash
pw-jack "./build/linux-debug/src/app/FKAudioSuite_artefacts/Debug/FK Audio Suite"
```

Prüfen: `ldconfig -p | grep libjack.so.0` muss auf einen Pfad mit `pipewire-0.3/jack` zeigen. Mit `qpwgraph` oder `helvum` sieht man die App dann als Knoten im Graphen.

## Repository

Submodule sind eingetragen (`external/tracktion_engine` mit JUCE darin, `external/Catch2`, `external/sfizz` mit eigenen Submodulen). Nach einem frischen Klon:

```bash
git submodule update --init --recursive --depth 1
```

sfizz baut aus dem Submodul mit, ohne Systempakete (eigener WAV/FLAC-Leser, kein libsndfile); sein Verzeichnis wird mit C++17 übersetzt, der eigene Code mit 23 (siehe CMakeLists.txt).

Auf dieser Maschine fehlen laut `pkg-config` konkret: xinerama, xext, xrandr, xcursor, fontconfig, gl. Ohne sie bricht die Konfiguration des App-Targets ab; `core`, `dsp` und die Tests bauen trotzdem (Preset `linux-libs-only`).

Ohne Root-Rechte lassen sich CMake, Ninja und clang-format auch per pip in ein virtuelles Umfeld installieren:

```bash
python3 -m venv ~/.local/fk-tools && ~/.local/fk-tools/bin/pip install cmake ninja clang-format
export PATH=~/.local/fk-tools/bin:$PATH
```

## Bauen (ab M0)

```bash
cmake --preset linux-debug          # oder linux-libs-only ohne JUCE-Systembibliotheken
cmake --build --preset linux-debug -j 3   # Tracktion braucht pro Job viel Speicher; mehr Jobs nur mit reichlich RAM
ctest --preset linux-debug
./build/linux-debug/src/app/FKAudioSuite_artefacts/Debug/FKAudioSuite
```

Vor jedem Commit formatieren:

```bash
clang-format -i $(git ls-files 'src/*.h' 'src/*.cpp' 'tests/*.h' 'tests/*.cpp')
```

## Windows

Einrichten, bauen und testen unter Windows: **`docs/WINDOWS.md`**. Die Pakete für Linux und
Windows baut der Workflow „Release“ (GitHub Actions). Gebaut wird auf Windows selbst, nicht über Kreuz.

Auf Windows nötig: Visual Studio 2022 mit „Desktopentwicklung mit C++“, CMake ≥ 3.25, optional
Inno Setup 6 (Installer). ASIO ist immer dabei (Header kommen mit JUCE).

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Release-Build, Pakete und Upload: `dist/README.md`.

## Editor

Empfohlen: VS Code mit den Erweiterungen *C/C++* (Microsoft) oder *clangd*, *CMake Tools*, *Clang-Format*. clangd braucht `compile_commands.json`, das CMake per `CMAKE_EXPORT_COMPILE_COMMANDS=ON` erzeugt (im Preset gesetzt). Alternativ CLion oder Qt Creator.

Als Java-Entwickler: IntelliJ-Nutzer finden sich in CLion sofort zurecht (gleiche Tastenkürzel, gleiche Refactoring-Werkzeuge). CLion ist seit 2025 für nicht-kommerzielle Nutzung kostenlos.

## Starten als Entwickler

`tools/fk-audiosuite` (einmal verlinken: `ln -s "$PWD/tools/fk-audiosuite" ~/.local/bin/`), danach überall `fk-audiosuite` (startet neu), `fk-audiosuite stop`, `fk-audiosuite log`.

## Sprache

Englisch ist Standard, Deutsch vollständig (ADR 0010). Die Wahl gilt für alle FK-Programme und steht in `~/.config/FKAudioSuite/common.json` (`{"language": "de"}`); ohne Wahl folgt die Sprache dem System. Zum Ausprobieren: DAW-Menü Studio → Sprache, im Plugin die Sprachwahl im Lizenzbereich, oder `LANG=de_DE.UTF-8` beim Start (nur ohne gespeicherte Wahl). Neue Texte: CODING_GUIDELINES.md §15; der Test `[i18n]` meldet fehlende Übersetzungen.

## Plugins scannen ohne Oberfläche

```bash
"./build/linux-debug/src/app/FKAudioSuite_artefacts/Debug/FK Audio Suite" --scan-plugins
```

Durchsucht die Standardpfade aller Formate (VST3: `~/.vst3`, `/usr/lib/vst3`; LV2: `~/.lv2`, `/usr/lib/lv2`) im Unterprozess, speichert die Liste in den Einstellungen und gibt alle gefundenen Plugins aus. Freie Testkandidaten: `sudo apt install lsp-plugins-lv2 calf-plugins` (zusammen über 220 Effekte).
