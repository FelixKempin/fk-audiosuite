# Architektur

Stand: 2026-10-08. Entscheidungen mit Begründung stehen in `docs/adr/`. Dieses Dokument beschreibt das *Wie*.

## 1. Leitprinzipien

1. **Abhängigkeiten zeigen nach innen.** Oberfläche kennt Anwendungslogik, Anwendungslogik kennt Engine, Engine kennt Tracktion. Nie umgekehrt. (Dependency Inversion, SOLID "D")
2. **Ein Modul, eine Verantwortung.** Jedes Verzeichnis unter `src/` ist ein CMake-Target mit klar benannter Aufgabe. (Single Responsibility, SOLID "S")
3. **Der Audio-Thread ist heilig.** Keine Allokation, kein Lock, kein I/O, keine Ausnahme. Alles, was dort läuft, ist in `CODING_GUIDELINES.md` Abschnitt 8 geregelt.
4. **Nicht duplizieren, was Tracktion Engine liefert.** Das Datenmodell (Edit, Track, Clip, Plugin) *ist* das von Tracktion. Wir bauen keine Parallelwelt, sondern Anwendungsfälle und Oberfläche darauf.
5. **DSP ist reine Mathematik.** Das Modul `dsp` kennt weder JUCE-GUI noch Tracktion und ist vollständig testbar.
6. **Erst lesbar, dann schnell.** Optimiert wird nur mit Messung, und nur im Audio-Pfad.

## 2. Technologie-Stack

| Bereich | Wahl | Version | Lizenz | ADR |
|---|---|---|---|---|
| Sprache | C++23 (eingeschränkter Feature-Satz) | GCC ≥ 13, MSVC ≥ 19.38 | – | 0001 |
| Build | CMake + Ninja, CMake Presets | ≥ 3.25 | – | 0001 |
| Framework | JUCE | 8.x | GPLv3 / kommerziell | 0002 |
| DAW-Engine | Tracktion Engine | aktueller `develop`-Branch, gepinnt als Submodul | GPLv3 / kommerziell | 0002 |
| Sampler | sfizz | ≥ 1.2 | BSD-2 | 0002 |
| Tests | Catch2 | 3.x | BSL-1.0 | 0005 |
| Formatierung/Analyse | clang-format, clang-tidy | ≥ 17 | – | 0005 |
| Settings/Presets | JSON (juce::JSON / juce::var) | – | – | |
| Bibliotheks-Index | SQLite (später, P2) | 3.x | Public Domain | |
| Icons | Lucide (SVG) | – | ISC | |

## 3. Schichten und Module

```
┌─────────────────────────────────────────────────────────────┐
│  app          Einstiegspunkt, Lebenszyklus, Composition Root │
├─────────────────────────────────────────────────────────────┤
│  ui           JUCE-Komponenten: Fenster, Zonen, Editoren     │
│               (kennt nur "services", nie Tracktion direkt)  │
├─────────────────────────────────────────────────────────────┤
│  services     Anwendungsfälle: ProjectService,               │
│               TransportService, TrackService, PluginService, │
│               RecordingService, LibraryService, ...          │
├────────────────────────┬────────────────────────────────────┤
│  engine                │  library           │  control      │
│  Tracktion-Adapter:    │  Content-Manager,  │  MIDI Learn,  │
│  EngineContext,        │  Preset-Index,     │  Mackie MCU,  │
│  Geräte, Scanner       │  Media-Browser     │  OSC          │
├────────────────────────┴────────────────────────────────────┤
│  plugins      Fundament (Edition, Presets, Modulation,       │
│               UI-Kit) und kleine Effekte (AudioProcessor)    │
├─────────────────────────────────────────────────────────────┤
│  dsp          Reine DSP-Bausteine, keine Framework-Abhängigkeit│
├─────────────────────────────────────────────────────────────┤
│  core         Gemeinsames: Result/Error, Logging, Ids, Units │
└─────────────────────────────────────────────────────────────┘
          ↓ darf benutzen (nur nach unten, nie nach oben)
   JUCE, Tracktion Engine, sfizz, Catch2  (externe Bibliotheken)
```

### Abhängigkeitsregeln (werden in CMake erzwungen)

| Modul | darf benutzen |
|---|---|
| `core` | nur Standardbibliothek und `juce_core` |
| `dsp` | `core`, Standardbibliothek. **Kein** `juce_gui_*`, **kein** Tracktion |
| `juce` | kompiliert JUCE und Tracktion genau einmal; reicht Include-Pfade und Schalter weiter (§7a) |
| `plugins` | `dsp`, `core`, `juce` (nur JUCE, **kein** Tracktion) |
| `engine` | `core`, `plugins`, `juce` (Tracktion) |
| `library` | `core`, `juce_core`, `juce_audio_formats` |
| `control` | `core`, `engine`, `juce_audio_devices` |
| `services` | `core`, `engine`, `library`, `control` |
| `ui` | `core`, `services`, `juce_gui_*`. **Kein** direkter Tracktion-Zugriff |
| `app` | alles |

Eigenständige FK-Plugins liegen **nicht** in diesem Repo, sondern in einem eigenen, nicht öffentlichen Repo, das dieses Repo einbindet und nur `core`, `dsp`, `plugins` baut (`FK_BUILD_APP=OFF`). Deshalb darf `plugins` nie Tracktion sehen. Die DAW lädt die Produkte als VST3 über den Plugin-Scan (ADR 0008).

Warum `ui` Tracktion nicht sehen darf: Die Oberfläche soll gegen stabile, von uns benannte Schnittstellen programmiert werden. So bleibt sie testbar (Services lassen sich durch Attrappen ersetzen) und wir können die Engine-Anbindung ändern, ohne jede Komponente anzufassen. Ausnahme, bewusst erlaubt: `ui` darf Tracktions **Datentypen** (`te::Track&`, `te::Clip&`) als *Handles* von Services empfangen und an sie zurückgeben, aber keine Tracktion-Methoden aufrufen. Die Grenze wird per `#include`-Regel in clang-tidy geprüft.

### Was ein "Service" ist

Ein Service ist eine Klasse mit Methoden, die genau einen Anwendungsfall ausführen, z. B. `TrackService::createAudioTrack(name) -> Result<TrackId>`. Services:

- werden per Konstruktor-Injektion zusammengesteckt (im `app`-Modul, der "Composition Root"),
- sind nur vom Message-Thread aufrufbar (UI-Thread),
- geben `Result<T>` zurück statt Ausnahmen zu werfen,
- benachrichtigen die Oberfläche über Änderungen per `juce::ListenerList` oder `juce::ValueTree::Listener`, nie per Rückruf in den Audio-Thread.

## 4. Threading-Modell

| Thread | Wer läuft dort | Darf |
|---|---|---|
| **Message-Thread** (UI) | Alle JUCE-Komponenten, alle Services, Tracktion-Editierung | allokieren, locken, I/O |
| **Audio-Thread** | Tracktion-Graph, alle `AudioProcessor::processBlock`, unser `dsp`-Code | **nur** rechnen, Atomics lesen, Lock-free-FIFOs |
| **Worker-Threads** | Plugin-Scan (Subprozess!), Datei-Import, Waveform-Berechnung, Bibliotheks-Index | allokieren, I/O; melden Ergebnis per `juce::MessageManager::callAsync` an UI |
| **MIDI-Input-Thread** | JUCE-Treiber | reicht Events über Lock-free-FIFO weiter |

Kommunikation UI → Audio: `std::atomic<float>` für Parameter, `juce::AbstractFifo` für Befehle. Audio → UI: `juce::AbstractFifo` für Meter-Daten, UI pollt per Timer mit 30 Hz. Tracktion Engine regelt den Großteil davon intern; die Regeln gelten für unseren eigenen Code in `plugins` und `dsp`.

## 5. Datenmodell und Persistenz

- **Projekt** = ein Ordner: `Projektname/Projektname.tracktionedit`, `Audio/`, `Presets/`, `Backups/`, `project.json` (unsere Metadaten: Version, Farben, UI-Zustand).
- **Edit** = Tracktions `te::Edit`, intern ein `juce::ValueTree` (hierarchischer Baum mit Properties, inkl. Undo). Alles, was der Nutzer am Projekt ändert, geht durch dieses ValueTree. Das gibt uns Undo/Redo und Änderungsbenachrichtigungen gratis.
- **Einstellungen** = JSON in `~/.config/FKAudioSuite/settings.json` (Linux) bzw. `%APPDATA%\FKAudioSuite\` (Windows).
- **Plugin-Liste** = `juce::KnownPluginList` als XML, plus Blacklist.
- **Content-Bibliothek** = Ordner mit `manifest.json` pro Bibliothek (Name, Version, Instrumente, SFZ-Pfade, Lizenz). Index zunächst als JSON, bei > 10 000 Einträgen SQLite.

## 6. Kernabläufe

### Start
1. `app` erzeugt `EngineContext` (kapselt `te::Engine`), lädt Einstellungen, initialisiert Audio-/MIDI-Geräte.
2. Composition Root baut Services, injiziert Abhängigkeiten.
3. `ProjectWindow` wird erzeugt, erhält Services.
4. Letztes Projekt oder Startdialog.

### Audio-Aufnahme (Mehrkanal)
1. Nutzer wählt pro Spur einen Eingang (UI → `TrackService::setInput(track, inputDevice)`).
2. Rec-Arm pro Spur, Record im Transport (→ `TransportService::record()`).
3. Tracktion schreibt pro Spur eine WAV-Datei in `Audio/` und erzeugt beim Stopp Clips. Latenz ist kompensiert.
4. Service benachrichtigt UI, Arrange-View zeichnet Waveforms (Berechnung im Worker-Thread, Cache auf Platte).

### Plugin einfügen
1. UI fragt `PluginService::availablePlugins()` (aus `KnownPluginList`).
2. `PluginService::insert(track, pluginDescription, slot)` → Tracktion instanziiert, fügt in den Graph ein.
3. Plugin-Fenster: `ui::PluginWindow` hält den `juce::AudioProcessorEditor`, einer pro Instanz.

### Fremde Plugins scannen
`FkEngineBehaviour::canScanPluginsOutOfProcess()` liefert `true`; Tracktion startet dann zum Scan eine zweite Instanz der App (gleiche Binärdatei, besondere Kommandozeile). `Application::initialise` erkennt sie über `EngineContext::handlePluginScanWorkerStartup` und beendet sich nach dem Scan, ohne Fenster oder Engine. Die bekannte Plugin-Liste speichert Tracktion in seinen Einstellungen. Formate: VST3 und LV2 (Linux). CLAP fehlt JUCE und Tracktion host-seitig noch; siehe FEATURES.md §8.

### Eigene Plugins
`plugins::CompressorProcessor : plugins::InternalPlugin : juce::AudioPluginInstance` nutzt `dsp::Compressor`. Registrierung (`EngineContext::registerInternalPlugins`): Die Beschreibungen aus `plugins::PluginCatalog` landen in Tracktions `knownPluginList`, und der Hook `PluginManager::createPluginInstance` baut FK-Plugins selbst, alles andere geht an JUCEs Format-Manager. Tracktion behandelt unsere Plugins damit als `te::ExternalPlugin`: gleiche Liste, Automation, Zustand im Projekt, Latenzausgleich. Dieselben Klassen können später unverändert als VST3/CLAP exportiert werden. Ein neues Plugin braucht genau einen Eintrag in `PluginCatalog.cpp`.

## 7. Verzeichnisstruktur (Ziel)

```
FKAudioSuite/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/                 Hilfsfunktionen (Warnungen, Sanitizer, Modul-Makro)
├── external/              Git-Submodule: JUCE, tracktion_engine, sfizz, Catch2
├── src/
│   ├── core/              fk::core
│   ├── dsp/               fk::dsp
│   ├── plugins/           fk::plugins (Fundament für alle Plugins, kleine Effekte; edition/, ui/ = UI-Kit)
│   ├── engine/            fk::engine
│   ├── library/           fk::library
│   ├── control/           fk::control
│   ├── services/          fk::services
│   ├── ui/                fk::ui
│   └── app/               main.cpp, Application
├── tests/                 Spiegelt src/, Catch2
├── resources/             Icons, Fonts, Theme-JSON, Default-Keymap
├── content/               Mitgelieferte Presets, kleine Beispiel-Samples (Content der eigenständigen Plugins liegt nicht hier)
└── docs/
```

Jedes Modul hat `CMakeLists.txt`, Header unter `src/<modul>/fk/<modul>/*.h`, damit Includes immer `#include "fk/dsp/Biquad.h"` lauten und die Herkunft sofort sichtbar ist.

## 7a. Build-Besonderheit: JUCE-Module genau einmal kompilieren

JUCE- und Tracktion-Module sind CMake-Interface-Bibliotheken: Ihre Quellen werden in das Target kompiliert, das sie verlinkt. Würden App, Tests und mehrere Module sie verlinken, gäbe es jede JUCE-Klasse mehrfach. Deshalb:

- **Nur `fk_juce` verlinkt `juce::*` und `tracktion::*`, und zwar PRIVATE** (`src/juce/CMakeLists.txt`). Include-Pfade und `JUCE_MODULE_AVAILABLE_*`-Makros reicht es als INTERFACE weiter.
- `fk::plugins` und `fk::engine` verlinken `fk::juce`; alle anderen bekommen es transitiv.
- Die Modul-Schalter (`JUCE_JACK=1`, `JUCE_PLUGINHOST_VST3=1`, ...) stehen PUBLIC an `fk_juce`, damit jede Übersetzungseinheit JUCE-Header mit denselben Schaltern sieht.
- `DEBUG`/`NDEBUG` setzt die Wurzel-`CMakeLists.txt` für alle Targets gleich. JUCE prüft das beim Linken; unterschiedliche Schalter würden Klassenlayouts in JUCE-Headern ändern (ODR-Verletzung).
- Warnungsflags gelten pro Quelldatei (`fk_set_warnings_for_sources`), damit Fremdcode nicht an unserem `-Werror` scheitert.
- **Ohne DAW** (`FK_BUILD_PLUGINS=ON`, `FK_BUILD_APP=OFF`, Preset `linux-plugins-only`; so bauen eigenständige Plugins): `fk_juce` entfällt, und `fk::plugins` ist eine INTERFACE-Bibliothek, die ihre Quellen und die JUCE-Module an das verlinkende Target weiterreicht, genau wie JUCE-Module selbst. Jedes VST3, jedes Standalone und die Tests kompilieren JUCE dann genau einmal für sich, und `juce_add_plugin` setzt seine eigenen Schalter (`JUCE_STANDALONE_APPLICATION`, Splash) ohne Konflikt. Tracktion ist in diesem Modus gar nicht im Build (ADR 0008 §2).

## 8. Fehlerbehandlung

- `core::Result<T>` = `std::expected<T, core::Error>`. `Error` trägt Code (enum class), Nachricht und Kontext.
- Programmierfehler (verletzte Vorbedingung) → `jassert`, im Release still.
- Fremdbibliotheken, die werfen (selten, z. B. Dateisystem), werden an der Modulgrenze gefangen und in `Result` übersetzt.
- Audio-Thread: weder `Result` mit Strings noch Ausnahmen. Fehler dort sind Programmierfehler → `jassert`.

## 9. Logging

`core::Log` als dünne Hülle um `juce::FileLogger`. Ebenen: `debug`, `info`, `warn`, `error`. Im Audio-Thread verboten; dort nur `jassert` oder Zähler in Atomics, die der UI-Timer ausliest.

## 10. Plattformen

- **Linux** (Entwicklungsplattform): Ubuntu 24.04, GCC 13, PipeWire über JACK-API.
- **Windows**: MSVC 2022, WASAPI, DirectSound und ASIO (Header aus JUCE, ADR 0003).
- CI: GitHub Actions, beide Plattformen, Build + Tests + clang-format-Check.

## 11. Entscheidungen, die noch anstehen

| Thema | Wann | Optionen |
|---|---|---|
| Plugin-Sandboxing (Out-of-Process) | nach 0.5 | eigener Host-Prozess vs. in-process bleiben |
| Index-Datenbank | wenn JSON zu langsam | SQLite |
| Lua-Scripting für Controller | nach 1.0 | sol2 |
| Light Theme | 1.0 | Theme-Tokens sind von Anfang an vorgesehen |
