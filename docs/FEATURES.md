# Feature-Analyse

Stand: 2026-10-08, Prioritäten am 2026-10-12 mit der neuen [Roadmap](ROADMAP.md) abgeglichen. Dieses Dokument beantwortet: *Was muss die DAW können, in welcher Reihenfolge, und was liefert uns Tracktion Engine bereits?* Offene Punkte nennen in der Notiz ihren Roadmap-Meilenstein (A1 … E).

## Zielbild

Eine DAW für **Aufnahme, Produktion und Mix** von Musik mit echten Instrumenten und virtuellen Instrumenten. Zielgruppe: Musiker und Produzenten, die Cubase/Logic/Studio One kennen und eine bezahlbare, offene Alternative wollen. Kein Live-Performance-Tool (wie Ableton Live), keine Notensatz-Software (wie Dorico); ein Noten-Editor zum Bearbeiten gehört dazu (Abschnitt 5a).

## Prioritäten

| Stufe | Bedeutung | Ziel-Release |
|---|---|---|
| **P0** | Ohne das ist es keine DAW | 0.1 (intern nutzbar) |
| **P1** | Damit kann man ein Projekt fertig produzieren | 0.5 (öffentliche Beta) |
| **P2** | Damit ist es ein Produkt, das man verkaufen kann | 1.0 |
| **P3** | Später, wenn Nutzer es verlangen | nach 1.0 |
| **–** | Bewusst nicht (Out of Scope) | |

Spalte **TE** = liefert Tracktion Engine das fertig? *ja* / *teilweise* / *nein* (wir bauen es).

---

## 1. Projekt und Transport

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Projekt anlegen, öffnen, speichern, Speichern unter | P0 | ja | Format: Tracktion-Edit (XML) im Projektordner; Speichern unter und zuletzt geöffnet seit 2026-10-12 |
| Projektordner mit Audio/, Presets/, Backups/ | P0 | teilweise | Ordnerstruktur legen wir fest |
| Autosave und Backup-Rotation | P0 | nein | **fertig 2026-10-12** (A1) |
| Transport: Play, Stop, Record, Loop, Return-to-Start | P0 | ja | |
| Positionsanzeige Takte/Zeit/Samples/Timecode | P0 | ja | **fertig 2026-10-12** (A3): Takte plus umschaltbare Zeitanzeige |
| Tempo und Taktart, Tempospur (Änderungen über Zeit) | P0/P1 | ja | Fixes Tempo P0, Tempospur P1 |
| Metronom (Click), Vorzähler | P0 | ja | **Vorzähler fertig 2026-10-09** (Transport: kein/1 Takt/2 Takte) |
| Marker und Markerspur, Cycle-Marker | P0 | ja | **Markerspur fertig 2026-10-12** (A2b): setzen, ziehen, benennen, springen, Loop auf Abschnitt; eigene Cycle-Marker offen |
| Pre-Roll / Post-Roll | P1 | ja | **fertig 2026-10-12** (A3): bei Punch-Aufnahme |
| Undo/Redo, unbegrenzt, mit Verlauf | P0 | ja | TE UndoManager; Verlauf als Liste seit A8a |
| Tastaturkürzel, konfigurierbar | P1 | nein | A8b fertig: Editor mit Suche und Konflikterkennung, Cubase-Belegung als Vorgabe |
| Projektvorlagen (Templates) | P1 | nein | **fertig 2026-10-12** (A1): ohne Audio-Clips |

## 2. Spuren

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Audiospur (mono/stereo) | P0 | ja | |
| Instrumentenspur (MIDI + Instrument-Plugin) | P0 | ja | |
| MIDI-Spur → Rack-Instrument (Instanz + MIDI-Kanal wählbar) | P1 | ja | B1a fertig, siehe Abschnitt 5a |
| MIDI-Spur → externes Gerät (Hardware-Synth, MIDI-Out + Kanal) | P1 | ja | B1b fertig: Spur > MIDI-Spur, Kanal, Programm und Bank im Inspector |
| Gruppenspur / Bus | P1 | ja | seit M6a: Audiospur mit Gruppen-Flag, Routing über TrackOutput, verschachtelbar |
| FX-Spur (Effekt-Return) | P1 | ja | Aux Send/Return |
| Ordnerspur | P1 | ja | **fertig 2026-10-12** (A2c): verschachtelt, zuklappbar, Mute/Solo für den Inhalt, Clip-Übersicht |
| Marker-, Tempo-, Taktartspur | P1 | ja | |
| Mehrkanal-Spuren (Surround) | P3 | teilweise | |
| Spurfarben, Icons, Umbenennen, Duplizieren | P0 | ja | **fertig 2026-10-12** (A2a): Farbe, Duplizieren, Umsortieren per Ziehen, Ausblenden, Löschen; Icons offen |
| Spur einfrieren (Freeze) als Track Bake: Instrument, Inserts bis Slot N, Gruppen mit Zuführungen, Sperre bis "Bake lösen" (ADR 0009) | P2 | ja | 2026-10-11 |
| Spurvorlagen (Track Presets) | P2 | nein | D1 |
| VCA-Fader | P2 | nein | D3 |

## 3. Audio-Aufnahme

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Aufnahme mono/stereo auf eine Spur | P0 | ja | |
| **Mehrkanal-Aufnahme**: viele Spuren gleichzeitig, jede mit eigenem Interface-Eingang | P0 | ja | Kernanforderung |
| Eingangsmonitoring (direkt, über DAW, automatisch) | P0 | ja | |
| Loop-Aufnahme mit Takes | P1 | ja | **fertig 2026-10-09**: Projektordner ist Tracktion-Projekt, Takes im Clip-Menü wählbar, Take-Lanes, Verteilen auf Spuren, Aufräumen, Take aus Datei |
| Comping (beste Teile aus Takes zusammensetzen) | P1 | teilweise | **v1 fertig 2026-10-09**: Bereich in einer Take-Lane ziehen schneidet und übernimmt den Take (ohne Crossfade-Render) |
| Punch In/Out | P0 | ja | **fertig 2026-10-12** (A3): zwischen den Locatoren, Taste I |
| Aufnahmeformat: WAV 24/32-bit float, FLAC | P0 | ja | |
| Pegelanzeige pro Eingang, Clipping-Warnung | P0 | ja | |
| Latenzkompensation für Aufnahme | P0 | ja | |

## 4. Audio-Editing

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Clips verschieben, kopieren, schneiden, trimmen | P0 | ja | **fertig 2026-10-09**: Trimmen an den Rändern (links verschiebt den Quellversatz, Snap gilt), Verschieben auch auf andere Spuren (gleiche Spurart), Mehrfachauswahl (Klick, Strg+Klick, Shift+Klick, Strg+A), Ausschneiden/Kopieren/Einfügen am Positionszeiger/Duplizieren (Strg+X/C/V/D) als Buttons, Bearbeiten-Menü und Kontextmenü; offen: Lasso-Auswahl |
| Clips kleben (mehrere zu einem) | P1 | teilweise | **fertig 2026-10-09**: Strg+J; MIDI führt die Noten zusammen (Tracktion), Audio rendert die Clips ohne Plugins in eine neue Datei in Audio/ (Fades und Clip-Gain eingerechnet, Takes gehen verloren) |
| Fade In/Out, Crossfade, Clip-Gain | P0 | ja | **Fades und Clip-Gain fertig 2026-10-09**; **automatische Crossfades fertig 2026-10-13** (A4b), auch beim Comping |
| Snap/Raster (Takt, Beat, Sekunde, Sample) | P0 | teilweise | **fertig 2026-10-13** (A4a): Takt, Zählzeit, 1/1–1/128 gerade/punktiert/triolisch, Sekunden, Samples; Nudge |
| Werkzeuge: Auswahl, Schneiden, Stift, Radierer, Mute, Zoom | P0 | nein | **fertig 2026-10-13** (A4): dazu Kleber, Bereich, Lasso; Tasten 1–8 wie Cubase |
| Stumm/Solo pro Clip | P1 | ja | Stumm **fertig 2026-10-13** (A4a); Solo pro Clip offen |
| Normalisieren, Umkehren, Stille einfügen | P0 | ja | **fertig 2026-10-13** (A4b/c), dazu Phase umkehren |
| Time-Stretch / Pitch-Shift (Audio an Tempo anpassen) | P0 | ja | **fertig 2026-10-13** (A5): Signalsmith Stretch, Musikmodus mit Tempo-Erkennung, Tonhöhe, Strecken |
| Warp-Marker (Audio-Quantisierung) | P2 | ja | D2 |
| Audio-zu-MIDI | P3 | nein | E |
| Clip-Effekte (Offline-Bearbeitung) | P2 | teilweise | Sample-Editor mit sieben Offline-Bearbeitungen seit 2026-10-13 (A5c); Plugins als Offline-Effekt offen |

## 5. MIDI und Instrumentenspuren

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| MIDI-Eingabe von Keyboard, Echtzeit-Aufnahme | P0 | ja | |
| Piano-Roll-Editor (Key Editor) | P0 | nein | größtes Stück UI nach dem Arrangierfenster |
| Velocity-, Controller-Lanes (CC, Pitchbend, Aftertouch) | P0 | ja (Daten) | **Velocity-Lane fertig 2026-10-09** (Balken ziehen); **Controller-Lanes fertig 2026-10-09**: beliebig viele Lanes (Button „Lane“), Art je Lane wählbar (Sustain CC64, Sostenuto CC66, Soft CC67, Mod-Wheel CC1, Pitch-Bend, Aftertouch, jeder CC), Stufenlinie mit Punkten, Klick setzt, Ziehen zeichnet, Rechtsklick löscht; aufgezeichnete Daten erscheinen sofort |
| Quantisierung (Raster, Stärke, Swing) | P0 | ja | B3c: Stärke, Swing und Groove aus einem anderen Clip (Quantisier-Optionen im Key Editor) |
| Step-Eingabe | P2 | nein | B3c fertig: Key Editor „Step“, Akkorde durch gleichzeitiges Halten, Pause/Zurück |
| Drum-Editor | P1 | nein | B2 fertig: Drum-Maps (GM, FK Drums, eigene als JSON), Rauten, Anschlag ziehen, Quantisieren, F8 |
| Noten-Transposition, Längen, Legato | P1 | ja | B3a fertig: Key Editor > Funktionen (Transponieren, feste Länge, Legato, Humanize, Anschlag, Doppelte löschen); Listen-Editor (F9) |
| MIDI-Effekte (Arpeggiator, Chord, Transpose) | P2 | teilweise | B3b fertig: FK Arpeggiator, FK Chord, FK Transpose; MIDI-Inserts sitzen automatisch vor dem Instrument |
| Akkordspur mit Akkordassistent | P3 | nein | E, Cubase-Alleinstellungsmerkmal |
| **Signaturspur**: Tempo (Sprung/Rampe), Taktart, Tonart am Raster; Projekttonart | P0 | ja (TempoSequence mit Kurve, PitchSequence) | **fertig 2026-10-09**, `ui/SignatureTrack` |
| **Akkorde nach Stufen** im Key Editor (I–VII relativ zur Tonart, Dreiklang/Septakkord/Umkehrung) | P0 | teilweise (Scale, Chord) | **fertig 2026-10-09**: Stufenliste zeigt Akkordsymbol, Button hängt an, Alt+Klick setzt an der Mausposition; skalenfremde Zeilen gedimmt |
| Mehrere Automationsspuren je Spur gleichzeitig | P0 | ja | **fertig 2026-10-09**, Plus/Minus im Lagenkopf, Liste im Projekt |
| MIDI-Import/Export (SMF) | P1 | ja | **Import fertig 2026-10-09**: .mid/.midi per Drag-and-drop aus Dateimanager und Medien-Browser, Menü Datei → Audio- oder MIDI-Datei importieren; jede Spur der Datei wird ein Clip, auf Instrumentenspuren, fehlende werden mit FK Synth angelegt; Tempo übernehmen und Export seit A7 |
| MPE-Unterstützung | P2 | ja | |
| MIDI 2.0 | P3 | nein | |

## 5a. Instrumente hosten: Spur-Instrumente, Rack-Instrumente, Multi-Output

Große Instrumente wie Superior Drummer, EastWest Opus, Kontakt oder Omnisphere sind **multi-timbral** (eine Instanz, 16 MIDI-Kanäle, mehrere Sounds) und **multi-output** (bis zu 16 oder 32 Stereo-Ausgänge). Eine DAW, die das nicht abbildet, ist für diese Plugins unbrauchbar. Cubase löst das mit zwei Instrument-Arten, wir übernehmen das.

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| **Spur-Instrument**: eine Instrumentenspur = eine Plugin-Instanz, MIDI und Audio auf derselben Spur | P0 | ja | der einfache Fall |
| Instrument öffnen und ersetzen | P0 | ja | **fertig 2026-10-09**: Button im Spurkopf und Inspector (Doppelklick auf den Instrumentnamen öffnet), „Instrument ersetzen“ im Inspector tauscht das Plugin, Inserts, Eingang und Clips bleiben |
| **Rack-Instrument**: Instanz lebt im "VST-Instrumente"-Rack, beliebig viele MIDI-Spuren senden auf Kanal 1–16 | P1 | ja | B1a fertig: Spur > Rack-Instrument; MIDI-Kanal je Spur über eine Patch Bay (gilt für Clips und Live-Spiel) |
| **Multi-Output**: jeder aktivierte Plugin-Ausgang bekommt einen eigenen Mixer-Kanal (Inserts, Sends, Fader) | P1 | ja | B1a fertig; FK Sampler hat 8 Stereo-Ausgänge (SFZ `output=`) |
| Ausgänge im Rack aktivieren/deaktivieren wie in Cubase ("Ausgänge aktivieren") | P1 | ja | B1a fertig: Inspector > Ausgänge |
| MIDI-Kanal pro Spur, Transposition, Velocity-Offset, Programmwechsel | P0 | ja | Kanal (Rack, Gerät) und Programmwechsel (Gerät) seit B1; Transposition, Velocity B3 |
| Mehrere MIDI-Spuren → eine Instanz, gleichzeitig aufnehmen | P1 | ja | z. B. Streicher-Sektion in EastWest |
| VST3-MIDI-Zuordnung: CC → Plugin-Parameter (VST3 bekommt keine rohen CCs!) | P0 | ja | JUCE/TE bilden `IMidiMapping` ab |
| Keyswitches und Artikulationen: Noten unter dem Spielbereich senden | P1 | ja (Daten) | B4 fertig: Keyswitch je Note vor der Note |
| **Spielweise als Noten-Eigenschaft**: je Note Strich/Verzierung (Staccato, Akzent, Tremolo, …), als Anweisung auf der Zeitachse Tonerzeugung/Klangfarbe (pizz., sul pont., con sord.) | P1 | nein | B4: je Note fertig (Kennung an der Note); Anweisungen auf der Zeitachse (Tonerzeugung, Klangfarbe, Dämpfer als Bereich) offen |
| Expression Maps: Spielweise → Kanal/CC/Keyswitch des Ziel-Plugins beim Abspielen, Nachsuchen beim Springen, Import von Plugin-Spielweisen-Listen (JSON) | P1 | nein | B4 fertig: Kanal, Keyswitch und CC je Note (dadurch kein Nachsuchen nötig), FK-Orchestra-Streicher und Keyswitches ab C0 mitgeliefert, JSON-Import |
| Notenzeichen und Artikulations-Lane im Key Editor | P1 | nein | B4 fertig: Zeichen an den Notenbalken, Lane mit einer Zeile je Spielweise |
| Keyswitches beim Einspielen in Noten-Eigenschaften umwandeln | P2 | nein | |
| **Noten-Editor** (Notenansicht derselben MIDI-Clips mit Spielweisen-Zeichen, bearbeitbar) | P2 | nein | kein Druck-Notensatz (2026-10-09) |
| MusicXML-Import/-Export mit Spielweisen | P2 | nein | Partitur aus MuseScore/Dorico laden und mit FK Orchestra abspielen |
| Sidechain-Eingang für Instrumente (z. B. Vocoder) | P2 | ja | |
| Instrumenten-Preset und Zustand mit dem Projekt speichern (VST3 State) | P0 | ja | |
| Disk-Streaming: Sache des Plugins; wir garantieren nur große Puffer-Optionen und keine Audio-Thread-Blockaden | P0 | – | |

### Kompatibilitätsziele für fremde Instrumente

| Plugin | Format | Windows | Linux | Besonderheiten |
|---|---|---|---|---|
| Superior Drummer 3 (Toontrack) | VST3 | nativ | über yabridge/Wine, funktioniert laut Nutzerberichten | 16 Stereo-Outs, eigenes Mixer-Routing |
| EastWest Opus / Play | VST3 | nativ | über yabridge/Wine, **Lizenzmanager (iLok) läuft unter Wine meist nicht** | multi-timbral 16 Kanäle, 16 Outs, Keyswitches |
| Kontakt 7/8 (NI) | VST3 | nativ | yabridge, Native Access unter Wine problematisch | 16 Kanäle, 32 Outs, Scripting |
| Spitfire (BBCSO, LABS) | VST3 | nativ | yabridge | Keyswitches, CC1/CC11 |
| Omnisphere | VST3 | nativ | yabridge | 8 Parts, 8 Outs |
| Surge XT, Vitalium, Dexed (frei) | VST3/CLAP/LV2 | nativ | **nativ** | |
| eigene Instrumente | intern + VST3/CLAP-Export | nativ | nativ | |

Fazit: Auf **Windows** sind alle kommerziellen Instrumente nativ nutzbar, auf **Linux** hängt es von yabridge und vom Kopierschutz des Herstellers ab. Das ist eine Eigenschaft des Linux-Ökosystems, nicht unserer DAW; jede Linux-DAW (Reaper, Bitwig, Ardour) hat dieselbe Grenze. Wir dokumentieren das ehrlich und machen yabridge-Plugins im Plugin-Manager erkennbar.

## 6. Automation

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Automation für Volume, Pan, Mute | P0 | ja | |
| Automation aller Plugin-Parameter | P0 | ja | |
| Automationsspuren (Lanes) unter der Spur | P0 | nein | UI; **fertig 2026-10-09** (M6: Kurve je Spur, Punkte setzen/ziehen/löschen, Read/Touch/Latch/Write je Spur, Lesen/Schreiben global) |
| Modi: Read, Write, Touch, Latch | P1 | ja | |
| Kurven (linear, exponentiell, S-Kurve) | P1 | ja | |
| Automation aus Controller aufnehmen | P1 | ja | **fertig 2026-10-09** über MIDI Learn: gelernte Regler bewegen den Parameter wie ein Mausregler, Write/Latch zeichnen mit |

## 7. Mixer und Routing

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| Mixer (MixConsole): Fader, Pan, Mute, Solo, Rec-Arm, Meter | P0 | ja | UI selbst |
| Insert-Slots pro Kanal (mind. 8) | P0 | ja | |
| Sends zu FX-Spuren (pre/post) | P1 | ja | |
| Ein-/Ausgangs-Routing pro Spur frei wählbar | P0 | ja | seit M6a: Master oder Gruppe; seit A6 auch Direct Out auf jedes Ausgangspaar |
| Master-Kanal mit Inserts | P0 | ja | |
| Kanal-EQ und Kanal-Strip (Gate, Comp, Sat, Limiter) wie Cubase | P2 | nein | D3, eigene Plugins, fest im Kanal |
| Sidechain-Routing | P1 | ja | A6 fertig |
| Pegelmeter: Peak, RMS, LUFS | P0/P2 | teilweise | Peak fertig; LUFS, True Peak, Korrelation: D3 |
| Mixer-Snapshots | P3 | nein | |
| Kanal-Linking | P3 | nein | |

## 8. Plugin-Hosting (fremde Plugins)

| Feature | Prio | TE | Notiz |
|---|---|---|---|
| VST3 (Linux .so, Windows .vst3) | P0 | ja | vollständig: mehrere Audio-Busse, Sidechain, MIDI-Mapping, Note Expression, State |
| CLAP | P1 | nein | JUCE 8 und Tracktion hosten CLAP noch nicht; braucht clap-juce-extensions host-seitig oder eigenes CLAP-Format. Stand 2026-10-08 zurückgestuft. |
| LV2 (Linux) | P1 | ja | seit M4 aktiv |
| Plugin-Scanner mit Blacklist, Abstürze überleben | P0 | ja | M4: Scan im Unterprozess über Tracktion, Dead-Man's-Pedal-Datei zusätzlich |
| Plugin-Manager (Liste, Kategorien, Favoriten, Ausblenden) | P1 | nein | A8c fertig: Reiter „Meine Plugins“, Menü mit Favoriten und Kategorien |
| Plugin-Fenster (natives GUI), generische Parameter-Ansicht | P0 | ja | |
| Presets speichern/laden (Plugin-Format und eigenes) | P1 | ja | A8c fertig: für jedes Plugin eigene Presets, dazu die Werks-Programme |
| Plugin Delay Compensation | P0 | ja | |
| Sandboxing (Plugin in eigenem Prozess) | P3 | nein | später, ADR fällig |
| Windows-VSTs unter Linux via yabridge | P1 | – | nur Doku, nichts zu bauen |
| VST2 | – | – | keine Lizenz mehr erhältlich |
| AU | – | – | nur macOS |

## 9. Mitgelieferte Effekte (eigene Entwicklung)

Alle als interne Plugins auf Basis von `juce::AudioProcessor`, DSP-Kern in `fk::dsp` separat und getestet. Später auch als VST3/CLAP exportierbar.

| Effekt | Prio | Notiz |
|---|---|---|
| Parametrischer EQ (4–8 Bänder, Analyzer) | P0 | Erstes DSP-Lernobjekt: Biquad-Filter |
| Kompressor (Threshold, Ratio, Attack, Release, Knee, Makeup, Sidechain) | P0 | Envelope Follower |
| Limiter (True Peak, Lookahead) | P0 | |
| Gate / Expander | P0 | A6 |
| Distortion / Saturation (mehrere Algorithmen, Oversampling) | P0 | Waveshaper |
| Delay (Stereo, Ping-Pong, Tempo-Sync) | P0 | A6 |
| Reverb (algorithmisch) | P0 | A6; eigener (FDN) |
| Chorus / Flanger / Phaser | P2 | D4 |
| Stereo-Tool (Breite, M/S) | P2 | D4 |
| Tuner | P1 | für Gitarristen |
| Analyzer / Meter (Spektrum, LUFS, Korrelation) | P2 | D3/D4 |
| Gain / Utility (Trim, Phase, Mono) | P0 | Einfachstes Plugin, erstes Übungsobjekt |

Ergänzend bündeln wir freie Fremd-Plugins als separate Binaries (siehe Lizenz-ADR): Airwindows (MIT), optional Dragonfly Reverb, LSP.

## 10. Mitgelieferte Instrumente (eigene Entwicklung)

| Instrument | Prio | Notiz |
|---|---|---|
| **Sampler** (SFZ-Format über sfizz) | P0 | Trägt die gesamte Orchester-/Piano-/Drum-Bibliothek. **Fertig 2026-10-09**: FK Sampler lädt .sfz, Datei im Projekt, Lautstärke, 64 Stimmen |
| **Drum-Sampler** (Pads, Kits, pro Pad Hüllkurve/Pitch) | P1 | B2 |
| **Subtraktiver Synth** (2 Osz, Filter, 2 Env, 2 LFO, Unisono) | P1 | Lehrbuch-Synth, gut zum Lernen |
| Einfacher Sample-Player für eigene Samples per Drag & Drop | P1 | B2 fertig: Sampler-Spur aus Datei (chromatisch) oder als leeres Kit, Samples im Drum-Editor auf Zeilen ziehen |
| FM- oder Wavetable-Synth | P3 | |
| Gebündelte freie Synths als separate Binaries: Surge XT, Vitalium, Dexed | P2 | GPL, getrennt ausgeliefert |

## 10a. Eigene Instrumente und Effekte

Die FK-Plugins (z. B. FK Rig) entstehen in einem eigenen, nicht öffentlichen Repo. Die DAW lädt sie wie jedes VST3 und liefert ihre Lite-Ausgaben als separate Dateien mit.

## 11. Instrumenten-Bibliothek (Content)

| Inhalt | Prio | Quelle / Lizenz |
|---|---|---|
| Orchester (Streicher, Holz, Blech, Perkussion) | P1 | VSCO 2 Community Edition (CC0) im Content-Manager seit B5a; Sonatina Symphonic Orchestra (CC Sampling Plus) offen |
| Konzertflügel | P1 | Salamander Grand Piano (CC BY 3.0) im Content-Manager seit B5a |
| Drum-Kits (akustisch, elektronisch) | P1 | eigene Aufnahmen oder CC0-Quellen (z. B. freesound-Packs prüfen) |
| Bass, Gitarre, E-Piano, Orgel | P2 | SFZ-Quellen prüfen, ggf. selbst aufnehmen |
| Content-Download bei Bedarf statt Riesen-Installer | P1 | B5a fertig: Studio > Content-Manager, Download mit Fortschritt und Abbrechen, Platzprüfung |
| Content-Format: SFZ + FLAC, eigener Ordner pro Bibliothek, Manifest-Datei (JSON) | P1 | |

## 12. Media- und Preset-Bibliothek (Browser)

| Feature | Prio | Notiz |
|---|---|---|
| Media-Browser: Dateisystem, Audio-Vorhören, Drag & Drop ins Projekt | P1 | **v1 fertig 2026-10-09** (untere Zone, Reiter Medien, F6): Dateibaum mit Audiofilter, Wellenform, Vorhören, Einfügen am Positionszeiger, Ziehen ins Arrangement |
| Instrumenten-Browser: alle Instrumente und Presets, nach Kategorie/Tags | P1 | B5b fertig: Reiter Instrumente (F10), Suche, Tags (automatisch und eigene), Favoriten, Doppelklick legt Spur an |
| Preset-System für eigene Plugins, mit Tags und Favoriten | P1 | Presets seit A8c, Tags und Favoriten für Instrument-Presets im Browser seit B5b |
| Suche über alles | P2 | Index-Datenbank (SQLite) |
| Loop-Browser mit Tempo-Anpassung beim Vorhören | P3 | |

## 13. Audio-Hardware

| Feature | Prio | Notiz |
|---|---|---|
| Linux: PipeWire (über JACK-API), JACK, ALSA | P0 | JUCE |
| Windows: ASIO, WASAPI (shared/exclusive) | P1 | C1; ASIO-Header aus JUCE (GPLv3), ADR 0003 |
| Mehrkanal: alle Ein-/Ausgänge des Interfaces nutzbar, benennbar | P0 | |
| Audio-Setup-Dialog: Gerät, Samplerate, Puffergröße, Latenzanzeige | P0 | |
| Ein-/Ausgangs-Busse konfigurieren (wie Cubase "Studio-Verbindungen") | P1 | A6 fertig: alle Paare als Direct Out, eigene Namen für alle Anschlüsse |
| Control Room (Monitor-Sektion, Kopfhörer-Mixe für Musiker) | P3 | |
| Externe Clock / Sync (LTC, MTC) | P3 | |

## 14. MIDI-Hardware und Controller

| Feature | Prio | Notiz |
|---|---|---|
| MIDI-Eingänge/Ausgänge, Hot-Plug | P0 | **fertig 2026-10-09**: MIDI-Eingänge in den Audio- und MIDI-Einstellungen an-/abschaltbar, „Neu suchen“, Ein-/Ausstecken aktualisiert Einstellungen und Spureingänge (Tracktion prüft die Hardware regelmäßig); Spureingang „Alle MIDI-Eingänge“ ist Standard neuer Instrumentenspuren |
| Virtuelles Keyboard am Bildschirm | P1 | **fertig**; seit 2026-10-09 voller Umfang A0–C8, beim Öffnen am mittleren C (C4), Oktave-Buttons; Tonnamen überall wissenschaftlich (C4 = mittleres C) |
| **MIDI Learn** für jeden Parameter | P1 | **fertig 2026-10-09**: Button in der Werkzeugleiste (und Menü Studio), Regler in der DAW anfassen, Hardware-Regler bewegen; Zuordnung im Projekt gespeichert, Fenster „MIDI-Zuordnungen“ listet und entfernt; Bewegungen schreiben Automation, wenn Schreiben aktiv ist (Tracktion ParameterControlMappings) |
| **Mackie Control (MCU)** und erweiterte Einheiten | P1 | D5; TE hat MCU-Implementierung |
| HUI | P2 | |
| Generische Controller-Zuordnung (Fader/Encoder-Bänke, speicherbar) | P1 | |
| OSC (Tablets, TouchOSC) | P2 | |
| Controller-Skripte (Lua) für beliebige Geräte | P3 | Bitwig/Reaper-Ansatz |

## 15. Export und Import

| Feature | Prio | Notiz |
|---|---|---|
| Mixdown-Export WAV/FLAC, Bereich oder Ganzes, Dither | P0 | TE Renderer; seit A7 mit AIFF, Samplerate, Dither, LUFS-Normalisierung, Ausklingen |
| MP3/OGG/Opus-Export | P0 | A7 fertig: OGG (JUCE), MP3 über System-libmp3lame zur Laufzeit; Opus offen |
| Stem-Export (pro Spur/Bus) | P0 | A7 fertig: Spuren und Gruppen, Quellspuren laufen stumm mit |
| Import: WAV, AIFF, FLAC, MP3, OGG | P0 | JUCE; **fertig 2026-10-09**: Drag-and-drop aus dem Dateimanager auf die Spuransicht (Spur und Zeit unter dem Zeiger, ins Leere = neue Spur, mehrere Dateien auf Spuren darunter) und Datei → Audiodatei importieren; Kopie nach Audio/ |
| MIDI-Datei Import/Export | P1 | Import fertig, seit A7 mit Tempo-Übernahme; Export A7 fertig (SMF Typ 1) |
| Projekt-Archiv (alles in einen Ordner/ZIP) | P2 | D1 |
| Import aus anderen DAWs (AAF/OMF) | – | |

## 16. Oberfläche (allgemein)

| Feature | Prio | Notiz |
|---|---|---|
| Ein Projektfenster mit Zonen (siehe UI_CONCEPT.md) | P0 | |
| Dark Theme, modern, hochauflösungsfähig | P0 | |
| Light Theme | P2 | |
| Zoom (horizontal/vertikal), Zoom-Presets, Zoom auf Auswahl | P0 | A8a fertig: ganzes Projekt, Auswahl, H/G, Takt-Zooms, eigene Presets; vertikal über Spurhöhe |
| Mehrere Fenster (Mixer, Editor abdockbar), Multi-Monitor | P1 | D5 |
| Werkzeugleiste anpassbar | P2 | |
| Lokalisierung (DE, EN) | P1 | C3; Englisch Standard, Deutsch vollständig über Übersetzungstabellen (ADR 0010), weitere Sprachen ohne Codeänderung |
| Onboarding / Erster Start: Audio einrichten, Content laden | P1 | C3 |

## 17. System und Robustheit

| Feature | Prio | Notiz |
|---|---|---|
| Absturzschutz: Autosave, Recovery beim Neustart | P0 | **fertig 2026-10-12** (A1) |
| Plugin-Scan im Subprozess (Absturz eines Plugins killt nicht die App) | P0 | |
| Logging mit Log-Datei, Diagnose-Export | P0 | **fertig 2026-10-12** (A1) |
| Einstellungen (JSON im Benutzerordner) | P0 | |
| Update-Hinweis | P2 | |
| Installer: Linux AppImage/Flatpak/DEB, Windows Installer | P1 | C2 |

## 18. Innovation: Was uns von Cubase abheben kann

Noch nicht entschieden, aber als Kandidaten festgehalten. Wird nach 0.5 mit Nutzer-Feedback priorisiert.

1. **Session-View für Musiker**: Ein "Record-Modus", der nur Eingänge, Pegel, Kopfhörer-Mix und Record zeigt. Keine Ablenkung beim Einspielen.
2. **Versionierung von Projekten** wie Git: Snapshots, Vergleich, Zurückspringen. Kein anderes DAW macht das sauber.
3. **Takes-Vergleich** mit A/B-Umschaltung pro Abschnitt, auch über mehrere Spuren (Band-Comping).
4. **Kommentare und Marker mit Text/Audio-Notizen** für Zusammenarbeit zwischen Musiker und Produzent.
5. **Projekt-Austausch als offenes Format** (Edit-XML + Audio), lesbar ohne die App.
6. **Lernmodus**: Tooltips, die erklären, was ein Kompressor-Parameter tut, mit Beispiel. Passt zum Lernprojekt-Charakter.

## 19. Bewusst nicht

- Notensatz auf Druckniveau (Stichbild, Layout, Stimmenauszüge: Dorico-Territorium). Ein Noten-Editor zum Bearbeiten ist seit 2026-10-09 geplant (Abschnitt 5a)
- Video-Spur (vielleicht P3, zunächst nicht)
- VST2, AU, AAX
- macOS (technisch über JUCE möglich, aber Test-Hardware und Signierung fehlen; später entscheiden)
- Dolby Atmos / immersiver Mix
- ARA (Melodyne-Integration)
- Cloud-Kollaboration in Echtzeit
