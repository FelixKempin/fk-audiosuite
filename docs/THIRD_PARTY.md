# Fremdbibliotheken

Jede Bibliothek mit Version, Lizenz und Grund. Pflicht laut ADR 0003. Die Spalte "Produkte" sagt, ob die Bibliothek in den geschlossenen Produkt-Plugins (ADR 0008, nicht in diesem Repo) eingelinkt werden darf: nur BSD, MIT, ISC, OFL, BSL oder kommerziell lizenzierte Bibliotheken; GPL-Code nie.

| Bibliothek | Version / Commit | Lizenz | Verwendung | Produkte | Seit |
|---|---|---|---|---|---|
| JUCE | 8.0.13 (Submodul in Tracktion Engine) | GPLv3 / kommerziell | GUI, Audio-/MIDI-Geräte, Plugin-Hosting | ja, unter JUCE Starter/Indie | M0 |
| Tracktion Engine | develop, e760754 (2026-10-08) | GPLv3 / kommerziell | DAW-Modell und Audio-Graph | **nein** | M1 |
| Catch2 | v3.16.1 | BSL-1.0 | Tests | ja (nur Tests) | M0 |
| Signalsmith Stretch (in Tracktion gebündelt, `3rd_party/signalsmith-stretch`) | mit Tracktion e760754 | MIT | Time-Stretch und Pitch-Shift von Audio-Clips (A5) | ja | A5 |
| SoundTouch (in Tracktion gebündelt) | mit Tracktion | LGPL-2.1 | Tempo-Erkennung (`TempoDetect`), einfacher Time-Stretch | nein (LGPL, nur DAW) | M5 |
| Salamander Grand Piano (SFZ von sfzinstruments, **nicht** mitgeliefert) | master-Zweig auf GitHub | CC BY 3.0, Alexander Holm | Content-Manager lädt es auf Wunsch herunter (B5a); Namensnennung im Content-Manager | nein (Content) | B5 |
| VSCO 2 Community Edition (**nicht** mitgeliefert) | Release 1.1.0 auf GitHub | CC0, Versilian Studios | Content-Manager lädt es auf Wunsch herunter (B5a) | nein (Content) | B5 |
| libmp3lame (LAME, System-Bibliothek, **nicht** mitgeliefert) | wie installiert (`libmp3lame.so.0`) | LGPL-2.0 | MP3-Export (A7); Tracktion lädt sie zur Laufzeit per `dlopen`, fehlt sie, ist MP3 im Export-Dialog aus | nein (nur DAW) | A7 |
| ASIO-Header von Steinberg (in JUCE gebündelt, `juce_audio_devices/native/asio`) | mit JUCE 8.0.13 | GPLv3 / Steinberg-ASIO-Lizenz | ASIO-Treiber unter Windows (DAW, Standalone der Produkte) | ja, unter der Steinberg-ASIO-Lizenz: vor der ersten Veröffentlichung mit ASIO die unterschriebene Lizenzvereinbarung von Steinberg einholen (steinberg.net/en/company/developers.html). In fk-audio-plugins schaltet erst `FK_ASIO=ON` (`package-windows.ps1 -Asio`) ASIO ein; das About-Fenster jedes Produkts und der DAW zeigt dann den Markenhinweis aus § 3, Produktseiten und Impressum brauchen ihn ebenfalls, dazu das Logo „ASIO compatible“ | 2026-10-25 |
| libvorbis/libogg (in JUCE gebündelt) | mit JUCE 8.0.13 | BSD-3 | OGG-Vorbis-Export und -Import | ja | A7 |
| sfizz | 1.2.3 (Submodul `external/sfizz` mit eigenen Submodulen: abseil, simde, st_audiofile u. a.) | BSD-2 | SFZ-Sampler (FK Sampler) | ja | M5 |

Werkzeuge für die Auslieferung (nicht im Programm, nicht im Git; `dist/README.md`): linuxdeploy (MIT, packt das AppImage, lädt `dist/package-linux.sh` nach `dist/tools/`), AppImage-Laufzeit (MIT, steckt in jedem AppImage), Inno Setup 6 (eigene freie Lizenz, baut den Windows-Installer), rclone (MIT, Upload).

Geplant: RTNeural (BSD-3, Strang L, Produkte ja), Inter-Schrift (OFL, ja), Lucide-Icons (ISC, ja), VST3-SDK über JUCE (MIT seit VST 3.8, ja). Gebündelte GPL-Plugins (Surge XT, Vitalium, Dexed, LSP, Airwindows ist MIT) nur als separate Programme, nie eingelinkt.

Trainings- und Messdaten (keine Bibliotheken, aber lizenzpflichtig): VSCO 2 Community Edition (CC0), NSynth (CC BY 4.0), Salamander Grand Piano (CC BY 3.0), OpenAIR-Impulsantworten (CC BY). Ausgeschlossen: Good-Sounds (CC BY-NC), URMP, University-of-Iowa-Samples (keine Lizenz).
