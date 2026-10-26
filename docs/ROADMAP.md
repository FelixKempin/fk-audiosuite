# Roadmap

Neu aufgestellt am 2026-10-12 nach einer Bestandsaufnahme gegen Cubase und Logic. Ziel ist eine vollwertige, professionelle DAW. Jeder Meilenstein liefert etwas Lauffähiges und führt genau die C++-Themen ein, die er braucht. Zeitangaben sind grobe Schätzungen für eine Person neben dem Beruf.

## Stand 2026-10-12

Ungefähr die Hälfte von Release 0.1 ist erreicht. Das Fundament steht, aber es fehlen Grundfunktionen, ohne die niemand ein echtes Projekt beginnt (Autosave, Crossfades, Time-Stretch, Hall/Delay, MP3/Stems), und Windows.

| Bereich | Vorhanden |
|---|---|
| Projekt/Transport | Anlegen, Öffnen, Speichern, Undo/Redo, Loop, Metronom, Vorzähler, Signaturspur (Tempo mit Rampen, Taktart, Tonart) |
| Aufnahme | Mehrkanal, Mono/Stereo, Monitoring, Takes, Comping v1 |
| Audio-Editing | Verschieben, Schneiden, Trimmen, Mehrfachauswahl, Zwischenablage, Kleben, Fades, Clip-Gain, Import per Drag-and-drop |
| MIDI | Instrumentenspuren, Aufnahme, Key Editor, Velocity- und Controller-Lanes, Akkorde nach Stufen, Quantisieren, Vorhören, MIDI-Import |
| Mixer | MixConsole, Inspector, 8 Inserts, verschachtelte Gruppen, FX-Spuren mit Sends Pre/Post, Gain-Reduction-Anzeige |
| Automation | Mehrere Lanes je Spur, Plugin-Parameter, Read/Touch/Latch/Write |
| Plugins | VST3/LV2-Scan im Unterprozess, PDC, Plugin-Manager, Track Bake |
| Eigene Plugins | Gain, EQ, Compressor, Limiter, Distortion, FK Synth, FK Sampler; FK Rig (privates Repo) |
| Controller | MIDI-Hot-Plug, MIDI Learn mit Automation-Aufnahme, Bildschirm-Keyboard |
| Export | WAV/FLAC, ganzes Projekt oder Loop, 16/24/32 Bit |
| Robustheit | Hänger-Wächter, DSP-Lastanzeige |

### Abgeschlossene Meilensteine (bis 2026-10-11)

| # | Meilenstein | Ergebnis |
|---|---|---|
| M0 | Setup und Hello Audio | CMake, Fenster, Sinus über PipeWire, erster Test |
| M1 | Audiospur | Projekt, Audiospur, Aufnahme, Play/Stop, Waveform, Undo |
| M2 | Mehrkanal und Transport | Mehrspuraufnahme, Metronom, Loop, Lineal, Zoom, Snap, Werkzeuge, Takes, Comping v1, Clip-Bearbeitung |
| M3 | Eigene Effekte | Gain, EQ, Compressor, Limiter, Distortion, Insert-Slots, MixConsole |
| M4 | Fremde Plugins | VST3/LV2-Scan im Unterprozess, Plugin-Manager, Plugin-Fenster, PDC |
| M5 | MIDI und Instrumente (Teil) | FK Synth, FK Sampler, Key Editor, Velocity- und Controller-Lanes |
| M6 | Automation und Mixer (Teil) | Inspector, Gruppen, Sends/FX-Spuren, Automation, Export WAV/FLAC, Track Bake |
| M6b | Signaturspur | Tempo, Taktart, Tonart, Akkorde nach Stufen |
| M7 | Content (begonnen) | Medien-Browser mit Vorhören und Drag-and-drop |
| M8 | Controller (Teil) | MIDI-Geräte mit Hot-Plug, MIDI Learn |

Offene Reste dieser Meilensteine sind unten in die neuen Phasen einsortiert.

## Phase A: Release 0.1, intern nutzbar

Ziel: Ein Song lässt sich von der Aufnahme bis zum MP3 fertig produzieren, ohne Daten zu verlieren und ohne an Grundfunktionen zu scheitern.

| # | Meilenstein | Ergebnis | Neue C++-Themen (→ LEARNING.md) | Dauer |
|---|---|---|---|---|
| **A1** | Sicherheit und Projekt | Autosave mit Backup-Rotation, Wiederherstellung nach Absturz, Speichern unter, zuletzt geöffnete Projekte, Projekteinstellungen (Samplerate, Bittiefe, Aufnahmeformat), Projektvorlagen, Log-Datei mit Diagnose-Export. **Fertig 2026-10-12** (A1a: Autosave, Backups, Wiederherstellung, Nachfrage bei ungespeicherten Änderungen, Speichern unter, zuletzt geöffnet; A1b: Projekteinstellungen, Vorlagen, Log-Datei, Absturzbericht, Diagnose-Export, Autosave-Intervall). Offen: Mehrspuraufnahme mit dem gewählten Format am Interface prüfen. | Dateisystem-Atomarität (schreiben, dann umbenennen), Timer außerhalb des Audio-Threads | 1–2 Wochen |
| **A2** | Spuren-Grundlagen | Spuren per Ziehen umsortieren, Spurfarbe, Spur duplizieren, Ordnerspur, Spuren ausblenden, Höhe für alle Spuren, Markerspur mit Sprüngen. **A2a fertig 2026-10-12** (Umsortieren, Farbe, Duplizieren, Ausblenden, Löschen; Höhe für alle gab es schon). **Fertig 2026-10-12** (A2a Spuren organisieren, A2b Markerspur, A2c Ordnerspuren). | ValueTree-Reihenfolge, Drag-and-drop zwischen Komponenten | 2 Wochen |
| **A3** | Transport komplett | Punch In/Out, Pre-/Post-Roll, Locatoren, Vor-/Rücklauf, Positionsanzeige in Timecode und Samples, Aufnahmemodi (neuer Take, ersetzen, MIDI mischen), Mehrspuraufnahme am Interface getestet. **Fertig 2026-10-12** (Hardware-Test steht im Testprotokoll). | Zustandsmaschinen für Transport | 1–2 Wochen |
| **A4** | Audio-Editing Profi-Grundlage | Automatische Crossfades (auch beim Comping), Bereichswerkzeug über mehrere Spuren, Lasso im Arrangement, Werkzeuge Mute/Zoom/Kleber, Nudge per Taste, Raster mit Triolen, punktiert, Sekunden und Samples, Clip-Bearbeitung Normalisieren, Umkehren, Stille, Phase. **Fertig 2026-10-13** (A4a Raster und Werkzeuge, A4b Crossfades und Clip-Bearbeitung, A4c Lasso und Bereichswerkzeug). | Werttypen für Bereiche, Befehlsobjekte | 3 Wochen |
| **A5** | Time-Stretch und Sample-Editor | Time-Stretch und Pitch-Shift über RubberBand/SoundTouch aus Tracktion, Clip an Projekttempo anpassen, Sample-Editor in der unteren Zone (Bereich wählen, Bearbeitungen, Zoom bis Sample). **Fertig 2026-10-13** (Signalsmith Stretch, eigene Tempo-Erkennung, Musikmodus, Tonhöhe, Strecken mit Alt+Ziehen, Sample-Editor mit sieben Bearbeitungen). | Offline-Verarbeitung in Hintergrund-Threads, Caches | 3 Wochen |
| **A6** | Mixer und Effekte | Sidechain-Routing, Ein-/Ausgangs-Busse (wie Studio-Verbindungen), Eingangs-Trim und Phase im Kanal; neue Plugins FK Reverb (algorithmisch), FK Delay (Stereo, Ping-Pong, Tempo-Sync), FK Gate/Expander; eigene Oberflächen für EQ, Compressor, Limiter, Distortion aus dem UI-Kit. **Fertig 2026-10-13** (A6a Reverb, Delay, Gate; A6b Kennlinien; A6c Sidechain, Trim und Phase, Direct Outs und Studio-Verbindungen). | Feedback-Netzwerke (FDN), Tempo-Sync, Sidechain-Busse | 4 Wochen |
| **A7** | Export komplett | MP3/OGG, Stem-Export je Spur und Bus, Dither, Samplerate-Wandlung, MIDI-Export (SMF), Tempo beim MIDI-Import übernehmen. **Fertig 2026-10-13** (dazu AIFF, Lautheits-Normalisierung in LUFS, Ausklingen; MP3 über die System-Bibliothek libmp3lame). | Externe Bibliotheken anbinden (LAME, libvorbis), Lizenzen prüfen | 2 Wochen |
| **A8** | Bedienung | Tastaturkürzel-Editor mit Cubase-Vorlage, Undo-Verlauf als Liste, Übersichtsleiste, Zoom-Presets und Zoom auf Auswahl, Plugin-Manager mit Favoriten, Ausblenden und Kategorien, Presets für fremde Plugins. **Fertig 2026-10-13** (A8a Verlauf, Zoom, Übersichtsleiste; A8b Tastaturbefehle; A8c Plugin-Manager und Presets). Statt `ApplicationCommandManager` eine eigene, schlanke Befehlstabelle. | Command-Pattern, `juce::ApplicationCommandManager` | 2–3 Wochen |

## Phase B: Profi-Instrumente und MIDI

Ziel: Große Libraries (Superior Drummer, Kontakt, Opus) und Orchester-Arbeit funktionieren wie in Cubase.

| # | Meilenstein | Ergebnis | Neue C++-Themen (→ LEARNING.md) | Dauer |
|---|---|---|---|---|
| **B1** | Rack-Instrumente und Multi-Output | Instrumenten-Rack, MIDI-Spuren auf Kanal 1–16 einer Instanz, Ausgänge aktivieren wie in Cubase, jeder Ausgang ein Mixer-Kanal, MIDI-Spur an externes Hardware-Gerät mit Programmwechsel, Sidechain-Eingänge an fremden Plugins. **Fertig 2026-10-13** (B1a Rack-Instrumente und Multi-Output, B1b MIDI-Spuren an Geräten; Sidechain seit A6). | Tracktion RackType, Graphen mit mehreren Bussen | 4 Wochen |
| **B2** | Drum-Editor und FK Drums | Drum-Editor mit Drum-Maps (Name, Note, Raute statt Balken), FK Drums als Instrument (eigenes Plugin), Sampler-Spur für eigene Samples per Drag-and-drop. **Fertig 2026-10-13** (DAW-Seite: Drum-Maps, Drum-Editor, Sampler-Spur, Drum-Map für FK Drums mit allen Schlagzonen; das Plugin FK Drums selbst entsteht außerhalb dieses Repos). | Datengetriebene Maps (JSON), Modalsynthese | 4–6 Wochen |
| **B3** | MIDI-Werkzeuge | Listen-Editor, Längen, Legato, Transponieren und Humanize als Funktionen, MIDI-Inserts (Arpeggiator, Transpose, Chord), Step-Eingabe, Groove-Quantisierung, retrospektive Aufnahme. **Fertig 2026-10-13** (B3a Funktionen und Listen-Editor, B3b MIDI-Inserts, B3c Step-Eingabe, Groove, Retrospektive Aufnahme). | MIDI-Verarbeitung im Audio-Thread, Ringpuffer | 3–4 Wochen |
| **B4** | Expression Maps und Spielweisen | Spielweise als Noteneigenschaft, Artikulations-Lane im Key Editor, Expression Maps (Keyswitch/Kanal/CC beim Abspielen, Nachsuchen beim Springen), Import von Spielweisen-Listen. **Fertig 2026-10-13** (Umsetzung je Note in FkEngineBehaviour::createPlaybackMidiSequence; offen: Anweisungen als Bereiche auf der Zeitachse, Keyswitches beim Einspielen umwandeln, P2) | Abbildungen als Daten, Chasing | 3–4 Wochen |
| **B5** | Content und Browser | Instrumenten- und Preset-Browser mit Tags und Favoriten, Content-Manager mit Download, freie Bibliotheken (Salamander Piano, VSCO 2) als Pakete. **Fertig 2026-10-14** (B5a Content-Manager, B5b Browser; Index im Speicher statt SQLite, reicht für einige tausend Einträge) | SQLite-Index, Hintergrund-Downloads | 4 Wochen |

## Phase C: Windows und Auslieferung

Ziel: Die DAW läuft auf Windows und Linux und lässt sich installieren. Phase C beginnt spätestens nach Phase A, damit sich keine Linux-Annahmen festsetzen.

| # | Meilenstein | Ergebnis | Neue C++-Themen (→ LEARNING.md) | Dauer |
|---|---|---|---|---|
| **C1** | Windows-Build | MSVC-Build, WASAPI, ASIO (Header aus JUCE, ADR 0003), alle Tests grün auf Windows. **Vorbereitet 2026-10-14** (Presets, MSVC-Optionen, ASIO-Schalter; offen: erster Build auf Windows bzw. in der CI, Fehler dort beheben) | Plattform-Abstraktion, Präprozessor sparsam | 2–3 Wochen |
| **C2** | CI und Pakete | CI auf beiden Plattformen, AppImage oder Flatpak, Windows-Installer, Update-Hinweis. **Gebaut 2026-10-14** (GitHub Actions, AppImage, Inno Setup + Zip, rclone-Upload in Bucket, Update-Hinweis; offen: Bucket und Domain festlegen, echtes Logo, Signierung der Windows-Dateien) | Build-Pipelines, Signierung | 2 Wochen |
| **C3** | Release 0.5 (öffentliche Beta) | Lokalisierung DE/EN über Übersetzungstabelle, Onboarding (Audio einrichten, Content laden), Handbuch, Website. **Release Candidate 0.5.0 beta gebaut 2026-10-19** (FK Rig Lite mitgeliefert, Pro-Kauf über Paddle und fk-licensing, Hilfe-Menü zu Kauf/Aktivierung/Download, Update-Hinweis über fk-licensing, Pipeline je Plattform in einem Befehl, Website mit Download-, Preis- und Aktivierungsseite). Offen: Lokalisierung EN, Onboarding, Handbuch, erster Windows-Lauf, Bucket und Paddle-Konto | Übersetzungen, Ressourcen | 4 Wochen |

## Phase D: Cubase/Logic-Niveau (Release 1.0)

| # | Meilenstein | Ergebnis | Neue C++-Themen (→ LEARNING.md) | Dauer |
|---|---|---|---|---|
| **D1** | Arrangieren | Arranger-Spur (Abschnitte umordnen und wiederholen), Spurvorlagen, Track Versions, Audio-Pool mit Aufräumen, Projekt archivieren | Nicht-lineare Wiedergabe-Ketten | 3 Wochen |
| **D2** | Audio-Quantisierung | Tempo-Erkennung, Warp-Marker, Audio-Quantisierung, Groove aus Audio, Render in Place | Onset-Erkennung, Transienten-Analyse | 4 Wochen |
| **D3** | Mixer-Ausbau | VCA-Fader, Kanal-EQ und Channel Strip im Kanalzug, Meter für LUFS, True Peak und Korrelation, Mixer-Ansichten (Breite, Sichtbarkeit, Verknüpfen), Export auf Loudness-Ziel | ITU-R BS.1770, SIMD | 4 Wochen |
| **D4** | Effekt-Sammlung | Chorus, Flanger, Phaser, Stereo-Tool, De-Esser, Multiband-Compressor, Analyzer, Tuner | Modulierte Delays, Linkwitz-Riley-Weichen | 4–6 Wochen |
| **D5** | Controller und Fenster | Mackie Control, generische Controller-Zuordnung, HUI, OSC; Mixer und Editoren abdockbar, Multi-Monitor, Light Theme, anpassbare Werkzeugleiste, Suche über alles | Protokoll-Parsing, Fensterverwaltung | 4 Wochen |
| **D6** | Release 1.0 | Stabilisierung, Performance (Multicore-Last, große Projekte), Plugin-Kompatibilitätsliste, Dokumentation | Profiling, Sanitizer-Läufe | 4 Wochen |

## Phase E: Alleinstellungsmerkmale (nach 1.0)

Wird mit Nutzer-Feedback priorisiert. Kandidaten:

- Tonhöhenkorrektur von Gesang (wie VariAudio oder Flex Pitch), Audio zu MIDI
- Akkordspur mit Akkordassistent, Noten-Editor, MusicXML
- Projekt-Versionierung wie Git, Takes-Vergleich über mehrere Spuren (FEATURES §18)
- Record-Modus für Musiker, Kopfhörermixe, Control Room
- Stem-Trennung, Mastering-Assistent
- Plugin-Sandbox, CLAP (sobald JUCE/Tracktion es hosten), MPE
- Neu prüfen: Video-Spur und ARA (Melodyne), beide bisher bewusst ausgeschlossen (FEATURES §19)

## Eigene Plugins außerhalb dieses Repos

Die FK-Plugins (z. B. FK Rig) entstehen in einem eigenen, nicht öffentlichen Repo. Die DAW lädt sie wie jedes VST3 und liefert ihre Lite-Ausgaben als separate Dateien mit.

## Definition of Done (für jeden Meilenstein)

- Baut mit `-Wall -Wextra -Werror`, clang-format sauber, clang-tidy ohne neue Warnungen.
- Tests für alles in `dsp` und `services` grün, und der Testlauf meldet keine einzige "JUCE Assertion failure".
- Keine Verstöße gegen die Echtzeit-Regeln (manuelles Review + Sanitizer-Lauf).
- Jede neue Funktion hat einen sichtbaren Button mit Icon und Tooltip, nicht nur ein Tastenkürzel.
- LEARNING.md um die neuen Themen ergänzt, mit Links auf konkreten Code.
- Ein kurzer Eintrag im CHANGELOG.
- Manuelle Prüfschritte (Bedienung, Dialoge, Hardware, Hören) stehen in [TESTPROTOKOLL.md](TESTPROTOKOLL.md); abgearbeitet am Ende einer Phase.
