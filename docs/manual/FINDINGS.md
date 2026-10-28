# Funde beim Schreiben des Handbuchs

Bugs und Unstimmigkeiten, die beim Dokumentieren auffallen. **Nicht sofort fixen**: Am Ende geht der Code-Agent die Liste durch. Das Handbuch beschreibt bis dahin das **aktuelle** Verhalten.

## So hängen Fund und Handbuch zusammen

- Jeder Fund hat eine Nummer `F-NNN`.
- Jede Handbuchstelle, die sich durch den Fix ändern würde, trägt direkt davor einen Marker:
  `<!-- FIX F-001: was sich am Text ändert -->`
  Finden: `grep -rn "FIX F-001" docs/manual tests/manual`. Szenen, deren Bild sich ändert, tragen denselben Marker als Kommentar (`// FIX F-001`).
- Nach dem Fix: markierte Stellen anpassen, Marker entfernen, Bilder neu erzeugen, Status auf **behoben** mit Commit.
- `tools/manual.py check` meldet Marker ohne Fund und offene Funde, deren Marker fehlen; `tools/manual.py findings` listet je Fund die Stellen.

Typen: **Bug** (falsches Verhalten), **Unstimmigkeit** (verschiedene Stellen verhalten sich oder benennen unterschiedlich), **Text** (Tooltip/Beschriftung falsch oder abgeschnitten), **UX** (funktioniert, aber umständlich oder unerwartet).

---

## F-001 · Unstimmigkeit · Oktavbezeichnung des mittleren C

- **Status:** offen
- **Code:** `src/ui/fk/ui/KeyEditor.cpp:22` und `src/ui/fk/ui/VirtualKeyboard.cpp:80` nennen MIDI-Note 60 **C4**; `src/ui/fk/ui/DrumEditor.cpp:290`, `:527` und `src/ui/fk/ui/ListEditor.cpp:116` nennen sie **C3** (`getMidiNoteName (…, 3)`).
- **Erwartung:** eine Konvention in der ganzen DAW, am besten einstellbar (C3 wie Cubase/Yamaha, C4 wie Logic/wissenschaftlich).
- **Handbuch:** key-editor.md §2 (Reading the note grid); später Drum Editor, List Editor, Bildschirm-Keyboard. Konventionen in README.md.

## F-002 · Text · Tooltip von Quantize sagt „all note starts“

- **Status:** offen
- **Code:** `src/ui/fk/ui/KeyEditor.cpp:344` überschreibt den richtigen Tooltip aus dem Konstruktor (Zeile 272: „…of the selection (all notes without a selection)“) mit „Quantize: snap all note starts to the grid“. Quantisiert wird aber nur die Auswahl, ohne Auswahl alle Noten.
- **Erwartung:** Zeile 344 entfernen.
- **Handbuch:** keine Textänderung nötig (beschreibt schon das Verhalten); Szene `KeyEditorScenes.cpp` sucht den Button über „Quantize:“ und bleibt gültig.

## F-003 · UX · Controller-Lanes werden nicht gespeichert

- **Status:** offen
- **Code:** `src/ui/fk/ui/KeyEditor.h:167` (`controllerLanes` nur im Editor), keine Ablage im Projekt.
- **Beobachtung:** Nach Neustart oder Projektwechsel sind alle zusätzlich geöffneten Lanes weg; man muss sie jedes Mal neu hinzufügen und die Art wählen. Cubase merkt sich die Lanes je Spur.
- **Handbuch:** key-editor.md §16 (Absatz „Lanes are not saved …“).

## F-004 · UX · Velocity-Lane: Akkordnoten liegen übereinander

- **Status:** offen
- **Code:** `src/ui/fk/ui/VelocityLane.cpp` (Balken je Notenanfang, `findNoteAt` trifft nur eine Note).
- **Beobachtung:** Bei Akkorden liegen alle Balken auf derselben Stelle; sichtbar und greifbar ist nur einer. Einzelne Akkordnoten lassen sich in der Lane nicht ändern, nur über Auswahl + Ziehen oder Ctrl+Shift im Raster.
- **Erwartung:** z. B. nur ausgewählte Noten in der Lane bearbeiten, wenn mehrere übereinander liegen, oder Balken leicht versetzen.
- **Handbuch:** key-editor.md §11.

## F-005 · Text · Abgeschnittene Beschriftungen im Key Editor

- **Status:** offen
- **Code:** `src/ui/fk/ui/KeyEditor.cpp` `resized()`: Button **Lane** sitzt in 56 px (zeigt „L…“); Kopf der Artikulations-Lane (`ArticulationLane`, `headerWidth` = 56 px) kürzt Namen („Staccatiss…“, „Bartok …“); in schmalen Fenstern wird die Werkzeugleiste rechts abgeschnitten (Step zeigt „S…“, Akkordbereich unsichtbar).
- **Erwartung:** Lane-Button nur mit Symbol oder breiter; Artikulationsköpfe breiter oder Tooltip mit vollem Namen; Werkzeugleiste umbrechen oder Überlaufmenü.
- **Handbuch:** key-editor.md §2 (Tip zur schmalen Werkzeugleiste), §16, §17; Bilder `key-editor/overview`, `key-editor/controller-lanes`, `key-editor/articulations`.

## F-101 · Unstimmigkeit · Zwei Einstellungsordner „FKAudioSuite“ und „FK Audio Suite“

- **Status:** offen
- **Code:** `src/app/Application.cpp:57-60` (`appDataFolder()` = `…/FKAudioSuite`: Logs, Berichte, Vorlagen, Content, Updates.txt), `src/plugins/fk/plugins/i18n/SharedSettings.cpp:13-18` (`FKAudioSuite/common.json`) gegen `src/engine/fk/engine/EngineContext.cpp:11` mit `getApplicationName()` = „FK Audio Suite“ → Tracktion `PropertyStorage::getAppPrefsFolder()` legt `~/.config/FK Audio Suite/Settings.xml` an (Audiogerät, Pluginliste, zuletzt geöffnete Projekte).
- **Beobachtung:** Der Nutzer hat zwei fast gleich heißende Ordner im Profil. Wer „die Einstellungen zurücksetzen“ will, erwischt leicht nur einen.
- **Erwartung:** ein Ordner, z. B. `PropertyStorage` überschreiben (`getAppPrefsFolder` → `FKAudioSuite`) mit einmaliger Übernahme der alten `Settings.xml`.
- **Handbuch:** getting-started.md §7.

## F-102 · Text · Deutsche Ordner- und Dateinamen im Nutzerprofil

- **Status:** offen
- **Code:** `src/app/Application.cpp:62-73` (`Absturz`, Datei „Absturz <Datum>.txt“), `:121` und `:182` (`Haenger`), `:175` (`Vorlagen`), `:200` (`Bibliothek.json`).
- **Beobachtung:** In der englischen Oberfläche heißen die Ordner, die man über **Help › Show log folder** oder den Diagnosebericht sieht, „Haenger“, „Absturz“, „Vorlagen“. Das englische Handbuch muss die deutschen Namen nennen.
- **Erwartung:** neutrale englische Namen (Hangs, Crashes, Templates, Library.json), mit Umzug der vorhandenen Ordner beim Start. Zusammen mit F-306 beheben (gleiche Ursache, Projektordner).
- **Handbuch:** getting-started.md §7.

## F-103 · Text · Bildschirm-Keyboard heißt als MIDI-Gerät immer „Bildschirm-Keyboard“

- **Status:** offen
- **Code:** `src/services/fk/services/MidiService.cpp:18-21` (`getVirtualKeyboardName()` gibt fest „Bildschirm-Keyboard“ zurück, ohne TRANS).
- **Beobachtung:** In **Audio settings › MIDI inputs** und in den Eingangslisten der Spuren steht auch in der englischen Oberfläche „Bildschirm-Keyboard“. Der Reiter heißt dagegen **Keyboard** und der Befehl „On-Screen Keyboard“.
- **Erwartung:** Gerätekennung intern belassen (Tracktion merkt sich das virtuelle Gerät unter seinem Namen), aber den angezeigten Namen übersetzen, wie bei „All MIDI inputs“ (`getAllInputsName`).
- **Handbuch:** getting-started.md §5.7 (Note). Szene `GettingStartedScenes.cpp` (Bild `getting-started/audio-settings` zeigt den Namen).

## F-104 · UX · Zwei Testton-Bedienelemente in den Audio-Einstellungen

- **Status:** offen
- **Code:** `src/ui/fk/ui/AudioSettingsComponent.cpp:18` (`AudioDeviceSelectorComponent` zeigt neben dem Ausgang JUCEs Button **Test**, „Plays a test tone“) und `:19`, `:52-63` (eigener **Test tone** mit **Frequency**).
- **Beobachtung:** Zwei Knöpfe für fast dasselbe. **Test** spielt einen kurzen Ton, **Test tone** einen Dauerton mit einstellbarer Frequenz. Unklar, welchen man nehmen soll.
- **Erwartung:** nur einen behalten, z. B. JUCEs Test-Button ausblenden.
- **Handbuch:** getting-started.md §5.2 (Tabelle 2 und 10), §5.6. Bild `getting-started/audio-settings`.

## F-105 · UX · Spurkopf: R, M, S und Eingangsfeld ohne Tooltip

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:25-27` (`armButton ("R")`, `muteButton ("M")`, `soloButton ("S")`) und `inputBox` ohne `setTooltip`. Tooltips gibt es nur für Name, Instrument und Bake (`:112-131`).
- **Beobachtung:** Einsteiger sehen nur Buchstaben. Dass **R** die Aufnahmebereitschaft ist und das Feld den MIDI- bzw. Audioeingang wählt, erfährt man nirgends. Widerspricht der Regel „jede Funktion sichtbar mit Tooltip“.
- **Erwartung:** Tooltips wie „Record enable: record on this track (red = armed)“, „Mute“, „Solo“, „Input: MIDI or audio input of the track“.
- **Handbuch:** getting-started.md §6 Schritt 4 (Aufnahme), Ergebnistabelle Nr. 1.

## F-106 · Text · Studio-Verbindungen: Titel und Hinweis weichen vom Menü ab

- **Status:** offen
- **Code:** `src/ui/fk/ui/StudioConnectionsDialog.cpp:48` (Hinweis „…under Studio > Audio and MIDI Settings…“, Menü heißt **Audio and MIDI settings...**), `:100` und `:106` (Titel „Studio Connections“ gegen Menü `src/ui/fk/ui/ProjectComponent.cpp:2111` „Studio connections...“).
- **Erwartung:** gleiche Schreibweise wie im Menü, Pfad mit „›“ wie die übrigen Hinweise.
- **Handbuch:** getting-started.md §5.8.

## F-107 · UX · Studio-Verbindungen nur mit offenem Projekt

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:2112-2114` (`connections.isEnabled = hasProject`), `src/services/fk/services/TrackService.cpp:256-262`, `:283-288` (`getStudioPorts`/`setPortAlias` brauchen ein Edit, obwohl die Geräte der Engine gehören).
- **Beobachtung:** Die Namen gelten laut Tooltip für alle Projekte. Einrichten lässt sich das Studio aber erst, nachdem man ein Projekt angelegt hat.
- **Erwartung:** Ports über `EngineContext`/`DeviceManager` ohne Edit lesen und benennen, Menüeintrag immer aktiv.
- **Handbuch:** getting-started.md §5.8, Quick reference.

## F-108 · UX · Hänger- und Absturzberichte bleiben unbemerkt

- **Status:** offen
- **Code:** `src/app/Application.cpp:66-73` (Absturzbericht), `src/app/HangWatchdog.cpp` (Hängerbericht). Beim nächsten Start prüft `Application::initialise` nicht, ob neue Berichte da sind.
- **Beobachtung:** Berichte entstehen still. Der Nutzer weiß nicht, dass es sie gibt und dass er sie mit **Help › Export diagnostic report...** schicken könnte.
- **Erwartung:** beim nächsten Start einmal fragen („FK Audio Suite was not closed properly / stopped responding. Export a diagnostic report?“ mit **Export…**, **Not now**).
- **Handbuch:** getting-started.md §8.

## F-201 · Unstimmigkeit · Fest eingetragene Tasten in Tooltips und Menütexten

- **Status:** offen
- **Code:** Die Werkzeugleiste setzt die Taste in vielen Tooltips aus der gültigen Belegung (`src/ui/fk/ui/ProjectComponent.cpp:992` `updateShortcutTooltips`), andere Stellen haben die Taste fest im Text: Zoom-Button `ProjectComponent.cpp:210` („Shift+F“, „Alt+S“), Untere-Zone-Button `:241` („F3“ … „F10“), Untermenü `:1919` „Range (range tool, key 2)“, Bereichswerkzeug `:232` („Del“, „Shift+Del“), Bereichsmenü `src/ui/fk/ui/ArrangeView.cpp:236-237` („\tDel“, „\tShift+Del“), alle Tooltips der Transportleiste `src/ui/fk/ui/TransportBar.cpp:47-62` und `:128-131` (Home, Num -, Num +, Shift+B, Shift+N, Insert, Space, *, Shift+Num *, /, I, C, Num 1/2, Ctrl+Num 1/2), Marker-Plus `src/ui/fk/ui/MarkerTrack.cpp:22` („Insert“).
- **Beobachtung:** Nach einer Änderung unter **Edit › Keyboard shortcuts…** nennen diese Stellen weiter die alte Taste. Der Aufnahme-Tooltip sagt „*“, Menü und Tastenliste sagen „Num *“.
- **Erwartung:** alle Tasten über `KeyBindings::shortcutText` bzw. `menuText` einsetzen, wie bei den übrigen Toolbar-Buttons.
- **Handbuch:** project-window.md §4 (Tabelle Zoom-Button, Lower zone), §3.2 (Range-Untermenü), §5 (Transportleiste: Tasten in der Tabelle sind die Standardtasten).

## F-202 · Unstimmigkeit · Reiter der unteren Zone heißen anders als Menü und Tooltip

- **Status:** offen
- **Code:** `src/ui/fk/ui/LowerZone.cpp:19-26` (Reiter **Keyboard**, **Media**, **List**, **Instruments**) gegen `src/ui/fk/ui/ProjectComponent.cpp:2015-2048` (Menü **On-screen keyboard**, **Media browser**, **List Editor**, **Instrument browser**) und Tooltip `:241` („Keyboard“, „Media“, „List Editor“, „Instruments“); Befehlsnamen in `src/ui/fk/ui/Commands.cpp:68-72` („On-Screen Keyboard“, „Media Browser“, „Instrument Browser“).
- **Erwartung:** ein Name je Bereich überall (z. B. Reiter „List Editor“).
- **Handbuch:** project-window.md §8 (Tabelle der Reiter); Bild `project-window/lower-zone`.

## F-203 · Unstimmigkeit · Befehlsnamen der Tastenliste weichen von Menüs ab

- **Status:** offen
- **Code:** `src/ui/fk/ui/Commands.cpp:24-75` gegen die Menüs in `src/ui/fk/ui/ProjectComponent.cpp:1824-2161`: „Left/Right Locator to Cursor“ ↔ „Set left/right locator to playhead“, „Paste at Cursor“ ↔ „Paste at playhead“, „Show Entire Project“ ↔ „Show whole project“ ↔ Zoom-Menü „Whole project“, „Nudge Selection Left/Right“ ↔ „One grid step left/right“, „Keyboard Commands“ (Befehl und Fenstertitel `ShortcutEditor.cpp:295`) ↔ Menü „Keyboard shortcuts...“, „Delete“ ↔ „Delete clips“. Der Befehl „Keyboard Commands“ steht in der Kategorie **View**, der Menüeintrag unter **Edit**. Dazu „cursor“ in Tooltips („Add marker at the cursor“) neben „playhead“ in Menüs; Menü **Studio › Audio and MIDI settings...** öffnet ein Fenster „Audio settings“.
- **Erwartung:** dieselben Wörter in Menü, Tooltip und Tastenliste (entweder „cursor“ oder „playhead“).
- **Handbuch:** project-window.md §3 (Menütabellen), §11 (Shortcuts).

## F-204 · UX · Reihenfolge der Werkzeuge passt nicht zu ihren Tasten

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:653-660` (Leiste: Select, Range, Pencil, Split, Eraser, Glue, Zoom, Mute) gegen `src/ui/fk/ui/Commands.cpp:52-59` (Tasten 1, 2, 8, 3, 5, 4, 6, 7).
- **Beobachtung:** Wer die Tasten von links nach rechts lernt, liegt ab dem dritten Werkzeug falsch.
- **Erwartung:** Leiste nach Tasten ordnen (Select 1, Range 2, Split 3, Glue 4, Eraser 5, Zoom 6, Mute 7, Pencil 8, wie Cubase).
- **Handbuch:** project-window.md §4 (Tabelle 22–29); Bild `project-window/toolbar-edit`.

## F-205 · Unstimmigkeit · Undo/Redo-Buttons aktiv, obwohl nichts rückgängig zu machen ist

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:2775-2776` (Buttons aktiv, sobald ein Projekt offen ist) gegen `:1868-1871` (Menü fragt `canUndo`/`canRedo`).
- **Erwartung:** Buttons wie das Menü sperren, wenn es keinen Schritt gibt.
- **Handbuch:** project-window.md §4 (Tabelle 13–14).

## F-206 · Unstimmigkeit · Nummern neuer Spuren zählen verschieden

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1660`, `:1684`, `:1695`, `:2236`, `:2428`, `:2437`, `:2516` zählen **alle** Spuren (+1) für „Audio {n}“, „<Instrument> {n}“, „Group {n}“, „MIDI {n}“; `:1714` zählt nur FX-Spuren („FX {n}“), `:1707` nur Ordner („Folder {n}“).
- **Beobachtung:** In einem Projekt mit drei Spuren heißt die erste Gruppenspur „Group 4“, die erste FX-Spur aber „FX 1“.
- **Erwartung:** je Spurart zählen (oder überall gleich).
- **Handbuch:** project-window.md §3.3 (Hinweis zu Spurnamen).

## F-207 · UX · Fenstertitel zeigt weder Projekt noch ungespeicherte Änderungen

- **Status:** offen
- **Code:** `src/app/Application.cpp:193-194` übergibt nur den Programmnamen, `src/ui/fk/ui/ProjectWindow.cpp:9` ändert ihn nie.
- **Beobachtung:** Der Titel ist immer „FK Audio Suite“. Den Projektnamen zeigt nur die Werkzeugleiste rechts, ohne Kennzeichen für ungespeicherte Änderungen.
- **Erwartung:** „Demo Song* – FK Audio Suite“ o. ä.; Taskleiste und Fensterwechsel zeigen dann das Projekt.
- **Handbuch:** project-window.md §1.

## F-208 · UX · Fensteraufteilung wird nicht gespeichert

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectWindow.cpp:14` (immer 1280×720, mittig), `src/ui/fk/ui/ProjectComponent.h` `lowerZoneHeight` (Startwert `theme::mixerHeight`), Inspector-, Unterzonen- und Übersichtsleisten-Zustand nur im Speicher; Spurhöhe in `ArrangeView` ebenso.
- **Beobachtung:** Nach jedem Start sind Fenstergröße, Höhe und Reiter der unteren Zone, Inspector, Übersichtsleiste und Spurhöhe wieder auf Standard.
- **Erwartung:** Layout je Projekt oder global merken.
- **Handbuch:** project-window.md §1, §9.

## F-209 · UX · Häufige Befehle ohne Taste, Transport-Menü unvollständig

- **Status:** offen
- **Code:** `src/ui/fk/ui/Commands.h` hat keine Befehle für Neues Projekt, Öffnen, Beenden, Importieren und „Lower zone on/off“; sie lassen sich darum auch nicht belegen. Umgekehrt fehlen im Menü **Transport** (`src/ui/fk/ui/ProjectComponent.cpp:2075-2107`) Loop, Metronom und retrospektive Aufnahme, obwohl es Befehle mit Tasten gibt (Num /, C, Shift+Num *).
- **Beobachtung:** Die untere Zone lässt sich per Taste öffnen (F3–F10), aber nicht schließen; F3 bei sichtbarer MixConsole tut nichts.
- **Erwartung:** Befehle für Neu (Ctrl+N), Öffnen (Ctrl+O), Beenden (Ctrl+Q), Import, untere Zone ein/aus; Menüeinträge für Loop, Metronom, retrospektive Aufnahme.
- **Handbuch:** project-window.md §3.1, §3.5, §9, §11.

## F-210 · Bug · Marker hinzufügen verschluckt Fehler

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:86`, `src/ui/fk/ui/ProjectComponent.cpp:841` und `:2353` verwerfen das Ergebnis von `MarkerService::addMarker` (`(void)`); `src/ui/fk/ui/MarkerTrack.cpp:186` zeigt den Fehler.
- **Beobachtung:** Ohne Projekt (Taste Insert, Transport-Button) passiert stumm nichts; derselbe Fehler über das Plus der Markerspur erscheint als Meldung.
- **Erwartung:** überall dieselbe Fehlermeldung (onMessage/showError).
- **Handbuch:** project-window.md §10 (keine Textänderung nötig, Verhalten wird dort nicht beschrieben).

## F-211 · Text · „Loop the range“ und „Loop to the range“

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1918` (**Edit › Range › Loop the range**) gegen `src/ui/fk/ui/ArrangeView.cpp:240` (Rechtsklickmenü „Loop to the range“).
- **Erwartung:** ein Wortlaut.
- **Handbuch:** project-window.md §3.2.

## F-301 · Bug · Automatische Crossfades werden nicht zurückgelesen

- **Status:** offen
- **Code:** `src/services/fk/services/ProjectSettingsService.cpp:90-113` (`getSettings` liest `autoCrossfadesProperty` nie, liefert immer den Standard `true`), geschrieben wird es in `:147`; `projectChanged` (`:250`) und `valueTreeChildAdded` (`:262`) wenden `getSettings().autoCrossfades` an.
- **Beobachtung:** Der Haken „Automatic crossfades on overlapping audio clips“ ist beim Öffnen des Dialogs immer gesetzt. Abhaken entfernt die Crossfades nur von den vorhandenen Clips; neu hinzugefügte Clips und nach dem Wiederöffnen alle Clips bekommen wieder automatische Crossfades.
- **Erwartung:** `settings.autoCrossfades = node.getProperty (autoCrossfadesProperty, true);` in `getSettings`.
- **Handbuch:** projects.md §13 (Note unter der Tabelle); Szene `ProjectsScenes.cpp` (Bild `projects/project-settings`, Haken bleibt gesetzt).

## F-302 · UX · Neu/Öffnen schließt das aktuelle Projekt, bevor das neue sicher geht

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1257-1282` (`newProject`) und `:1295-1310` (`openProjectFile`) rufen erst `closeProjectThen` und dann `createProject`/`createProjectFromTemplate`/`openProject`; die Prüfung „Ordner existiert schon“ liegt in `src/services/fk/services/ProjectService.cpp:117` bzw. `:154`.
- **Beobachtung:** Wählt man für ein neues Projekt einen Namen, dessen Ordner schon existiert, oder ist die zu öffnende Datei beschädigt, kommt die Fehlermeldung erst, nachdem das bisherige Projekt geschlossen wurde; danach ist kein Projekt offen.
- **Erwartung:** Ordner bzw. Datei vor dem Schließen prüfen (oder neues Edit erst laden, dann tauschen); bei Fehler bleibt das alte Projekt offen.
- **Handbuch:** projects.md §3 (Note zum vorhandenen Ordner), §4 (Absatz „If the file cannot be read …“).

## F-303 · Bug · Backup wiederherstellen mit „Save“ kann das gewählte Backup löschen

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1485-1503` (Backup-Liste wird vor `closeProjectThen` festgehalten), `src/services/fk/services/ProjectService.cpp:222-241` (`saveProject` legt ein Backup an und ruft `pruneBackups`), `:40-46`, `:343` (`restoreBackup`: „Backup is not readable“).
- **Beobachtung:** Bei 10 vorhandenen Backups, Wahl des ältesten und „Save“ in der Frage nach ungespeicherten Änderungen löscht das Speichern genau dieses Backup; die Wiederherstellung scheitert mit „Backup is not readable: …“, und es ist kein Projekt mehr offen.
- **Erwartung:** Inhalt des gewählten Backups vor dem Speichern lesen (oder Pruning hier aussetzen).
- **Handbuch:** projects.md §12 (Note am Ende).

## F-304 · Bug · Projektnamen mit Punkt werden abgeschnitten

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1274` (Neu) und `:1369` (Save as) nehmen `chosen.getFileNameWithoutExtension()`.
- **Beobachtung:** „Song v1.2“ ergibt das Projekt „Song v1“ (Ordner und Datei).
- **Erwartung:** Nur die Endung `.tracktionedit` entfernen, sonst den vollen eingegebenen Namen verwenden.
- **Handbuch:** projects.md §3 (Note zum Punkt), §7 (Note).

## F-305 · Bug · Save as und Vorlagen lassen Sampler-Spuren am alten Projektordner hängen

- **Status:** offen
- **Code:** `src/services/fk/services/ProjectService.cpp:259-274` (kopiert nur `Audio/`, `relinkFileReferences` passt nur `source`-Eigenschaften an), `:167-197` (Vorlage übernimmt Plugin-Zustand unverändert); `src/plugins/fk/plugins/SamplerProcessor.cpp:139` speichert den absoluten SFZ-Pfad im Plugin-Zustand; SFZ-Dateien liegen in `Sampler/` (`src/services/fk/services/SamplerService.cpp:132`).
- **Beobachtung:** Nach Save as, nach Verschieben/Umbenennen des Projektordners und in Projekten aus einer Vorlage laden Sampler-Spuren die SFZ-Datei aus dem ursprünglichen Projektordner; fehlt der, schlägt das Laden fehl („SFZ file not found“).
- **Erwartung:** `Sampler/` bei Save as mitkopieren und den SFZ-Pfad relativ zum Projekt speichern bzw. beim Laden aus den gespeicherten Regionen neu schreiben.
- **Handbuch:** projects.md §2 (Note), §7 (Liste „What to know about the copy“), §9.1 (Rules).

## F-306 · Text · Deutsche Ordner- und Dateinamen sichtbar für Nutzer

- **Status:** offen
- **Code:** `src/app/Application.cpp:169` (Vorlagenordner „Vorlagen“), `src/services/fk/services/ProjectService.cpp:352` (Backup-Name „… vor Wiederherstellung …“); gleiche Art: `src/services/fk/services/ClipService.cpp:1041` („… geklebt.wav“ in Audio/), `src/app/Application.cpp:64` („Absturz“), Hänger-Ordner „Haenger“.
- **Beobachtung:** Die englische Oberfläche zeigt im Dateimanager und in der Backup-Liste deutsche Namen.
- **Erwartung:** Englische, feste Namen (z. B. „Templates“, „before restore“, „glued“, „Crashes“, „Hangs“), mit Übernahme vorhandener Vorlagen. Zusammen mit F-102 beheben.
- **Handbuch:** projects.md §9.1 (Tabelle Vorlagenordner), §10 (Tabelle der Dateinamen), §11.2 und §12 (Erwähnung „vor Wiederherstellung“).

## F-307 · UX · Keine Meldung über fehlende Audiodateien oder Plugins beim Öffnen

- **Status:** offen
- **Code:** `src/services/fk/services/ProjectService.cpp:200-214` (`openProject` lädt nur das Edit); keine Prüfung in `src/ui/fk/ui/ProjectComponent.cpp:1295-1344`.
- **Beobachtung:** Fehlen Audiodateien (Projekt auf anderem Rechner, Dateien verschoben) oder Plugins, öffnet das Projekt ohne Hinweis.
- **Erwartung:** Nach dem Öffnen Liste fehlender Dateien/Plugins zeigen, ideal mit „Suchen…“ zum Neuverknüpfen.
- **Handbuch:** projects.md §4.1.

## F-308 · UX · Fehlgeschlagene automatische Sicherung bleibt unbemerkt

- **Status:** offen
- **Code:** `src/services/fk/services/AutosaveService.cpp:64-65` (Fehler nur ins Log), `getLastAutosaveTime` wird in der Oberfläche nirgends genutzt.
- **Beobachtung:** Kann die automatische Sicherung nicht geschrieben werden (Platte voll, keine Rechte), erfährt der Nutzer nichts; es gibt auch keine Anzeige, wann zuletzt gesichert wurde.
- **Erwartung:** Einmalige Meldung beim ersten Fehler; Zeit der letzten Sicherung z. B. im Dialog Projekteinstellungen oder als Tooltip anzeigen.
- **Handbuch:** projects.md §11.1 (Note).

## F-309 · Unstimmigkeit · Bezeichnungen rund um Neu, Vorlagen und Autosave

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1832` „New empty project...“ ↔ `:1125` „Empty project...“; `:1846` „Save as template...“ ↔ `:1138` „Save current project as template...“; `:1139` „Open templates folder (rename, delete)“ nur im Menü des New-Buttons, nicht im Menü **File**; Tooltip `:195` „auto-save“ ↔ `src/ui/fk/ui/ProjectSettingsDialog.cpp:109` „Autosave“ ↔ Tooltip „Automatic backup …“; Menü „Project settings...“ ↔ Fenstertitel `ProjectSettingsDialog.cpp:212` „Project Settings“.
- **Erwartung:** Ein Wortlaut je Funktion; Eintrag „Open templates folder“ auch unter **File**.
- **Handbuch:** projects.md §1 (Tabelle New-Menü).

## F-310 · Bug · Undo der Projekteinstellungen erreicht die Eingänge nicht

- **Status:** offen
- **Code:** `src/services/fk/services/ProjectSettingsService.cpp:149-150` (`applyToInputs`/`applyAutoCrossfades` nur in `setSettings`) und `projectChanged`; der ValueTree-Listener reagiert nur auf `valueTreeChildAdded` (`:262`), nicht auf Eigenschaftsänderungen durch Undo/Redo.
- **Beobachtung:** Nach Undo eines Schritts „Project settings“ stehen die alten Werte im Projekt, die Eingänge nehmen aber weiter im zuletzt gewählten Format, mit der Bittiefe und dem Aufnahmemodus auf, bis ein Feld geändert oder das Projekt neu geöffnet wird. Ein offener Dialog zeigt die alten Werte nicht.
- **Erwartung:** `valueTreePropertyChanged` für FKPROJECTSETTINGS beobachten und Einstellungen erneut anwenden.
- **Handbuch:** projects.md §13 (Note zu Undo).

## F-401 · Bug · Loop und Punch schließen sich nur über den Loop-Button aus

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:97-104` schaltet Punch aus, wenn Loop eingeschaltet wird. Die anderen Wege rufen nur `setLooping (true)`: Taste Num / (`src/ui/fk/ui/ProjectComponent.cpp:819-820`), Ziehen im Lineal (`src/ui/fk/ui/TimeRuler.cpp:96-97`), Range › Loop (`src/ui/fk/ui/ArrangeView.cpp:212-214`), „Loop this section“ (`src/ui/fk/ui/MarkerTrack.cpp:221-222`).
- **Beobachtung:** Mit Punch an lassen Num / oder Ziehen im Lineal Loop und Punch beide an. Die nächste Aufnahme lehnt Tracktion dann ab („Recording can be done in either loop mode or punch in/out mode, but not both at the same time!“).
- **Erwartung:** Ausschluss an einer Stelle, z. B. in `TimelineService::setLooping` oder einem gemeinsamen Helfer.
- **Handbuch:** transport.md §6.3 (Absatz „Loop and punch exclude each other“), §7 (Meldungstabelle).

## F-402 · Unstimmigkeit · Überlastanzeige ab 90 %, Engine schaltet erst bei 98 % stumm

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:474-486` (`overloadLoad = 0.9f` → „Overload: sound muted“) gegen Tooltip `:167` („From 98 % the engine mutes“) und `external/tracktion_engine/modules/tracktion_engine/playback/tracktion_DeviceManager.h:273` (`cpuLimitBeforeMuting { 0.98 }`).
- **Beobachtung:** Zwischen 90 % und 98 % steht „sound muted“, obwohl nichts stumm ist.
- **Erwartung:** Rot und „Overload: sound muted“ erst ab 98 %, darunter z. B. „Overload risk“.
- **Handbuch:** transport.md §14; project-window.md §5/§10 (Schwellen, dort keine Textänderung nötig).

## F-403 · UX · Aufnahme-Meldungen der Engine nur als Blase am Mauszeiger

- **Status:** offen
- **Code:** `src/engine/fk/engine/FkUIBehaviour.h` überschreibt `showWarningMessage`/`showInfoMessage` nicht; Tracktions Standard (`external/tracktion_engine/modules/tracktion_engine/utilities/tracktion_UIBehaviour.cpp:74-87`) zeigt eine BubbleMessage an der Komponente unter der Maus. Quellen: `playback/tracktion_TransportControl.cpp:1484`, `:1490`, `:1609-1610`.
- **Beobachtung:** Num * ohne scharfe Spur, ein Loop bis 2 s oder Loop+Punch erzeugen nur eine kurze Blase am Mauszeiger; liegt der Zeiger außerhalb des Fensters, sieht man nichts. Die Texte sind Tracktion-Texte (Doppel-Leerzeichen in „possible  when“), nicht übersetzt.
- **Erwartung:** FkUIBehaviour leitet sie an die Meldungsanzeige des Projektfensters, mit eigenen Texten.
- **Handbuch:** transport.md §7 (Absatz „When recording does not start“ und Tabelle).

## F-404 · UX · Arrangement folgt dem Positionszeiger nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/ArrangeView.cpp:842-869` (timerCallback zeichnet nur den Zeiger, rollt nie); keine Einstellung zum Mitlaufen.
- **Beobachtung:** Bei der Wiedergabe läuft der Zeiger aus dem sichtbaren Bereich; man muss von Hand rollen.
- **Erwartung:** Auto-Scroll (seitenweise oder fortlaufend) mit Schalter, wie Cubase „Auto-Scroll“.
- **Handbuch:** transport.md §3 (Note zum Positionszeiger).

## F-405 · UX · Kein „Zum Ende“

- **Status:** offen
- **Code:** `src/ui/fk/ui/Commands.cpp:22-37` (Kategorie Transport ohne Befehl für Projektende; Taste Ende frei); kein Button in `src/ui/fk/ui/TransportBar.cpp`.
- **Beobachtung:** Zum Anfang springt Pos1, zum Projektende gibt es nichts.
- **Erwartung:** Befehl „Go to End“ (Ende) und Eintrag im Menü **Transport**.
- **Handbuch:** transport.md §4 (Note).

## F-406 · UX · Hinweis der retrospektiven Aufnahme erscheint als Fehlerfenster

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:359` (`onMessage` mit „Nothing to retrieve…“), `src/ui/fk/ui/ProjectComponent.cpp:443` leitet `onMessage` an `showError` (`:2633-2635`, Titel „Error“, Warnsymbol).
- **Beobachtung:** Ein normaler Hinweis (nichts gespielt, keine Spur scharf) erscheint als **Error**.
- **Erwartung:** Info-Fenster oder Statushinweis statt Fehlerfenster.
- **Handbuch:** transport.md §10.

## F-407 · Bug · Samples-Anzeige ohne Audiogerät zählt Sekunden

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:466` (`juce::jmax (1.0, projectSettings.getDeviceSampleRate())`), `src/services/fk/services/ProjectSettingsService.cpp:168-172` (liefert 0 ohne Gerät).
- **Beobachtung:** Ohne geöffnetes Audiogerät zeigt die Zeitanzeige im Format Samples die Sekundenzahl (Rate 1).
- **Erwartung:** Auf die Projekt-Abtastrate zurückfallen (`getSettings().sampleRate`).
- **Handbuch:** transport.md §5 (letzter Absatz); Szene `TransportScenes.cpp` (kein Samples-Bild).

## F-408 · Text · Locator-Tooltip verspricht „Right-click: set to the cursor“

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:148`, `:150` (Tooltip) gegen `:312-313` und `:428-429` (Rechtsklick öffnet ein Menü mit „Jump there“ und „Set to cursor“).
- **Erwartung:** Tooltip z. B. „Right-click: menu (jump there, set to the cursor)“.
- **Handbuch:** transport.md §6.1.

## F-409 · UX · Automation schreiben ist in neuen Projekten an

- **Status:** offen
- **Code:** `external/tracktion_engine/modules/tracktion_engine/model/automation/tracktion_AutomationRecordManager.cpp:79-80` (`readingAutomation`/`writingAutomation` standardmäßig `true`); FK setzt nichts (`src/services/fk/services/AutomationService.cpp:376-380`).
- **Beobachtung:** Der rote Schreib-Button leuchtet in jedem neuen Projekt. Solange die Spuren auf Read stehen, wird nichts geschrieben, es sieht aber nach Automationsaufnahme aus. Cubase startet mit W aus.
- **Erwartung:** Beim Anlegen eines Projekts Schreiben ausschalten.
- **Handbuch:** transport.md §13.

## F-410 · UX · Metronom hat keine Einstellungen

- **Status:** offen
- **Code:** `src/ui/fk/ui/TransportBar.cpp:113-114` (nur an/aus), `src/services/fk/services/TimelineService.cpp:394-405`. Tracktion bietet `clickTrackGain`, `clickTrackEmphasiseBars`, `clickTrackRecordingOnly`, `clickTrackDevice` (`external/tracktion_engine/modules/tracktion_engine/model/edit/tracktion_Edit.h:882-901`).
- **Beobachtung:** Lautstärke, Betonung auf Zählzeit 1, Klang, Ausgang und „nur bei Aufnahme“ lassen sich nicht einstellen.
- **Erwartung:** Metronom-Einstellungen (z. B. Rechtsklick auf den Metronom-Button oder in den Studio-Einstellungen).
- **Handbuch:** transport.md §9.

## F-411 · Text · Tooltip der retrospektiven Aufnahme nennt nur MIDI (ungeprüft)

- **Status:** offen (vor dem Fix Verhalten prüfen)
- **Code:** Tooltip der retrospektiven Aufnahme in `src/ui/fk/ui/TransportBar.cpp` nennt nur MIDI; `src/engine/fk/engine/EngineContext.cpp:19-29` (`sizeRetrospectiveBuffers`) gibt auch Audioeingängen, die einer Spur zugewiesen sind, 30 s Puffer.
- **Beobachtung:** Möglicherweise holt die retrospektive Aufnahme auch Audio zurück; dann sind Tooltip und Handbuch zu eng.
- **Erwartung:** Verhalten prüfen; Tooltip und transport.md §10 angleichen.
- **Handbuch:** transport.md §10.
