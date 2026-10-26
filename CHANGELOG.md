# Changelog

## 0.7.0 beta (unveröffentlicht)

Erste Version mit öffentlichem Quellcode (GPLv3). Frühere Betas (0.5, 0.6) waren nur als Programm
verteilt und sind zurückgezogen; dieser Stand ersetzt sie.

Was die DAW kann:

- **Projekt und Transport:** Anlegen, Öffnen, Speichern, automatische Sicherung, Vorlagen, Undo/Redo
  mit Verlauf, Loop, Punch, Pre- und Post-Roll, Metronom, Vorzähler, Signaturspur (Tempo mit Rampen,
  Taktart, Tonart), Marker.
- **Spuren:** Audio-, Instrumenten-, MIDI-, Gruppen-, FX- und Ordnerspuren, Racks mit Multi-Output,
  Track Bake.
- **Aufnahme:** Mehrkanal, Mono/Stereo, Monitoring, Takes und Comping, retrospektive MIDI-Aufnahme.
- **Audio-Editing:** Werkzeuge mit Raster, Crossfades, Clip-Gain, Lasso und Bereichswerkzeug,
  Time-Stretch und Pitch-Shift, Sample-Editor, Import per Drag-and-drop (WAV, FLAC, OGG, MP3).
- **MIDI:** Key-, Drum- und Listen-Editor, Controller-Lanes, Akkorde, Quantisieren mit Groove,
  Step-Eingabe, MIDI-Inserts, Spielweisen und Expression Maps, MIDI-Import und -Export.
- **Mixer und Automation:** MixConsole, Inspector, Inserts, Sends, Sidechain, Direct Outs,
  Automation mit Read/Touch/Latch/Write, MIDI Learn.
- **Plugins:** VST3- und LV2-Scan im eigenen Prozess, Latenzausgleich, Plugin-Manager; eigene
  Effekte und Instrumente (Gain, Trim, EQ, Compressor, Limiter, Gate, Reverb, Delay, Distortion,
  FK Synth, FK Sampler mit SFZ).
- **Export:** Mixdown und Stems als WAV, FLAC, OGG und MP3; MIDI.
- **Content und Browser:** Content-Manager für freie Bibliotheken, Instrumenten- und Medien-Browser.
- **Sprache:** Englisch und Deutsch, Wahl gilt für alle FK-Programme.
- **Plattformen:** Linux (AppImage, ab glibc 2.35) und Windows (Installer und Zip; WASAPI,
  DirectSound und ASIO).
- **Mitgeliefert:** FK Rig Lite als separates VST3-Plugin (nicht Teil dieses Quellcodes).

Neu gegenüber 0.6:

- **ASIO unter Windows** in jedem Build; die ASIO-Header kommen mit JUCE (GPLv3), der Schalter
  `FK_ASIO_SDK_DIR` entfällt.
- **Quellcode aufgeteilt:** Dieses Repo enthält nur noch, was die DAW selbst braucht.
  `dist/check-open-source.sh` prüft das in der CI und vor jedem Release.
- **Mitgelieferte Plugins als fertige Release-Pakete:** `dist/bundled-plugins.lock` nennt Paket und
  SHA-256, `dist/fetch-plugins.sh`/`.ps1` laden und prüfen sie. Der Release-Workflow braucht damit
  keinen Zugriff auf ein privates Repo mehr.
