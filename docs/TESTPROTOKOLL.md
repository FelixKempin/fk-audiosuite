# Testprotokoll (manuell)

Alles, was die automatischen Tests nicht abdecken: Bedienung, Dialoge, echte Hardware, Hören. Nach jedem Meilenstein kommen hier die Schritte dazu; durchgearbeitet wird am Ende einer Phase in einem Rutsch.

So wird es benutzt:
- `[ ]` offen, `[x]` bestanden, `[!]` Fehler. Bei Fehler eine Zeile darunter: was passiert ist.
- Debug-Build starten: `"build/linux-debug/src/app/FKAudioSuite_artefacts/Debug/FK Audio Suite"`.
- Geht etwas schief: Hilfe → „Diagnose-Bericht exportieren…“ und die ZIP-Datei zum Fehler legen.

Automatisch geprüft (hier nicht nötig): Service-Logik, Speichern und Laden, Undo der Service-Aktionen, Layout der Werkzeugleiste bei 1024/1280/1920 px.

---

## Vorbedingung: Testprojekt

- [ ] Neues Projekt „Testlauf“ anlegen, zwei Audiospuren (eine Mono, eine Stereo), eine Instrumentenspur mit FK Synth, eine kurze Audiodatei auf Spur 1 ziehen.

---

## Offen aus früheren Meilensteinen

### M1/M2 Aufnahme am Interface
- [ ] Eine Spur scharf schalten, Eingang wählen, aufnehmen: Pegel bewegt sich, Clip erscheint, Datei liegt in `Audio/`.
- [ ] Mehrspuraufnahme: vier Spuren mit vier verschiedenen Eingängen gleichzeitig aufnehmen. Alle vier Clips starten an derselben Stelle und enthalten jeweils das richtige Signal.
- [ ] Loop-Aufnahme über drei Durchgänge: Takes im Clip-Menü wählbar.

---

## A1 Sicherheit und Projekt

### Ungespeicherte Änderungen
- [ ] Etwas ändern, Fenster schließen: Frage „Speichern / Nicht speichern / Abbrechen“. Abbrechen lässt die App offen.
- [ ] Dasselbe über „Neu“, „Öffnen“ und Datei → Beenden.
- [ ] Ohne Änderungen schließen: keine Frage.

### Automatische Sicherung und Absturz
- [ ] Projekteinstellungen → „Autom. sichern“ auf 1 min. Etwas ändern, 1 min warten: `Backups/<Name>.autosave.tracktionedit` existiert.
- [ ] App hart beenden (`pkill -9 "FK Audio Suite"`), neu starten, Projekt öffnen: Dialog „Wiederherstellen?“. „Sicherung wiederherstellen“ bringt die Änderung zurück.
- [ ] Dasselbe noch einmal, diesmal „Gespeicherte Fassung öffnen“: alte Fassung, und die Sicherung liegt als Backup in `Backups/`.
- [ ] Nach dem Test das Intervall wieder auf 5 min stellen.

### Backups
- [ ] Dreimal mit Änderungen speichern: drei Dateien mit Zeitstempel in `Backups/`.
- [ ] Uhr-Button → „Backup wiederherstellen…“ → ein älteres wählen: Stand von damals ist da.

### Speichern unter und zuletzt geöffnet
- [ ] Strg+Shift+S (oder Button Diskette mit Plus): neuer Ordner mit kopiertem `Audio/`. Clips spielen im neuen Projekt. Den alten Ordner umbenennen: Die Kopie spielt weiter.
- [ ] Strg+S speichert.
- [ ] Uhr-Button und Datei → „Zuletzt geöffnet“ zeigen die letzten Projekte; ein Klick öffnet.

### Projekteinstellungen (Regler-Button, Shift+S)
- [ ] Format FLAC, 24 Bit, etwas aufnehmen: In `Audio/` liegt eine `.flac`-Datei.
- [ ] Format WAV, 32 Bit Float: Aufnahme ist eine 32-Bit-Float-WAV (z. B. `soxi` oder Dateimanager-Eigenschaften).
- [ ] Projekt-Samplerate auf einen anderen Wert als das Gerät stellen, Projekt schließen und öffnen: Frage „Gerät umstellen?“. Unter PipeWire erscheint die Erklärung mit `pw-metadata`.

### Vorlagen
- [ ] „Neu“ → „Aktuelles Projekt als Vorlage speichern…“ → Name „Band“.
- [ ] „Neu“ → „Band“: neues Projekt mit denselben Spuren, Plugins und Routing, ohne Audio-Clips.
- [ ] Eine gebackene Spur im Projekt: Vorlage speichern wird mit Erklärung abgelehnt.
- [ ] „Vorlagen-Ordner öffnen“ öffnet den Dateimanager.

### Log und Diagnose
- [ ] Hilfe → „Log-Ordner anzeigen“: Pro Start eine Log-Datei.
- [ ] Hilfe → „Diagnose-Bericht exportieren…“: Die ZIP enthält `System.txt` mit Audiogerät und Projekt sowie die Logs.

---

## A2 Spuren-Grundlagen

### Spuren organisieren
- [ ] Spur am Namen nach oben und unten ziehen: Linie zeigt die Stelle, die Spur landet dort, Strg+Z macht es rückgängig.
- [ ] Rechtsklick auf Spurkopf → Farbe: Spurkopf, Clips und Kanalzug in der MixConsole wechseln die Farbe.
- [ ] Duplizieren: Kopie direkt darunter mit Plugins und Clips. Eine gebackene Spur duplizieren wird abgelehnt.
- [ ] Ausblenden: Die Spur verschwindet aus Arrangement und MixConsole und ist beim Abspielen weiter zu hören. Auge-Button → Haken setzen holt sie zurück. „Alle einblenden“.
- [ ] Rechtsklick → „Spur löschen“, dann Strg+Z.
- [ ] Spur-Menü: duplizieren, ausblenden, löschen wirken auf die ausgewählte Spur.

### Marker
- [ ] Einfg setzt einen Marker am Positionszeiger; Plus-Button in der Markerzeile und Fahnen-Button in der Transportleiste ebenso.
- [ ] Doppelklick in die leere Markerzeile setzt einen Marker am Raster.
- [ ] Marker ziehen (mit und ohne Shift), Doppelklick benennt um.
- [ ] Klick auf einen Marker springt dorthin. Shift+N / Shift+B und die Pfeil-Fahnen-Buttons springen vor und zurück, auch während der Wiedergabe.
- [ ] Rechtsklick → „Loop auf diesen Abschnitt“: Loop geht bis zum nächsten Marker.
- [ ] Tempo ändern: Marker bleiben auf ihrem Takt.

### Ordnerspuren
- [ ] Button „Ordner“ legt einen Ordner an.
- [ ] Spur auf die Mitte des Ordnerkopfs ziehen: Rahmen um den Ordner, Spur landet darin (eingerückt).
- [ ] Rechtsklick Spur → „In Ordner verschieben“, „Neuer Ordner mit dieser Spur“, „Aus Ordner nehmen“.
- [ ] Ordner zuklappen (Dreieck oder Doppelklick auf die freie Fläche): Inhalt verschwindet, die Ordnerzeile zeigt die Clips weiter. Speichern und öffnen: bleibt zugeklappt.
- [ ] M und S am Ordner schalten alle Spuren darin stumm bzw. solo (hörbar).
- [ ] Ordner in Ordner ziehen (verschachteln); einen Ordner in sich selbst ziehen wird abgelehnt.
- [ ] Ordner ziehen: Inhalt wandert mit.
- [ ] Rechtsklick Ordner → „auflösen“ (Spuren bleiben) und „mit allen Spuren löschen“, jeweils mit Strg+Z.

### Werkzeugleiste
- [ ] Fenster schmal ziehen: erst verschwinden Beschriftungen, dann zweite Reihe. Kein Button wird abgeschnitten. Tooltips nennen die Funktion.

---

## A3 Transport

### Punch, Pre- und Post-Roll
- [ ] Locatoren auf Takt 3 und 5 setzen (Loop-Bereich im Lineal ziehen). Punch-Button (oder I) an: Loop geht aus.
- [ ] Pre auf 2 s, Post auf 1 s. Spur scharf, Positionszeiger weit hinten, Record: Wiedergabe startet 2 s vor Takt 3, Aufnahme beginnt genau bei Takt 3, endet bei Takt 5, Stop 1 s danach.
- [ ] Der aufgenommene Clip liegt genau zwischen den Locatoren.
- [ ] Post auf „aus“: Nach Takt 5 läuft die Wiedergabe weiter, die Aufnahme endet trotzdem bei Takt 5.
- [ ] Loop einschalten, während Punch an ist: Punch geht aus.

### Locatoren und Navigation
- [ ] Klick auf „L“ / „R“ springt hin; Rechtsklick → „Auf Positionszeiger setzen“.
- [ ] Num 1 / Num 2 springen, Strg+Num 1 / Strg+Num 2 setzen. Num 1 schaltet dabei nicht das Werkzeug um.
- [ ] Rück-/Vorlauf-Button: ein Takt pro Klick, gedrückt halten läuft weiter. Num − / Num + ebenso, auch während der Wiedergabe.
- [ ] Menü Transport: Jeder Eintrag tut, was er sagt.

### Zeitanzeige
- [ ] Klick auf die Zeitanzeige: Zeit, Timecode, Samples. Bei Timecode mit 25 fps zählen die Frames 00–24.
- [ ] Projekteinstellungen → Timecode 30 fps: Frames zählen 00–29. Speichern, öffnen: Format bleibt.
- [ ] Samples stimmen mit der Rate des Audiogeräts (z. B. nach 1 s genau 48 000 bei 48 kHz).

### Aufnahmemodi (Projekteinstellungen)
- [ ] Audio „Takes behalten“: zweimal über dieselbe Stelle aufnehmen, beide Aufnahmen sind als Takes wählbar.
- [ ] Audio „Ersetzen“: Die zweite Aufnahme ersetzt die erste im Bereich.
- [ ] MIDI „Mischen“: zweimal über denselben Clip einspielen, alle Noten in einem Clip.
- [ ] MIDI „Neuer Clip“: zwei Clips. MIDI „Ersetzen“: nur die Noten der zweiten Aufnahme.

### Transportleiste
- [ ] Fenster schmal ziehen: Die Leiste bricht gruppenweise in eine zweite Reihe um, nichts wird abgeschnitten.

---

## A4 Audio-Editing

### Raster und Werkzeuge (A4a)
- [ ] Rasterwahl öffnen: Takt, Zählzeit, Notenwerte mit „.“ und „T“, Sekunden, Samples. 1/8T: Clips rasten in Triolen-Abständen ein (drei pro Viertel).
- [ ] Bei 1/128 weit herauszoomen: keine schwarze Linienfläche, Taktlinien bleiben.
- [ ] Sekunden-Raster: Clip-Anfänge rasten auf volle Sekunden; das Lineal zeigt weiter Takte.
- [ ] Kleber (4): Klick auf einen Clip verbindet ihn mit dem nächsten rechts daneben.
- [ ] Lupe (6): Klick vergrößert um die Klickstelle, Alt+Klick verkleinert.
- [ ] Mute (7): Klick macht den Clip grau und stumm (hörbar prüfen), zweiter Klick hörbar.
- [ ] Shift+M auf eine Auswahl aus stummen und hörbaren Clips: erst alle stumm, dann alle hörbar.
- [ ] Nudge-Buttons und Strg+Links/Rechts: Auswahl wandert um einen Rasterschritt, Strg+Z nimmt es zurück.

### Crossfades und Clip-Bearbeitung (A4b)
- [ ] Zwei Audio-Clips auf einer Spur überlappen lassen: Im Überlappungsbereich zeigen beide eine Rampe, beim Abhören kein harter Schnitt.
- [ ] Projekteinstellungen → „Automatische Crossfades“ aus: Rampen verschwinden; wieder an: sind zurück.
- [ ] Comping: in einer Take-Lane einen Bereich ziehen. An beiden Schnittstellen ist kein Knacks zu hören.
- [ ] Rechtsklick → „Normalisieren“ auf einen leisen Clip: Gain-Anzeige im Clip (z. B. „+8.3 dB“), hörbar lauter, Spitze knapp unter 0 dBFS am Master-Meter.
- [ ] „Umkehren“: Clip spielt rückwärts; nochmal: vorwärts. Nach Speichern und Öffnen bleibt der Zustand.
- [ ] „Phase umkehren“: neue Datei „… Phase.wav“ in `Audio/`. Mit Original auf zweiter Spur zusammen abspielen: Auslöschung (fast still). Strg+Z: wieder die alte Datei.
- [ ] Bearbeiten → Audio: dieselben drei Einträge, nur aktiv mit ausgewählten Clips.

### Lasso und Bereichswerkzeug (A4c)
- [ ] Auswahlwerkzeug, auf leerer Fläche ein Rechteck über zwei Spuren ziehen: berührte Clips sind ausgewählt. Mit Strg dazu, ohne Strg ersetzt.
- [ ] Taste 2 wählt das Bereichswerkzeug (Taste 8 den Stift). Bereich über drei Spuren aufziehen: halbtransparente Fläche nur auf diesen Spuren, Kanten am Raster; mit Shift frei.
- [ ] Entf: Material im Bereich ist weg, Lücke bleibt; Strg+Z.
- [ ] Shift+Entf: Lücke geschlossen, späteres Material rückt vor (auch Clips weit hinten).
- [ ] Rechtsklick → „Stille einfügen“: alles ab Bereichsanfang rückt um die Bereichslänge nach hinten, andere Spuren bleiben.
- [ ] Rechtsklick → „An den Grenzen teilen“ und „Loop auf den Bereich“.
- [ ] Klick ohne Ziehen oder Werkzeugwechsel hebt den Bereich auf.

---

## A5 Time-Stretch und Sample-Editor

### Tempo und Tonhöhe (A5a/b)
- [ ] Einen Drum-Loop mit bekanntem Tempo (z. B. 95 BPM aus dem Dateinamen) importieren, Rechtsklick → „Tempo und Tonhöhe…“ → „Erkennen“: Wert auf ±1 BPM richtig.
- [ ] „Folgt dem Projekttempo“ an, Projekttempo ändern: Loop bleibt im Takt (gegen Metronom hören), Clip-Länge passt sich an, Titel zeigt „Tempo … BPM“.
- [ ] Eine Fläche ohne Rhythmus: „Erkennen“ meldet, dass kein Tempo erkennbar ist; Tempo von Hand eingeben geht.
- [ ] Tonhöhe +3 HT: höher, gleiche Länge, Klang ohne starke Artefakte; Doppelklick auf den Regler setzt 0.
- [ ] Alt+Ziehen an der rechten Kante: Material wird langsamer/schneller statt abgeschnitten; Titel zeigt „… % Länge“. Ohne Alt: normales Trimmen.
- [ ] DSP-Last mit 8 gestreckten Spuren im Blick behalten (Release-Build).

### Sample-Editor (A5c)
- [ ] Doppelklick auf einen Audio-Clip: unterer Reiter „Sample-Editor“ zeigt ihn groß; F7 und Ansicht-Menü öffnen den Reiter auch. Ohne Clip erklärt ein Text, was zu tun ist.
- [ ] Abschnitt ziehen, „Stille“: nur der Abschnitt ist still (hören); Strg+Z stellt her.
- [ ] Verstärken +6 dB, Normalisieren, Fade-in/-out, Abschnitt rückwärts, Phase: jeweils hörbar und mit Strg+Z zurück.
- [ ] Ohne Auswahl wirkt eine Bearbeitung auf den ganzen Clip.
- [ ] Strg+Rad zoomt um den Zeiger, Rad scrollt, Buttons +/−/alles.
- [ ] Clip mit „Folgt dem Projekttempo“: Bearbeitungs-Buttons gesperrt, Hinweis in der Infozeile.
- [ ] Nach einigen Bearbeitungen liegen mehrere Dateien in `Audio/` (erwartet; Aufräumen kommt mit dem Audio-Pool in D1).

---

## A6 Mixer und Effekte

### FK Reverb, FK Delay, FK Gate (A6a)
- [ ] FX-Spur mit FK Reverb (Mix 100 %), Gesang per Send darauf: Hall klingt natürlich, ohne Flattern oder Metallklang; Decay 0,5 s / 2 s / 8 s hörbar verschieden; Pre-Delay trennt die Stimme vom Hall.
- [ ] Damping runter: Hall dunkler. Low Cut hoch: Bass verschwindet aus dem Hall. Width 0: Hall mono.
- [ ] Export mit Reverb auf der letzten Note: Die Fahne klingt im Export vollständig aus.
- [ ] FK Delay auf einer Gitarre, Sync an, 1/8.: Echos sitzen im Takt (gegen Metronom). Projekttempo ändern: Echos folgen.
- [ ] Ping-Pong: Echos springen links/rechts. Feedback 90 %: lange Fahne, keine Übersteuerung ins Unendliche. Time während des Spielens drehen: sanfter Tonhöhen-Schleifer, kein Knacken.
- [ ] FK Gate auf einer Snare-Spur mit Übersprechen: Zwischen den Schlägen still; Range −15 dB: Übersprechen nur leiser. GR-Anzeige bewegt sich.
- [ ] DSP-Last mit 4× Reverb und 4× Delay im Release-Build im Blick behalten.

### Kennlinien (A6b)
- [ ] FK Compressor öffnen, Threshold und Ratio drehen: Knick der Kurve wandert sofort mit; Knee macht den Knick rund. Bei Wiedergabe läuft ein gelber Punkt auf der Kurve.
- [ ] FK Distortion: Drive hoch = Kurve steiler und flacher oben; Shape Hard = eckige Kanten; Output verschiebt die Höhe.

### Sidechain (A6c)
- [ ] Bass mit FK Compressor, Rechtsklick auf den Slot → Sidechain-Quelle → Kick: Slot zeigt „SC“, im Plugin ist „Ext. Sidechain“ an. Bass pumpt mit der Kick (Ratio hoch, Threshold runter).
- [ ] Kick stumm schalten: Bass pumpt weiter (Geister-Kick).
- [ ] Quelle „Keine“: Pumpen hört auf, Schalter ist aus.
- [ ] FK Gate mit Sidechain: Bass öffnet nur, wenn die Kick spielt.
- [ ] Ein VST3 mit Sidechain (z. B. ein freier Compressor): Menü erscheint, Quelle wirkt.

### Trim und Phase (A6c)
- [ ] Inspector: Trim-Drehregler (kleiner Knopf neben Ø) auf +6 dB: Spur hörbar lauter, Insert-Kompressor greift stärker; Doppelklick = 0 dB; Wert erscheint beim Ziehen als Popup.
- [ ] Ø an einer von zwei identischen Spuren: fast Stille. Snare oben/unten: mit Ø voller.
- [ ] MixConsole: dieselben Regler je Kanalzug, bleiben mit dem Inspector synchron.
- [ ] Trim erscheint nicht in den Insert-Slots und nicht im Effekt-Menü.
- [ ] Fenster niedrig ziehen: Inspector bekommt einen Rollbalken, Fader bleibt erreichbar.

### Direct Outs und Studio-Verbindungen (A6c)
- [ ] Interface mit mehr als zwei Ausgängen: Ausgangsmenü einer Spur zeigt nach Master und Gruppen „Ausgang 3+4“ usw.
- [ ] Spur auf „Ausgang 3+4“: Signal kommt nur an Ausgang 3/4, nicht im Master-Meter und nicht im Export der Summe.
- [ ] Zurück auf Master: Signal wieder in der Summe.
- [ ] Studio > Studio-Verbindungen (Symbol mit Buchsen): Eingang 3 in „Gitarren-DI“ umbenennen, Enter. Eingangsmenü einer Monospur zeigt „Gitarren-DI“.
- [ ] Ausgangspaar umbenennen („Kopfhörer Drummer“): erscheint im Ausgangsmenü und als Ausgangsname im Inspector.
- [ ] Programm neu starten, anderes Projekt öffnen: Namen sind noch da; alte Projekte haben ihre Eingänge behalten.
- [ ] Namen löschen (Feld leer, Enter): Treibername erscheint wieder.
- [ ] Ohne Audiogerät: Dialog zeigt den Hinweis auf die Audio-Einstellungen.

## A7 Export komplett

### Mixdown
- [ ] Export-Button in der Werkzeugleiste und Datei > Exportieren öffnen denselben Dialog „Exportieren“.
- [ ] MP3, 320 kbit/s: Datei spielt in einem Player, klingt wie das Projekt, Länge passt.
- [ ] OGG, 192 kbit/s: dasselbe. Qualität umstellen ändert die Dateigröße.
- [ ] WAV 16 Bit, 44100 Hz, Dither an: Datei hat 44,1 kHz/16 Bit (Dateieigenschaften), leise Ausklänge rauschen fein statt zu kratzen.
- [ ] Ohne libmp3lame (zum Test Paket entfernen oder auf anderem Rechner): MP3 grau, Hinweis unten nennt das Paket.
- [ ] Normalisieren „Lautheit −14 LUFS“: in einem Lautheitsmesser (z. B. ffmpeg ebur128) ≈ −14 LUFS, True Peak ≤ −1 dBTP.
- [ ] Hall am Ende: mit „Ausklingen lassen“ endet die Datei erst nach dem Hall, ohne ist er abgeschnitten.
- [ ] Bereich „Loop-Bereich“: nur der Loop.
- [ ] Während des Exports bleibt das Fortschrittsfenster bedienbar, Abbrechen meldet „Export abgebrochen“.

### Stems
- [ ] Ziel „Stems“: Spurliste mit Häkchen erscheint, „Alle“/„Keine“ wirken; Ziel ist der Ordner Export/Stems.
- [ ] Drei Spuren + eine Gruppe exportieren: je eine Datei mit Spurnamen; die Gruppe enthält ihre Spuren; Master-Effekte (z. B. Limiter auf dem Master) sind nicht in den Stems.
- [ ] Bass mit Sidechain von der Kick: Bass-Stem pumpt, die Kick ist darin nicht zu hören.
- [ ] Alle Stems in ein neues Projekt ziehen: zusammen klingt es wie der Mix (ohne Master-Effekte).

### MIDI
- [ ] Ziel „MIDI-Datei“: Format- und Audio-Felder verschwinden, Hinweis erklärt den Inhalt.
- [ ] Projekt mit Tempo 100 und zwei Instrumentenspuren exportieren, in MuseScore oder einer anderen DAW öffnen: zwei Spuren mit Namen, Tempo 100, Noten an der richtigen Stelle.
- [ ] Tempowechsel und 6/8-Takt im Projekt: kommen in der Datei richtig an.

### Tempo beim MIDI-Import
- [ ] MIDI-Datei mit anderem Tempo (z. B. aus dem Internet) ins Arrangement ziehen: Rückfrage mit dem BPM-Wert der Datei.
- [ ] „Tempo übernehmen“: Tempospur zeigt das Tempo der Datei (auch Wechsel und Taktart), die Noten sitzen auf dem Raster. Strg+Z stellt das alte Tempo wieder her (zweimal: Clip, dann Tempo).
- [ ] „Projekttempo behalten“: Tempo bleibt, Noten in Schlägen.
- [ ] Datei ohne Tempo oder mit gleichem Tempo: keine Rückfrage.

## A8 Bedienung

### Verlauf, Zoom, Übersichtsleiste (A8a)
- [ ] Ein paar Aktionen (Spur anlegen, Clip verschieben, Fader): Verlauf-Button öffnet die Liste mit genau diesen Schritten und lesbaren Namen; der aktuelle Schritt ist hervorgehoben.
- [ ] Klick auf einen frühen Schritt: Projekt springt zurück, spätere Schritte grau. Klick auf den letzten grauen: alles wieder da.
- [ ] Verlauf offen lassen und weiterarbeiten: Liste wächst mit.
- [ ] Shift+F: ganzes Projekt passt in die Breite. Clips wählen, Alt+S: genau diese Clips füllen die Breite. Bereich mit dem Bereichswerkzeug, Alt+S: der Bereich. Ohne Auswahl: Hinweis.
- [ ] H/G zoomen um den Positionszeiger, er bleibt an seiner Stelle.
- [ ] Zoom-Button: „4 Takte“ zeigt vier Takte ab dem Takt am linken Rand. „Aktuellen Zoom speichern...“, Name eingeben: erscheint unter „Eigene Presets“, auch nach Neustart und in anderen Projekten. „Preset löschen“ entfernt es.
- [ ] Übersichtsleiste einschalten: zeigt alle Clips in Spurfarbe, Rahmen = sichtbarer Bereich. Rahmen ziehen scrollt flüssig, Rand ziehen zoomt, Klick daneben springt. Mausrad-Zoom im Arrangement bewegt den Rahmen mit.

### Tastaturbefehle (A8b)
- [ ] Bearbeiten > Tastaturbefehle (Symbol Tastatur) öffnet den Dialog, alle Befehle mit Bereich und Taste.
- [ ] Suche „zoom“ und „Strg+D“ filtern richtig.
- [ ] Werkzeug „Stift“ wählen, „Taste hinzufügen...“, P drücken: Liste zeigt P, Befehl fett. Im Arrangement wählt P den Stift.
- [ ] Strg+D einem anderen Befehl geben: Hinweis „Vorher bei »Duplizieren«“, Duplizieren hat keine Taste mehr; Menü Bearbeiten zeigt bei Duplizieren keine Taste mehr, Tooltip am Duplizieren-Button auch nicht.
- [ ] Programm neu starten: eigene Belegung ist noch da.
- [ ] „Alle auf Cubase-Standard“: alles wie vorher, nichts mehr fett.
- [ ] Alle bisherigen Kürzel funktionieren unverändert (Leertaste, Num-Block, 1–8, Strg+Z/Y, F2–F7, Shift+F, Alt+S, H/G, Entf/Shift+Entf mit Bereich). F5 öffnet den Key Editor.
- [ ] „*“ startet die Aufnahme auch über Shift+Plus auf der deutschen Tastatur.

### Plugin-Manager und Presets (A8c)
- [ ] Plugin-Manager: Reiter „Meine Plugins“ zeigt alle Plugins mit Kategorie; Suche filtert.
- [ ] Stern bei zwei Plugins: Im Insert-Menü (Inspector und MixConsole) stehen sie ganz oben unter „Favoriten“.
- [ ] Ein VST3 ausblenden: fehlt im Menü, ein Projekt, das es nutzt, lädt und spielt es trotzdem.
- [ ] Kategorie eines Plugins ändern: im Menü unter der neuen Kategorie; „automatisch“ setzt zurück.
- [ ] Kategorien fremder Plugins plausibel (Kompressoren unter Dynamik, Hall unter Hall und Delay); falsche notieren.
- [ ] Instrumentenspur anlegen und Instrument tauschen: dasselbe Menü mit Favoriten und Kategorien.
- [ ] Rechtsklick auf einen Insert > Presets > speichern, Namen eingeben; Regler verdrehen; Preset laden: alles wie gespeichert. Strg+Z nimmt das Laden zurück.
- [ ] Zweite Instanz desselben Plugins auf anderer Spur: das Preset steht auch dort im Menü.
- [ ] VST3 mit Werks-Presets (z. B. Surge XT, Vital): „Werks-Presets des Plugins“ listet sie, Laden wechselt den Klang.
- [ ] Preset löschen: verschwindet aus dem Menü.
- [ ] Gebackener Insert: kein Preset-Menü.

## B1 Rack-Instrumente und Multi-Output

### Rack-Instrumente (B1a)
- [ ] Spur > Rack-Instrument > FK Sampler: zwei Spuren entstehen („FK Sampler Kanal 1“ und „FK Sampler 1+2“). Strg+Z entfernt beide auf einmal.
- [ ] Auf der MIDI-Spur spielen (Keyboard): Ton kommt über den Kanal „1+2“, dessen Meter schlägt aus.
- [ ] SFZ mit `output=1` laden (z. B. Snare auf Ausgang 3+4), Inspector > Ausgänge > „Ausgang 3+4“: neuer Kanal, Snare nur dort, Kick weiter auf 1+2. Inserts auf 3+4 wirken nur auf die Snare.
- [ ] Ausgänge > 3+4 abwählen: Kanal verschwindet.
- [ ] Mit Superior Drummer oder Kontakt (VST3, Multi-Out): 16 Ausgänge erscheinen im Menü, einzelne aktivieren, Instrumente dort auf die Ausgänge legen.
- [ ] Inspector > MIDI-Spur: zweite Spur auf Kanal 2. In Kontakt zwei Instrumente auf Kanal 1 und 2: jede Spur spielt nur ihres, auch beim Live-Spielen.
- [ ] Kanal im Inspector umstellen: wirkt sofort auf Wiedergabe und Live-Spiel.
- [ ] Instrument öffnen (Button im Inspector der MIDI-Spur): das eine Plugin-Fenster des Racks.
- [ ] Instrument ersetzen und Backen sind bei Rack-Spuren ausgeblendet.
- [ ] Projekt speichern und neu öffnen: Rack, Kanäle und Ausgänge sind wieder da und klingen.
- [ ] Normale Instrumentenspur mit FK Sampler und Multi-Out-SFZ: alles auf einem Kanal hörbar.

### MIDI-Spuren an externen Geräten (B1b)
- [ ] Hardware-Synth per USB-MIDI anschließen, Spur > MIDI-Spur (externes Gerät): Ausgang ist der Synth (Spurkopf/Inspector zeigt ihn), Keyboard spielt den Synth.
- [ ] Kanal im Inspector auf 2 stellen, Synth auf Kanal 2: spielt; Synth auf Kanal 1: still.
- [ ] Programm 10 wählen: Synth wechselt den Klang sofort. Am Synth anderen Klang wählen, Play: Programm 10 kommt wieder.
- [ ] Bank 1 + Programm 1 bei einem Gerät mit Bänken: richtige Bank.
- [ ] MIDI-Clip zeichnen und aufnehmen, Key Editor öffnen: wie bei Instrumentenspuren.
- [ ] Clip von einer Instrumentenspur auf die MIDI-Spur ziehen: geht; auf eine Audiospur: geht nicht.
- [ ] Ohne MIDI-Gerät: Ausgangsliste leer, Spur lässt sich trotzdem bearbeiten.

## B2 Drum-Editor und Sampler-Spur

- [ ] FK Drums als Instrumentenspur: Doppelklick auf einen MIDI-Clip öffnet den Drum-Editor mit den FK-Drums-Zonen (Snare Mitte, Rimshot, Hi-Hat halb offen ...), gruppiert mit Trennlinien.
- [ ] Superior Drummer oder EZdrummer: automatisch GM-Map. FK Synth: Key Editor wie bisher.
- [ ] Klick setzt Schläge auf dem Raster, Anschlag-Auswahl wirkt, Klick auf eine Raute löscht, senkrecht ziehen ändert den Anschlag (Raute wird größer/kräftiger).
- [ ] Name anklicken spielt das Instrument an (Vorhören an); Vorhören aus: still.
- [ ] Quantisieren rückt eingespielte Schläge aufs Raster.
- [ ] „Nur benutzte“: nur Zeilen mit Schlägen. Eingespielte Note außerhalb der Map erscheint unten als „(nicht in der Map)“.
- [ ] Map auf „Keine“: Doppelklick öffnet Key Editor; zurück auf „Automatisch“: wieder Drum-Editor.
- [ ] Eigene Map: GM als JSON exportieren (über Service/Datei), Namen ändern, importieren: im Map-Menü wählbar.
- [ ] Spur > Sampler-Spur aus Audiodatei: Sample über die Tastatur spielbar, höher/tiefer gestimmt.
- [ ] Spur > Sampler-Spur (leeres Drum-Kit), MIDI-Clip anlegen, Doppelklick: Drum-Editor. Kick-, Snare-, Hi-Hat-Samples aus dem Dateimanager und aus dem Medien-Browser auf die Zeilen ziehen: Namen der Samples erscheinen unter den Zeilen, Schläge spielen sie.
- [ ] Projekt speichern, schließen, öffnen: Kit klingt wieder; Samples liegen in Audio/, SFZ in Sampler/.

## B3 MIDI-Werkzeuge

### Funktionen und Listen-Editor (B3a)
- [ ] Key Editor > Funktionen: Kopfzeile nennt „Alle Noten“ oder die Zahl der ausgewählten.
- [ ] Transponieren +12 auf eine Auswahl: nur diese wandern eine Oktave. Strg+Z nimmt es zurück.
- [ ] Legato auf eingespielte Noten mit Lücken: jede Note reicht bis zur nächsten; Akkorde gemeinsam.
- [ ] Humanize Timing deutlich auf ein quantisiertes Pattern: klingt lockerer, Noten stehen leicht neben dem Raster; zweimal angewendet wird es lockerer.
- [ ] Anschlag fest 127 / lauter / leiser: Velocity-Lane zeigt es.
- [ ] Doppelte löschen nach doppeltem Einspielen: je Note eine.
- [ ] Listen-Editor (F9 oder Reiter „Liste“): Noten nach Zeit, Position im Format 002.01.000. Doppelklick auf Note, „D#4“ eingeben: Note springt im Key Editor mit. Position „5.3“, Länge „0,5“, Anschlag „127“ wirken. Unsinn („xyz“) setzt die Zelle zurück.
- [ ] Zeilen mit Strg/Shift wählen, Entf: gelöscht.

### MIDI-Inserts (B3b)
- [ ] Insert-Slot > MIDI-Effekte > FK Arpeggiator auf einer FK-Synth-Spur: Slot erscheint vor dem Instrument (oberster Slot). Akkord halten: Arpeggio im Projekttempo. Play: läuft mit dem Metronom synchron; Tempo ändern: folgt.
- [ ] Richtung, Oktaven, Rate, Gate am Plugin-Fenster: hörbar wie beschriftet.
- [ ] FK Chord „Moll7“: eine Taste spielt den Akkord; Akkordart während gehaltener Taste umstellen und loslassen: keine hängenden Noten.
- [ ] FK Transpose +12 während gehaltener Taste, dann loslassen: keine hängende Note.
- [ ] Arpeggiator + Chord hintereinander: eine Taste ergibt ein arpeggierter Akkord.
- [ ] Auf einer Rack-MIDI-Spur: Arpeggiator wirkt nur auf diese Spur.
- [ ] MIDI-Spur an einem Hardware-Synth: Arpeggio kommt am Gerät an.

### Step-Eingabe, Groove, retrospektive Aufnahme (B3c)
- [ ] Key Editor, Länge 1/8, „Step“: grüner Cursor erscheint. C, D, E auf dem Keyboard: drei Achtel nacheinander. C-E-G gleichzeitig: Akkord an einer Stelle. Pause-Pfeil: Lücke; Zurück-Pfeil: Cursor zurück, nächste Note überschreibt nicht, sondern legt dazu.
- [ ] Länge während der Step-Eingabe auf 1/16: nächste Schritte kürzer.
- [ ] Anderen Clip öffnen: Step-Eingabe endet.
- [ ] Zahnrad neben Quantisieren: Stärke 50 % auf eingespieltes Material: halb so stark gerade gerückt. Swing 50 % auf gerade Sechzehntel: hörbar geshuffelt.
- [ ] Groove aus Clip: eingespieltes Hi-Hat-Pattern als Vorlage, auf ein gezeichnetes Pattern anwenden: übernimmt das Gefühl.
- [ ] Instrumentenspur scharf, ohne Aufnahme etwas spielen, dann Button Retrospektive Aufnahme (oder Shift+Num *): Clip mit dem Gespielten erscheint an der richtigen Stelle. Ohne scharfe Spur: Hinweis.

## B4 Spielweisen und Expression Maps

- [ ] Key Editor einer Streicherspur: „Spielweisen“ auf „FK Orchestra Streicher (Kanäle)“: Artikulations-Lane mit 16 Zeilen erscheint über der Velocity-Lane.
- [ ] Klick in „Staccato“ über einer Note: Block in der Zeile, „·“ auf dem Notenbalken. Nochmal: weg. Mehrere Noten auswählen, Klick über einer davon: alle bekommen die Spielweise.
- [ ] Mit einer Library, die Kanäle je Spielweise kann (z. B. Kontakt-Multi mit Instrumenten auf Kanal 1/3/13): Normal, Staccato und Pizzicato klingen unterschiedlich.
- [ ] „Keyswitches ab C0“ mit einer Keyswitch-Library (z. B. Spitfire, VSCO in Kontakt): Spielweisen wechseln richtig; mitten im Song starten: die erste Note hat sofort die richtige Spielweise.
- [ ] Spielweise ändern während der Wiedergabe: ab der nächsten Note hörbar (falls nicht: notieren, ob erst nach Stop/Play).
- [ ] MIDI-Export mit Spielweisen: in einer anderen DAW sind Kanäle bzw. Keyswitch-Noten zu sehen.
- [ ] Projekt speichern und öffnen: Map und Spielweisen sind wieder da.
- [ ] Map „Keine Spielweisen“: Lane verschwindet, Noten spielen normal; Spielweisen an den Noten bleiben erhalten und kommen mit der Map zurück.

## B5 Content und Browser

### Content-Manager (B5a)
- [ ] Studio > Content-Manager: zwei Pakete mit Beschreibung, Lizenz, Größe, „nicht installiert“.
- [ ] Salamander Grand Piano installieren (ca. 750 MB, vorher freien Platz prüfen!): Fortschritt läuft, danach „installiert“, Button „Entfernen“.
- [ ] Abbrechen während des Downloads: Status zurück auf „nicht installiert“, im Ordner Content/ bleibt nichts liegen.
- [ ] Spur > Instrument aus Bibliothek > Salamander Grand Piano: Varianten erscheinen; eine wählen: Spur spielt Klavier (Anschlagstufen hörbar).
- [ ] Projekt speichern und öffnen: Klavier lädt wieder.
- [ ] Entfernen: Paket weg, Menü zeigt den Hinweis auf den Content-Manager.
- [ ] Ohne Internet: verständliche Fehlermeldung, nichts bleibt liegen.
- [ ] VSCO 2 nur installieren, wenn genug Platz ist (ca. 5 GB frei nötig); sonst muss die Platzprüfung mit Meldung abbrechen.

### Instrumenten-Browser (B5b)
- [ ] F10 oder Ansicht > Instrumenten-Browser: Liste mit FK Synth, FK Sampler, fremden Instrumenten, Bibliotheksinstrumenten und eigenen Presets; jede Zeile mit Art, Quelle, Tags.
- [ ] Suche „piano“, „violin pizz“: passende Einträge. Tag-Filter „Strings“ (nach VSCO-Installation): nur Streicher.
- [ ] Stern anklicken: Eintrag rutscht nach oben; Stern-Schalter oben zeigt nur Favoriten. Plugin-Favorit erscheint auch im Plugin-Manager und im Insert-Menü oben.
- [ ] Rechtsklick > Tag hinzufügen „Film“: Tag-Filter kennt „Film“. Eigenen Tag wieder entfernen; automatischen Tag entfernen: Hinweis.
- [ ] Doppelklick auf ein Bibliotheksinstrument: Spur mit FK Sampler, spielt. Auf ein eigenes Preset: Spur mit dem Plugin, Preset geladen (Plugin-Fenster zeigt die Werte).
- [ ] Button „Bibliotheken“: Content-Manager; nach Installation neue Instrumente sofort in der Liste.

## C Windows, CI und Pakete

### Version und Update-Hinweis
- [ ] Hilfe > Über: Version, Kanal (dev), Git-Stand, Build-Datum, Plattform stehen da.
- [ ] Hilfe-Menü in einem Entwickler-Build: „Nach Updates suchen (Entwickler-Build ohne Update-Adresse)“ und der Haken sind ausgegraut.
- [ ] Release-Build mit Update-Adresse (`FK_UPDATE_URL`): liegt dort eine neuere Version, zeigt die App den Hinweis, „Herunterladen“ öffnet den Link im Browser.
- [ ] Haken „Beim Start nach Updates suchen“ aus: nach Neustart keine Prüfung, Haken bleibt aus.
- [ ] Ohne Internet: Prüfung von Hand gibt eine verständliche Meldung, beim Start kommt nichts.

### Linux-Paket
- [ ] `dist/build-linux.sh`: Release-Build und alle Tests grün.
- [ ] `dist/package-linux.sh`: AppImage, .sha256 und release-linux-x64.json in dist/out.
- [ ] AppImage starten (auch auf einem zweiten Linux ohne Entwicklungspakete, z. B. Live-USB): Fenster, Symbol, Ton über ALSA/JACK/PipeWire, Projekt speichern, Export WAV und MP3.

### Windows
- [ ] `.\dist\build-windows.ps1`: Build ohne Fehler, alle Tests grün (Warnungen notieren).
- [ ] ASIO erscheint in der Treiberwahl der Audio-Einstellungen, ASIO-Gerät (z. B. ASIO4ALL oder Interface-Treiber) spielt und nimmt auf.
- [ ] WASAPI: Ausgabe, Aufnahme, Latenz brauchbar.
- [ ] `.\dist\package-windows.ps1`: Setup.exe (mit Inno Setup) und Zip in dist\out.
- [ ] Installer: installiert ins Programme-Verzeichnis, Startmenü-Eintrag mit Symbol, startet; Deinstallation entfernt alles außer den Einstellungen.
- [ ] Zip entpacken, Programm starten: läuft ohne Installation.
- [ ] Umlaute in Menüs und Dateinamen (z. B. Projekt „Übung“) richtig.
- [ ] VST3-Plugins scannen und laden (Standardordner C:\Program Files\Common Files\VST3).

### CI
- [ ] Nach dem Push auf GitHub: Workflow „CI“ grün (Windows von Hand starten).

### Audio unter JACK/PipeWire
- [x] Mehrkanal-Interface (Scarlett 18i20) unter JACK: App startet, Audio-Thread hat Echtzeit-Priorität, Eingang 1 kommt an der Spur an.
- [x] Bei laufender App ALSA → JACK und zurück: kein Absturz (vorher SIGKILL durch RLIMIT_RTTIME).
- [x] Eingänge heißen „Eingang 1“, „Eingang 1+2“ …, keine „monitor_“-Einträge.
- [x] Projekt mit 44,1 kHz unter PipeWire (48 kHz) öffnen: Frage „Projekt auf 48000 Hz umstellen?“, Umstellen klappt.
- [ ] Spur auf Eingang 1, 30 s spielen, retrospektive Aufnahme: Clip ist volle 30 s lang (auch nach einem Gerätewechsel).
- [ ] Eigenen Namen für einen Eingang im Studio-Dialog vergeben: erscheint an der Spur statt „Eingang N“.

## Sprache (2026-10-20)

- [ ] `~/.config/FKAudioSuite/common.json` löschen, DAW mit `LANG=en_US.UTF-8` starten: Menüs, Werkzeugleiste, Dialoge englisch.
- [ ] Dasselbe mit `LANG=de_DE.UTF-8`: alles deutsch, ohne Einrichtungsdialog.
- [ ] Studio → Language/Sprache → English wählen: Hinweis erscheint, nach Neustart englisch, auch mit deutschem System. `common.json` enthält `"language": "en"`.
- [ ] Durchklicken auf Deutsch: kein englischer Rest in Menüs, Inspector, Mixer, Export-Dialog, Plugin-Manager, Content-Manager, Key-, Drum- und Listen-Editor; keine abgeschnittenen Beschriftungen (deutsche Texte sind länger).
- [ ] Ein bestehendes Projekt nach dem Sprachwechsel öffnen: Spuren, Plugins, Automation, Drum-Maps unverändert und spielbar.
