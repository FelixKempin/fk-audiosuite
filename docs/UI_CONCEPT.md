# UI-Konzept

Stand: 2026-10-08. Orientierung: Cubase-Projektfenster mit Zonen. Ziel: *vertraut für Cubase-Nutzer, aber aufgeräumter und moderner.*

## 1. Fensteraufbau: das Projektfenster

Ein Hauptfenster, fünf Zonen. Jede Zone ist ein- und ausblendbar (Tastenkürzel), die Größen sind per Ziehen änderbar und werden pro Projekt gespeichert.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ Menüleiste   Datei  Bearbeiten  Projekt  Audio  MIDI  Studio  Fenster  Hilfe  │
├──────────────────────────────────────────────────────────────────────────────┤
│ Werkzeugleiste  [Werkzeuge: ▸ ✂ ✎ ⌫ M 🔍]  [Snap ▾ Raster ▾]  [Zoom]  [Suche] │
├────────────┬──────────────────────────────────────────────┬──────────────────┤
│ LINKE ZONE │ PROJEKTZONE                                   │ RECHTE ZONE      │
│ Inspector  │ ┌──────────────────────────────────────────┐ │ Tabs:            │
│            │ │ Lineal (Takte/Zeit), Marker, Tempo        │ │ • VST-Rack       │
│ • Instrumente    │
│ Spurname   │ ├──────────┬───────────────────────────────┤ │ • Media          │
│ Eingang    │ │ Spurliste│ Arrange: Clips, Automation     │ │ • Plugins        │
│ Ausgang    │ │ Name     │ ▓▓▓▓▓▓  ▓▓▓▓▓                 │ │ • Presets        │
│ Inserts    │ │ M S R ●  │       ▓▓▓▓▓▓▓▓▓▓▓            │ │                  │
│ Sends      │ │ ───────  │ ╭─────╮ ╭────╮                │ │ Browser mit      │
│ EQ         │ │ ...      │ │MIDI │ │    │                │ │ Suche, Tags,     │
│ Fader      │ │          │ ╰─────╯ ╰────╯                │ │ Vorhören,        │
│            │ └──────────┴───────────────────────────────┘ │ Drag & Drop      │
├────────────┴──────────────────────────────────────────────┴──────────────────┤
│ UNTERE ZONE   Tabs: MixConsole | Key Editor | Drum Editor | Sampler | Takes   │
│ ┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐                                              │
│ │▐▌ ││▐▌ ││▐▌ ││▐▌ ││▐▌ ││▐▌ │   (Mixer-Kanäle mit Fader, Pan, Meter)        │
│ └───┘└───┘└───┘└───┘└───┘└───┘                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│ TRANSPORT  ⏮ ◀◀ ▶▶ ⏹ ▶ ● 🔁   001.01.01.000   00:00:00.000   120.00  4/4  🎵│
├──────────────────────────────────────────────────────────────────────────────┤
│ Statuszeile  48 kHz · 128 Samples · 5.3 ms · CPU 12 % · Disk ▂ · Projekt OK   │
└──────────────────────────────────────────────────────────────────────────────┘
```

Transportleiste **unten** (wie Cubase 12+), nicht als schwebendes Fenster. Sie ist immer sichtbar.

Menüleiste (seit M5b): Datei, Bearbeiten, Spur, Ansicht, Studio, Hilfe. Jeder Menüpunkt ist auch als Button mit Symbol erreichbar; das Menü Ansicht schaltet Zonen und Reiter (Inspector, untere Zone, MixConsole, Bildschirm-Keyboard, Key Editor).

## 2. Zonen im Detail

### Linke Zone: Inspector
Zeigt die ausgewählte Spur vollständig: Name/Farbe, Ein-/Ausgang, Inserts (8 Slots, aufklappbar), Sends, Kanal-EQ, Fader/Pan/Meter. Abschnitte sind Akkordeons, Zustand merkbar.

### Projektzone: Arrangement
- **Lineal** mit umschaltbarer Einheit, Marker-Zeile, Tempo-Zeile, Loop-Bereich (ziehbar).
- **Spurliste** links: Name, Farbe, M/S/Rec/Monitor-Buttons, Spurhöhe per Ziehen oder Tastatur, Ordner-Einklappen.
- **Untere Zone**: Reiter MixConsole (F3), Keyboard (F4), Key Editor (F5), Medien (F6).
- **Arrange-Bereich** rechts: Signaturspur unter dem Lineal (Tempo, Taktart, Tonart), Clips mit Waveform/MIDI-Vorschau, Fades als Griffe, Clip-Gain-Linie, Automationsspuren (mehrere Lagen) ausklappbar unter jeder Spur.
- **Werkzeuge**: Auswahl (Pfeil), Bereich, Schneiden, Stift, Radierer, Mute, Zoom. Tastenkürzel 1–7 wie Cubase. Smart-Tool: Der Pfeil wechselt an Clip-Kanten zu Trim, an Fade-Griffen zu Fade, in der oberen Hälfte Zeitverschiebung.
- **Drag-and-drop von Dateien**: Audio (WAV, AIFF, FLAC, OGG, MP3) landet auf Audiospuren, MIDI (.mid) auf Instrumentenspuren; passt die Spur unter dem Zeiger nicht, entsteht eine neue (bei MIDI mit FK Synth). Die Vorschau sagt „Neue Audiospur“ oder „Neue Instrumentenspur“. Eine MIDI-Datei mit mehreren Spuren belegt entsprechend viele Spuren untereinander.
- **Controller-Lanes im Key Editor**: Unter der Velocity-Lane beliebig viele Lanes (Button „Lane“ in der Werkzeugleiste des Editors), jede mit Artenliste links (Sustain, Sostenuto, Soft, Mod-Wheel, Pitch-Bend, Aftertouch, weitere CCs) und Minus zum Entfernen. Stufenlinie mit Punkten; leer erklärt die Lane, was zu tun ist.
- **MIDI Learn**: Button mit Stecker-Symbol in der Werkzeugleiste (rot im Lernmodus); solange er an ist, steht neben ihm der nächste Schritt („Regler in der DAW anfassen“, „Hardware-Regler bewegen“). Studio → MIDI-Zuordnungen öffnet die Liste mit Entfernen je Zeile.
- **Instrument im Spurkopf und Inspector**: Instrumentenspuren tragen einen Fenster-Button („Instrument öffnen“); der Inspector zeigt darunter den Instrumentnamen (Doppelklick öffnet) mit Öffnen- und Ersetzen-Button.
- **Audio- und MIDI-Einstellungen**: Unter der Geräteauswahl eine Liste aller MIDI-Eingänge mit Schaltern und „Neu suchen“; sie aktualisiert sich beim Ein- und Ausstecken.
- **Clip-Auswahl**: Klick wählt einen Clip, Strg+Klick schaltet ihn dazu oder weg, Shift+Klick nimmt ihn dazu, Strg+A wählt alle, Klick ins Leere hebt die Auswahl auf. Ausgewählte Clips tragen einen hellen Rahmen. Ziehen bewegt alle ausgewählten Clips gemeinsam, auch auf eine andere Spur derselben Art (Zielzeile wird hervorgehoben). Ausschneiden, Kopieren, Einfügen (am Positionszeiger auf der gewählten Spur), Duplizieren (direkt dahinter), Kleben und Löschen stehen als Buttons in der Werkzeugleiste, im Menü Bearbeiten und im Kontextmenü des Clips, jeweils mit Symbol und Tooltip.
- **Snap**: Raster (Takt, Beat, 1/8…1/64, Sekunde), Snap an Events, Snap an Cursor.

### Rechte Zone: Browser
Tabs: **VST-Instrumente** (Rack: alle Instanzen, pro Instanz Ausgänge aktivieren, Editor öffnen, MIDI-Spur hinzufügen), Instrumente (aus Bibliothek, mit Kategorien und Vorhören), Media (Dateisystem), Plugins (Effekte nach Kategorie), Presets. Drag & Drop auf Spur oder in leeren Bereich (erzeugt Spur).

### Untere Zone
- **MixConsole**: Kanäle wie Spuren, Ein-/Ausgang oben, Inserts, Sends, Pan, Fader mit Meter, Name/Farbe unten. Eingangsbus-Kanäle links, Master rechts. Scrollbar, Kanalbreite umschaltbar (schmal/normal/breit).
- **Key Editor** (Piano Roll): Klaviatur links, Notenraster, Controller-Lanes unten (Velocity, CC, Pitchbend), Quantisierungs-Leiste oben.
- **Drum Editor**: Zeilen pro Drum-Sound, Rauten statt Balken.
- **Sampler**: Waveform, Start/End/Loop-Griffe, Hüllkurven, Zuordnung auf Klaviatur.
- **Takes**: Comping-Ansicht der Loop-Aufnahmen.

### Eigene Fenster (nicht in Zonen)
Plugin-Editoren, Audio-Einstellungen, Studio-Verbindungen (Busse), Plugin-Manager, Tastenkürzel, Einstellungen, Content-Manager, Export-Dialog.

## 3. Designsystem

Alles Visuelle kommt aus **Design-Tokens** in `resources/theme/dark.json`, geladen in `ui::Theme`. Kein Hex-Wert im Code.

### Farben (Dark Theme, Vorschlag)

| Token | Wert | Verwendung |
|---|---|---|
| `bg.base` | `#1A1B1E` | Fensterhintergrund |
| `bg.panel` | `#222327` | Zonen, Panels |
| `bg.elevated` | `#2A2B30` | Karten, Popups |
| `bg.input` | `#151618` | Eingabefelder, Arrange-Hintergrund |
| `border.subtle` | `#2E2F34` | Trennlinien |
| `border.strong` | `#3C3D43` | Fokus-Rahmen, Zonengrenzen |
| `text.primary` | `#E8E8EA` | Haupttext |
| `text.secondary` | `#9A9BA3` | Beschriftungen |
| `text.disabled` | `#5C5D64` | |
| `accent` | `#4F8CFF` | Auswahl, aktive Buttons, Positionszeiger |
| `accent.soft` | `#4F8CFF33` | Auswahl-Hintergrund |
| `record` | `#E5484D` | Rec-Arm, Aufnahme |
| `solo` | `#F5A623` | Solo |
| `mute` | `#8E8E93` | Mute |
| `meter.low` / `meter.mid` / `meter.high` / `meter.clip` | `#30C56A` / `#E0C341` / `#F08A24` / `#E5484D` | Pegel |
| `grid.bar` / `grid.beat` / `grid.sub` | `#3A3B41` / `#2C2D32` / `#242529` | Raster |

**Spurfarben**: 16-Farben-Palette, gesättigt aber nicht grell, automatisch rotierend bei neuer Spur, Clips erben die Spurfarbe mit 70 % Deckkraft.

### Abstände und Formen
- 4-px-Raster: Abstände 4, 8, 12, 16, 24, 32.
- Eckenradius: 4 px für Buttons/Felder, 6 px für Clips, 8 px für Panels.
- Keine Schatten außer bei Popups (weich, 8 px).
- Keine 3D-Effekte, keine Verläufe auf Bedienelementen. Fader und Knöpfe sind flach mit klarer Füllung.

### Typografie
- Schrift: **Inter** (OFL), eingebettet.
- Größen: 11 px Beschriftungen, 12 px Standard, 13 px Spurnamen, 20 px Transport-Zeitanzeige (tabellarische Ziffern!).

### Icons
**Lucide** (ISV-Lizenz, SVG), 16 px in Leisten, 20 px in Transport. Eingebettet als Binärdaten, eingefärbt über Tokens.

## 4. Interaktionsregeln

- **Jede Funktion hat einen Button mit Symbol und Tooltip.** Tastenkürzel sind Beschleuniger, nie der einzige Weg. Leere Bereiche erklären, was zu tun ist.

- **Alles Doppelklickbare öffnet einen Editor**: Clip → Key Editor/Sample Editor in unterer Zone, Insert → Plugin-Fenster, Spurname → Umbenennen.
- **Rechtsklick** überall mit Kontextmenü, das dieselben Befehle wie das Hauptmenü nutzt (eine Befehlstabelle, `juce::ApplicationCommandManager`).
- **Mausrad**: scrollen; `Strg` + Rad: horizontal zoomen; `Shift` + Rad: horizontal scrollen; `Alt` + Rad: vertikal zoomen.
- **Tastatur**: Leertaste Play/Stop, `*` Record, `/` Loop, `Pos1` Anfang, `G`/`H` Zoom, `Strg+Z/Y` Undo/Redo. Vollständige Default-Keymap in `resources/keymap/default.json`, Cubase-kompatibel wo sinnvoll.
- **Drag & Drop** von außen: Audio-Dateien → neue Spur, MIDI-Dateien → Instrumentenspur, Plugin-Presets → Insert.
- **Tooltips** mit Verzögerung 600 ms, bei Parametern inklusive aktuellem Wert und Einheit.

## 5. Zuordnung zu JUCE-Komponenten (Ausblick auf `ui/`)

| UI-Element | Klasse | Basis |
|---|---|---|
| Hauptfenster | `ui::ProjectWindow` | `juce::DocumentWindow` |
| Zonen-Layout | `ui::ZoneLayout` | `juce::Component` + `juce::StretchableLayoutManager` |
| Arrangement | `ui::arrange::ArrangeView`, `TrackHeaderList`, `ClipComponent`, `TimeRuler`, `AutomationLane` | `juce::Component`, `juce::Viewport` |
| Mixer | `ui::mixer::MixConsole`, `ChannelStrip`, `Fader`, `LevelMeter` | eigene Komponenten |
| Key Editor | `ui::midi::KeyEditor`, `PianoKeyboard`, `NoteGrid`, `ControllerLane` | |
| Transport | `ui::TransportBar` | |
| Theme | `ui::Theme`, `ui::FkLookAndFeel` | `juce::LookAndFeel_V4` |
| Browser | `ui::browser::InstrumentBrowser`, `MediaBrowser`, `PluginBrowser` | `juce::TreeView`, `juce::ListBox` |

Jede Komponente kennt **nur** die Services, die sie braucht, und bekommt sie im Konstruktor. Komponenten halten keinen eigenen Projektzustand, sie lesen ihn aus den Services und reagieren auf deren Listener.
