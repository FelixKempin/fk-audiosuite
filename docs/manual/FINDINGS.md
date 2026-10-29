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
- **Ergänzung (Kapitel 7):** auch das Kontextmenü der Clips (`src/ui/fk/ui/TrackLane.cpp:628-643`, Tasten wie „\tCtrl+X“ fest im Text).

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
- **Ergänzung (Kapitel 7):** Kontextmenü „Paste at cursor“ (`src/ui/fk/ui/TrackLane.cpp:631`).

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
- **Ergänzung (Kapitel 11):** auch die Dateinamen der Sample-Editor-Bearbeitungen („Stille“, „Normalisiert“, „Rueckwaerts“ …).
- **Handbuch:** projects.md §9.1 (Tabelle Vorlagenordner), §10 (Tabelle der Dateinamen), §11.2 und §12 (Erwähnung „vor Wiederherstellung“); sample-editor.md §6.

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

## F-701 · Bug · Abgelehnte Mausbearbeitungen an Clips bleiben stumm

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackLane.cpp:873-876` (`TrackLane::reportError` schreibt nur ins Log, ruft `onError` nicht); betroffen sind Trimmen/Strecken (`:584-587`), Fade ziehen (`:597-598`), Split-, Radier-, Mute- und Kleber-Werkzeug (`:376-420`), Take wählen/Comping (`:555-560`, `:710-730`), Reset fades (`:695-696`). `ArrangeView::reportError` (`src/ui/fk/ui/ArrangeView.cpp:1380-1387`) zeigt dagegen eine Meldung.
- **Beobachtung:** Auf einer gebackenen Spur passiert beim Trimmen, Faden, Teilen, Radieren usw. einfach nichts. Auch „Stretching works from a quarter up to four times the length.“ und „The clip follows the project tempo …“ beim Alt+Ziehen sieht der Nutzer nie. Dieselbe Aktion über Menü oder Taste zeigt die Meldung.
- **Erwartung:** `TrackLane::reportError` ruft zusätzlich `onError (services::errorText (error))`.
- **Ergänzung (Kapitel 13):** Automationslage gleiches Muster: `src/ui/fk/ui/AutomationLane.cpp:256` schreibt Fehler nur ins Log (automation.md beschreibt „no message appears“).
- **Handbuch:** arranging.md §21 (Note zu stummen Mausbearbeitungen); recording.md §11.4 (Take-Funktionen: wählen, Comping, Verteilen, Aufräumen).

## F-702 · UX · Doppelklick legt mit jedem Werkzeug einen MIDI-Clip an

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackLane.cpp:791-804` (`mouseDoubleClick` prüft das Werkzeug nicht und ruft `createMidiClipAt`).
- **Beobachtung:** Auf einer Instrumenten- oder MIDI-Spur erzeugt ein Doppelklick ins Leere auch mit Lupe, Bereichs-, Split-, Radier-, Kleber- oder Mute-Werkzeug einen Ein-Takt-Clip. Mit der Lupe wird dabei zweimal gezoomt, mit dem Bereichswerkzeug zugleich ein Bereich begonnen.
- **Erwartung:** Leeren MIDI-Clip per Doppelklick nur mit Auswahlwerkzeug (und Stift) anlegen.
- **Handbuch:** arranging.md §2.1 (Absatz „Double-click“), §18 (Tabelle).

## F-703 · Bug · Clip auf andere Spur ziehen zählt ausgeblendete Spuren mit

- **Status:** offen
- **Code:** `src/ui/fk/ui/ArrangeView.cpp:1364-1378` berechnet `trackDelta` aus den angezeigten Zeilen (`displayedTracks`, `:881-890`, ohne ausgeblendete Spuren und ohne Inhalt zugeklappter Ordner, `src/services/fk/services/TrackService.cpp:668-697`). `ClipService::moveClips` (`src/services/fk/services/ClipService.cpp:796-870`) wendet den Versatz auf `te::getAudioTracks (*edit)` an, also auf alle Spuren. Gleiches Prinzip bei Einfügen/Duplizieren mehrerer Spuren (`makeEntries`/`insertEntries`, `:1104-1205`).
- **Beobachtung:** Liegt zwischen Quell- und Zielspur eine ausgeblendete Spur oder ein zugeklappter Ordner, landet der Clip auf einer anderen Spur als der hervorgehobenen („Move here“), womöglich auf einer unsichtbaren.
- **Erwartung:** Zielspur direkt übergeben (Handle statt Index-Differenz) oder Versatz über die angezeigten Spuren rechnen.
- **Handbuch:** arranging.md §6.2.

## F-704 · UX · Automatische Crossfades sind im Arrangement unsichtbar

- **Status:** offen
- **Code:** `src/ui/fk/ui/ClipView.cpp` `paint` zeichnet nur die eigenen Fades (`fadeInSeconds`/`fadeOutSeconds`), nicht das Überblenden nach `te::IDs::autoCrossfade` (gesetzt in `src/services/fk/services/ProjectSettingsService.cpp:253-268`).
- **Beobachtung:** Überlappende Audio-Clips liegen einfach übereinander. Man hört die Überblendung, sieht aber weder, dass es eine gibt, noch wie lang sie ist.
- **Erwartung:** Überlappungsbereich mit gekreuzten Rampen zeichnen (wie Cubase).
- **Handbuch:** arranging.md §12.2.

## F-705 · Unstimmigkeit · „One grid step left/right“ im Menü aktiv ohne Auswahl

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1937-1942` (Menüeinträge nur an `hasProject` gebunden) gegen `:2783-2784` (Nudge-Buttons brauchen ausgewählte Clips).
- **Beobachtung:** Ohne Auswahl sind die Buttons grau, die Menüeinträge aktiv und tun nichts.
- **Erwartung:** Menüeinträge wie die Buttons sperren (`canPerformClipAction (ClipAction::remove)`).
- **Handbuch:** arranging.md §6.3.

## F-706 · Unstimmigkeit · Alt+Ziehen kopiert im Key Editor, im Arrangement nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackLane.cpp:348-373` und `:603-611` (Ziehen verschiebt immer, Alt wirkt nur an der rechten Kante als Strecken) gegen Key Editor (Alt+Ziehen kopiert Noten, key-editor.md §7).
- **Beobachtung:** Wer im Key Editor mit Alt kopiert, erwartet das auch bei Clips (Cubase: Alt+Ziehen kopiert). Im Arrangement geht Kopieren nur über Kopieren/Einfügen oder Duplizieren.
- **Erwartung:** Alt+Ziehen in der Clipmitte kopiert die Auswahl (Kopien halbtransparent wie im Key Editor), Alt an der rechten Kante bleibt Strecken.
- **Handbuch:** arranging.md §6.2 (Note).

## F-601 · Unstimmigkeit · „Keep takes“ legt bei normaler Aufnahme keine Takes an

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectSettingsDialog.cpp:79-83` (Eintrag „Keep takes“, Tooltip „earlier ones stay selectable as takes“), `src/services/fk/services/ProjectSettingsService.cpp:274-281` (setzt Tracktions Merge-Modus 0 „Overlay newly recorded clips onto edit“); Tracktion `playback/devices/tracktion_WaveInputDevice.cpp:895-905` fügt den neuen Clip mit `DeleteExistingClips::no` ein.
- **Beobachtung:** Bei einer normalen Aufnahme (ohne Loop) liegt der neue Clip über dem alten. Der alte bleibt ein eigener Clip darunter und ist kein Take des neuen. Takes entstehen nur bei Loop-Aufnahme und über „Add take from file...“.
- **Erwartung:** Entweder überlagerte Aufnahmen als Takes in einen Clip übernehmen (wie Cubase „Verlauf behalten“), oder Eintrag und Tooltip ehrlich benennen (z. B. „Overlay“).
- **Handbuch:** recording.md §6 (Tabelle Audio-Modi und Absatz darunter), §8; projects.md §13 (Tabelle Nr. 5).

## F-602 · Bug · R-Button ohne Eingang leuchtet, Spur ist aber nicht scharf

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:48-50` und `src/ui/fk/ui/Inspector.cpp:296-302` (`setClickingTogglesState (true)`); `src/services/fk/services/TrackService.cpp:928-932` (`setArmed` tut ohne Eingang nichts, meldet nichts).
- **Beobachtung:** Bei einer Spur mit „No input“ wird R beim Klick rot, die Spur ist aber nicht scharf. Da sich am Projekt nichts ändert, bleibt der Button bis zum nächsten Neuaufbau rot. Record meldet dann „Recording is only possible when at least one active input device is assigned to a track“.
- **Erwartung:** R ohne Eingang deaktivieren oder nach dem Klick den echten Zustand zeigen, mit Hinweis „Choose an input first“.
- **Handbuch:** recording.md §3 (Note), §16 (Tabelle); tracks.md §13.

## F-603 · UX · Eingangsfeld im Spurkopf wird nie angezeigt

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:205-237` (Feld erst ab 70 px Restbreite); Kopfbreite fest `theme::trackHeaderWidth = 220` (`src/ui/fk/ui/Theme.h:44`).
- **Beobachtung:** Nach R, M, S, Automation und Bake (Instrumentenspur zusätzlich Instrument-Button) bleiben höchstens 64 px. Das Feld ist auf keiner Spurart sichtbar, der Code dafür ist toter Code. Eingänge lassen sich nur im Inspector wählen.
- **Erwartung:** Eingangswahl im Spurkopf erreichbar machen (zweite Zeile ab einer Spurhöhe, breitere Spurliste oder Kontextmenü) oder das Feld entfernen.
- **Ergänzung (Kapitel 5):** `TrackHeader.cpp:236` (`resized`) überschreibt die Sichtbarkeit, die `rebuildInputChoices` je Spurart setzt (`:428`). Würde das Feld gezeigt, böten Gruppen-, FX- und Rack-Ausgangsspuren dort echte Audioeingänge an (der Inspector zeigt richtig „Groups have no input“).
- **Handbuch:** recording.md §2 (Note); tracks.md §4; Szene `RecordingScenes.cpp` (Bild `recording/arm-and-input`, Spurkopf).

## F-604 · UX · Aufnahme erscheint im Verlauf nur als „Change“

- **Status:** offen
- **Code:** `src/services/fk/services/TransportService.cpp:81-102` (`record()` ohne `beginUndoStep`); Tracktion legt die Clips beim Stopp an, die Transaktion bleibt unbenannt; `src/services/fk/services/ProjectService.cpp:509-510` ersetzt leere Namen durch „Change“.
- **Beobachtung:** In Edit › History... steht nach einer Aufnahme nur „Change“. Man erkennt nicht, welcher Schritt die Aufnahme ist.
- **Erwartung:** Vor dem Aufnahmestart einen benannten Undo-Schritt beginnen (z. B. „Recording“), der die beim Stopp entstehenden Clips umfasst.
- **Handbuch:** recording.md §15 (Tabelle und Absatz darunter).

## F-605 · Bug · „Distribute takes to the tracks below“ legt MIDI-Takes auf dieselbe Spur

- **Status:** offen
- **Code:** `src/services/fk/services/ClipService.cpp:1410-1432` ruft `clip->unpackTakes (false)`. Tracktion `model/clips/tracktion_MidiClip.cpp:615-628` nimmt bei `toNewTracks == false` immer die eigene Spur; `model/clips/tracktion_WaveAudioClip.cpp:500-516` nimmt bei Audio die folgenden Spuren ohne Prüfung der Spurart.
- **Beobachtung:** Bei MIDI-Clips landen alle Takes als „Take #1“, „Take #2“… übereinander auf derselben Spur. Bei Audio landen Takes auf beliebigen folgenden Spuren, auch auf Instrument-, Gruppen- oder FX-Spuren; das erlaubt ClipService sonst nicht (`clipFitsTrack`).
- **Erwartung:** Je Take eine passende Spur darunter (fehlt sie oder passt die Art nicht: neue Spur gleicher Art direkt darunter), für Audio und MIDI gleich.
- **Handbuch:** recording.md §11.4 (Absatz „Distribute takes to the tracks below works differently…“).

## F-606 · Unstimmigkeit · Nach Loop-Aufnahme ist bei Audio der erste, bei MIDI der letzte Durchgang aktiv

- **Status:** offen
- **Code:** Tracktion `playback/devices/tracktion_WaveInputDevice.cpp:930-940` (Clip zeigt auf die Datei des ersten Durchgangs, weitere per `addTake`, aktueller Take bleibt 0); `model/clips/tracktion_MidiClip.cpp:439-464` (`addTake` setzt `currentTake` auf den zuletzt hinzugefügten).
- **Beobachtung:** Dieselbe Bedienung ergibt verschiedene Ergebnisse: Audio spielt nach dem Stopp Take 1, MIDI den letzten Take.
- **Erwartung:** Einheitlich, üblich ist der letzte Durchgang (Cubase), z. B. nach der Aufnahme `setCurrentTake (numTakes - 1)` für Audio-Clips.
- **Handbuch:** recording.md §11.1.

## F-607 · UX · Alle Spuren mit demselben MIDI-Eingang spielen gleichzeitig

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:904-909` (MIDI-Eingänge immer `MonitorMode::on`), `:367-372` (jede neue Instrumentenspur bekommt „All MIDI inputs“).
- **Beobachtung:** Mit mehreren Instrumentenspuren erklingen beim Spielen alle Instrumente zugleich, nicht nur das der gewählten Spur. Man muss die anderen Spuren von Hand auf „No input“ stellen.
- **Erwartung:** Live-Spiel nur auf der ausgewählten oder scharf geschalteten Spur (wie Cubase „MIDI Thru für ausgewählte Spur“), oder eine Einstellung dafür.
- **Handbuch:** recording.md §4 (Note), §16 (Tabelle).

## F-608 · Bug · Bereichsanzeige des Bildschirm-Keyboards folgt dem Rollen nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/VirtualKeyboard.cpp:17` (JUCE-Rollpfeile sichtbar), `:58-71` (`updateRangeLabel` nur in `scrollToMiddleC` und `shiftOctave`).
- **Beobachtung:** Nach dem Rollen mit den Pfeilen an den Enden der Tastatur (oder dem Mausrad) zeigt die Anzeige weiter die alte tiefste Taste.
- **Erwartung:** Anzeige bei jeder Änderung der tiefsten sichtbaren Taste nachführen (z. B. Timer oder eigene Rollpfeile).
- **Handbuch:** recording.md §5 (Note); Szene `RecordingScenes.cpp` (Bild `recording/keyboard`).

## F-609 · UX · Inspector: Eingang, R, M und S ohne Tooltip

- **Status:** offen
- **Code:** `src/ui/fk/ui/Inspector.cpp:69-71` (`armButton ("R")`, `muteButton ("M")`, `soloButton ("S")`), `inputBox` und `outputBox` ohne `setTooltip`.
- **Beobachtung:** Wie F-105 für den Spurkopf. Da der Inspector der einzige Ort für die Eingangswahl ist (F-603), wiegt das schwerer.
- **Erwartung:** Tooltips wie bei F-105; zusammen beheben. Die Szene `RecordingScenes.cpp` kann die Elemente dann über `findByTooltip` finden statt über Typ und Text.
- **Handbuch:** recording.md §3 (Tabelle); Szene `RecordingScenes.cpp` (Hilfsfunktion `findVisible`).

## F-501 · Bug · Menü Track › FX track (sends) tut nichts

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1987` (Eintrag `itemFx`), Enum `:150`; `menuItemSelected` (`:2170-2581`) hat keinen `case itemFx`. Der Toolbar-Button ruft `addFxTrack()` (`:297`, `:1712`).
- **Beobachtung:** Die Wahl von **Track › FX track (sends)** legt keine Spur an und meldet nichts. FX-Spuren lassen sich nur über den Button **FX** anlegen.
- **Erwartung:** `case itemFx: addFxTrack(); break;`
- **Handbuch:** tracks.md §3.6 (Note); project-window.md §3 (Tabelle Track-Menü).

## F-502 · UX · MIDI-Spur an einem Gerät ohne MIDI-Ausgang: leere Ausgangsliste, „MIDI to: Master“

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:164-178` (`getAvailableOutputs` liefert für MIDI-Spuren nur MIDI-Ausgänge), `:430` (`outputName` = „Master“), `src/services/fk/services/ExternalMidiService.cpp:42-48` (ohne MIDI-Ausgang bleibt die Spur am Master), `src/ui/fk/ui/Inspector.cpp:466` („MIDI to: {name}“).
- **Beobachtung:** Ohne eingeschalteten MIDI-Ausgang ist das Feld **Output** leer und der Inspector zeigt „MIDI to: Master“. Die Spur spielt nichts, und kein Hinweis sagt, was zu tun ist.
- **Erwartung:** Eintrag wie „No MIDI output (Studio › Audio and MIDI settings)“ im Feld und in der Beschriftung; neu eingeschaltete Ausgänge eventuell automatisch zuweisen.
- **Handbuch:** tracks.md §5.3; Szene `TracksScenes.cpp` (Bild `tracks/inspector-midi-device`).

## F-503 · Text · Ordner-Tooltip nennt nur Rechtsklick zum Hineinlegen

- **Status:** offen
- **Code:** `src/ui/fk/ui/FolderHeader.cpp:27` („Tracks go in via right-click on the track header.“) gegen `src/ui/fk/ui/ProjectComponent.cpp:204` („drag or right-click tracks into it“) und `src/ui/fk/ui/ArrangeView.cpp:1300-1333` (Ziehen auf die Mitte des Ordnerkopfs legt hinein).
- **Erwartung:** z. B. „Tracks go in by dragging them onto the folder or via right-click on the track header.“
- **Handbuch:** tracks.md §11.2.

## F-504 · UX · Keine Tastenbefehle für Spuren

- **Status:** offen
- **Code:** `src/ui/fk/ui/Commands.cpp:21-80` und `src/ui/fk/ui/Commands.h`: kein Befehl für Spur anlegen, löschen, duplizieren, umbenennen, Mute/Solo/Record der ausgewählten Spur.
- **Beobachtung:** Diese Funktionen gehen nur mit der Maus und lassen sich im Shortcut-Editor nicht belegen. Cubase bietet M/S/R für die ausgewählte Spur und belegbare Befehle zum Anlegen.
- **Erwartung:** Befehle (Kategorie „Track“) mit Standardtasten für Mute/Solo/Record der ausgewählten Spur, ohne Standardtaste für Anlegen/Löschen/Duplizieren.
- **Handbuch:** tracks.md §1 (Note).

## F-505 · UX · Neue Spuren landen am Ende und werden nicht ausgewählt

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:105-108` (`ensureNumberOfAudioTracks` → `TrackInsertPoint::getEndOfTracks`), `:568` (Ordner ebenso); `src/ui/fk/ui/ArrangeView.cpp:1070-1073` (Auswahl bleibt, wenn die alte Spur noch existiert).
- **Beobachtung:** Jede neue Spur hängt ganz unten auf oberster Ebene, auch wenn eine Spur in einem Ordner ausgewählt ist. Der Inspector zeigt weiter die alte Spur.
- **Erwartung:** Wie Cubase: unter der ausgewählten Spur (im selben Ordner) einfügen und auswählen.
- **Handbuch:** tracks.md §3 (Absatz „Selection“).

## F-506 · Bug · Spur anlegen zerfällt in mehrere Undo-Schritte

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:93` („Add track“), danach `:372` → `setAudioInput` mit eigenem `beginUndoStep` „Assign input“ (`:879`); `src/services/fk/services/ExternalMidiService.cpp:30`, `:46` (`setOutput` → „Assign output“, `TrackService.cpp:206`), `:52`; `src/services/fk/services/RackService.cpp:56`, `:160`; `src/services/fk/services/SamplerService.cpp` (Drum-Map und Regionen nach `createInstrumentTrack`); `src/ui/fk/ui/TrackHeader.cpp:396-403` („New folder with this track“: drei Schritte).
- **Beobachtung:** Nach dem Anlegen einer Instrumenten-, Sampler-, Rack- oder externen MIDI-Spur nimmt das erste Ctrl+Z nur die Eingangs- oder Ausgangszuweisung zurück; die Spur bleibt stehen. Der Verlauf zeigt zwei oder drei Schritte für eine Aktion.
- **Erwartung:** Ein Undo-Schritt je Anlegen (interne Aufrufe ohne eigenes `beginUndoStep`, wie `appendAudioTrack`).
- **Handbuch:** tracks.md §3 (Absatz Undo), §15 (Note).

## F-507 · Text · Tooltip des Inspector-Buttons „Inserts“/„Group“ spricht vom Instrument

- **Status:** offen
- **Code:** `src/ui/fk/ui/Inspector.cpp:63-65` (fester Tooltip „Bake Inserts: render the instrument and the marked inserts …“), `:942` (Beschriftung „Group“ bei Gruppen).
- **Beobachtung:** Auf Audiospuren (ohne Instrument) und Gruppenspuren (backen samt zuführender Spuren) beschreibt der Tooltip etwas anderes, als der Button tut.
- **Erwartung:** Tooltip je Spurart in `refreshBake` setzen.
- **Handbuch:** tracks.md §14 (Tabelle der Bake-Buttons).

## F-508 · Unstimmigkeit · Farbmenü des Ordners ohne Haken bei der aktuellen Farbe

- **Status:** offen
- **Code:** `src/ui/fk/ui/FolderHeader.cpp:150-159` (kein `isTicked`) gegen `src/ui/fk/ui/TrackHeader.cpp:314` (`item.isTicked`).
- **Erwartung:** Aktuelle Ordnerfarbe abhaken wie bei Spuren.
- **Handbuch:** tracks.md §11.5.

## F-509 · Unstimmigkeit · „New folder with this track“ heißt nur „Folder“

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:397` (`createFolderTrack (TRANS ("Folder"))`) gegen `src/ui/fk/ui/ProjectComponent.cpp:1703-1708` („Folder {number}“).
- **Beobachtung:** Ordner über Toolbar oder Menü heißen „Folder 1“, „Folder 2“ …; Ordner aus dem Spurmenü alle „Folder“.
- **Erwartung:** Dieselbe Benennung (mit Nummer).
- **Handbuch:** tracks.md §3.7.

## F-510 · Bug · Überschrift „Sends“ im Inspector wird nie angezeigt

- **Status:** offen
- **Code:** `src/ui/fk/ui/Inspector.cpp:175` setzt den Text von `sendsLabel`, `:633-634` gibt ihm Platz und Sichtbarkeit, aber die Liste der Kinder (`:333-348`) enthält `sendsLabel` nicht; es wird nirgends per `addAndMakeVisible` eingefügt.
- **Beobachtung:** Die Send-Zeilen stehen ohne Überschrift direkt unter den Bake-Buttons; über ihnen bleibt eine leere 14-px-Zeile. Beim Schreiben von Kapitel 5 gefunden (Szene fand das Label nicht).
- **Erwartung:** `&sendsLabel` in die Liste der Kinder aufnehmen (`addChildComponent`, Sichtbarkeit regelt `resized`).
- **Handbuch:** tracks.md §5 (Absatz „Sends (7)“); Szene `TracksScenes.cpp` (Bild `tracks/inspector-audio`, Markierung 7 dann mit Überschrift).

## F-1101 · Unstimmigkeit · Zwei „Normalize“ mit verschiedener Wirkung

- **Status:** offen
- **Code:** `src/services/fk/services/ClipService.cpp:430-467` (`normalizeClips`: setzt das Clip-Gain, Datei bleibt, Undo „Normalize“) gegen `:538` und `:555-640` (`processAudio` mit `AudioProcess::normalize`: schreibt eine neue Datei, Undo „Normalize section“, auch ohne Abschnitt); `src/ui/fk/ui/SampleEditor.cpp:31` (Tooltip).
- **Beobachtung:** Im Arrangement ist Normalize zerstörungsfrei über das Clip-Gain, im Sample Editor wird die Datei umgerechnet; das Clip-Gain bleibt dabei stehen. Ein Clip mit +6 dB Clip-Gain spielt nach Normalize im Sample Editor mit Spitze +6 dBFS. Die Undo-Namen „Reverse section“/„Normalize section“ erscheinen auch, wenn der ganze Clip bearbeitet wurde.
- **Erwartung:** Unterschiedliche Namen (z. B. „Normalize file“) oder das Clip-Gain beim Normalisieren im Sample Editor berücksichtigen bzw. auf 0 dB setzen; Undo-Name ohne Abschnitt „… clip“.
- **Handbuch:** sample-editor.md §5.3.

## F-1102 · Bug · Tasten im Sample Editor wirken auf das Arrangement (Entf löscht den Clip)

- **Status:** offen
- **Code:** `src/ui/fk/ui/SampleEditor.h` (kein `keyPressed`, kein Tastaturfokus); Tasten landen in `src/ui/fk/ui/ProjectComponent.cpp:796-804` → `performCommand`, z. B. `:899-907` (Entf löscht ausgewählte Clips oder den Bereich) und `:896-897` (Strg+A wählt alle Clips).
- **Beobachtung:** Wer im Sample Editor einen Abschnitt wählt und Entf drückt, löscht den Clip im Arrangement (der Doppelklick hat ihn ausgewählt). Strg+A wählt alle Clips statt des ganzen Materials. Im Key Editor wirken Entf und Strg+A dagegen im Editor.
- **Erwartung:** Sample Editor nimmt Tastaturfokus: Entf = Stille (oder Abschnitt löschen), Strg+A = ganzen Clip wählen, Esc = Auswahl aufheben; nicht behandelte Tasten weiter an das Projektfenster. Gleiches Muster wie F-901 (Drum-Editor) und F-1002 (List Editor).
- **Handbuch:** sample-editor.md §8 (Absatz „Keyboard“).

## F-1103 · UX · Sample Editor: kein Vorhören, kein Positionszeiger, nur Sekunden

- **Status:** offen
- **Code:** `src/ui/fk/ui/SampleEditor.cpp:221-274` (`paint`: nur Wellenform, Auswahl, Sekundenmarken), keine Verbindung zum Transport.
- **Beobachtung:** Ein Abschnitt lässt sich vor dem Bearbeiten nicht anhören. Kein Positionszeiger, keine Takte/Zählzeiten, die Auswahl rastet weder am Raster noch an Nulldurchgängen ein. Cubase spielt die Auswahl im Sample Editor ab.
- **Erwartung:** Button/Taste „Auswahl abspielen“, Positionszeiger, wahlweise Takte und Zählzeiten, Einrasten an Nulldurchgängen.
- **Handbuch:** sample-editor.md §8.

## F-1104 · Bug · Sample Editor passt die Ansicht nicht an, wenn sich die Clip-Länge ändert

- **Status:** offen (laut Code, im Lauf noch nicht geprüft)
- **Code:** `src/ui/fk/ui/SampleEditor.cpp:84` (`zoomToFit` nur in `setClip`), `:107-151` (`refresh` lässt `visibleStart`/`visibleLength` stehen), `:248-255`.
- **Beobachtung:** Wird der Clip im Arrangement gekürzt, während er im Sample Editor geladen ist, zeigt die Ansicht weiter die alte Länge; die Wellenform reicht über das Clip-Ende hinaus. Nach dem Verlängern ist nur ein Teil sichtbar, bis man „Show the whole clip“ klickt.
- **Erwartung:** In `refresh` bei geänderter Länge Ansicht begrenzen bzw. `zoomToFit`, Auswahl auf die neue Länge kürzen.
- **Handbuch:** sample-editor.md §3 (Tip).

## F-1105 · Bug · Bearbeitung eines Clips mit Takes löst die Take-Zuordnung

- **Status:** offen (Anzeige-Teil laut Code, im Lauf noch nicht geprüft)
- **Code:** `src/services/fk/services/ClipService.cpp:637` (`getSourceFileReference().setToFile` auf die neue Datei, Takes-Liste unverändert); Tracktion `tracktion_WaveAudioClip.cpp:243-262` (`getCurrentTake` sucht die Quelle in der Takes-Liste); `src/services/fk/services/TrackService.cpp:487-488`.
- **Beobachtung:** Die bearbeitete Datei wird kein Take. Wählt man danach einen Take, ist die Bearbeitung weg. Die Take-Anzeige im Arrangement zeigt danach vermutlich keinen oder einen falschen aktuellen Take.
- **Erwartung:** Bei Clips mit Takes die Datei des aktuellen Takes in der Takes-Liste ersetzen (im selben Undo-Schritt) oder die Bearbeitung als neuen Take anlegen.
- **Handbuch:** sample-editor.md §6 (Note zu Takes).

## F-1106 · Text · Tooltips der Bearbeitungen im Sample Editor uneinheitlich

- **Status:** offen
- **Code:** `src/ui/fk/ui/SampleEditor.cpp:29-35`.
- **Beobachtung:** Nur „Silence: … (or the whole clip)“ sagt, dass ohne Auswahl der ganze Clip bearbeitet wird. „Amplify or attenuate (selection in dB)“ ist unklar. „Invert phase“ erwähnt den Abschnitt nicht, wirkt aber nur auf ihn.
- **Erwartung:** Einheitlich, z. B. „Amplify the section (whole clip without a selection)“, „Invert the phase of the section (…)“.
- **Handbuch:** sample-editor.md §2 (Tooltip-Tabelle); Szene `SampleEditorScenes.cpp` sucht Buttons über die Tooltip-Anfänge.

## F-1107 · UX · Wellenform im Sample Editor zeigt nicht, was man hört

- **Status:** offen
- **Code:** `src/ui/fk/ui/SampleEditor.cpp:221-274` (`paint` zeichnet nur die Datei ab `offsetSeconds`; `gainDb`, `fadeInSeconds`, `fadeOutSeconds`, `reversed` aus `ClipInfo` bleiben unberücksichtigt).
- **Beobachtung:** Clip-Gain, die Fades der Clip-Griffe und „Reverse“ aus dem Arrangement sind im Sample Editor unsichtbar. Ein rückwärts laufender Clip wird vorwärts gezeichnet.
- **Erwartung:** Clip-Fades einzeichnen, Clip-Gain in der Höhe berücksichtigen oder in der Infozeile nennen, rückwärts gespiegelt zeichnen.
- **Handbuch:** sample-editor.md §2 (Absatz Waveform), §5.4 (Note); Bild `sample-editor/locked`.

## F-901 · Bug · Entf/Rücktaste im Drum-Editor löschen Clips im Arrangement

- **Status:** offen (aus dem Code abgeleitet)
- **Code:** `src/ui/fk/ui/DrumEditor.cpp` (weder `DrumGrid` noch `DrumEditor` rufen `setWantsKeyboardFocus`, kein `keyPressed`) gegen `src/ui/fk/ui/NoteGrid.cpp:36`; Tasten landen in `src/ui/fk/ui/ProjectComponent.cpp:796-801` → `:899-907` (`deleteSelection` löscht die ausgewählten Clips).
- **Beobachtung:** Wer wie im Key Editor einen Schlag mit Entf löschen will, löscht die im Arrangement ausgewählten Clips; nach dem Doppelklick meist genau den bearbeiteten Clip.
- **Erwartung:** Drum-Raster nimmt den Fokus und behandelt Entf/Rücktaste selbst (mit Auswahl, siehe F-911) oder schluckt die Tasten. Gleiches Muster wie F-1002 und F-1102: zusammen lösen.
- **Handbuch:** drum-editor.md §6 (Caution), §13.

## F-902 · Bug · Vorhören im Drum-Editor beendet Noten nie

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:386-390` und `:408-412` rufen `preview.startNote`, nirgends `preview.stopAll()`; der Key Editor beendet beim Loslassen (`src/ui/fk/ui/KeyEditor.cpp:224-238`). `NotePreviewService::startNote` sendet Note-Off erst, wenn dieselbe Note neu angeschlagen wird.
- **Beobachtung:** Bei Instrumenten mit gehaltenem Klang klingt die vorgehörte Note weiter, bis dieselbe Zeile erneut angespielt wird.
- **Erwartung:** `mouseUp` in `DrumGrid` und `DrumNames` ruft `stopAll` wie im Key Editor.
- **Handbuch:** drum-editor.md §10 (Note).

## F-903 · Bug · Klick neben einen Schlag setzt einen zweiten auf dieselbe Stelle

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:104-123` (`hitAt` trifft nur bis 8 px Abstand), `:197-208` (sonst neuer Schlag), `:99-102` (`snapped` rundet zum nächsten Rasterschritt).
- **Beobachtung:** Ein Klick 9 px oder weiter neben einer Raute, der zum selben Rasterschritt gerundet wird, legt eine zweite, unsichtbare Note genau auf die erste; ein Klick auf die Raute löscht nur eine.
- **Erwartung:** Vor dem Setzen prüfen, ob auf Note und Rasterschritt schon ein Schlag liegt; dann löschen oder nichts tun.
- **Handbuch:** drum-editor.md §5 (Note).

## F-904 · UX · Drum-Map lässt sich Spuren ohne automatische Map nicht zuordnen

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:484-486` (Map-Liste nur mit geladenem Clip), `src/ui/fk/ui/ProjectComponent.cpp:1767-1782` (`openMidiClip`: ohne Map in den Key Editor), `src/services/fk/services/DrumMapService.cpp:275-287` (automatisch nur „FK Drums“ bzw. Namen mit drum/kit/percussion/schlagzeug). Weder Inspector noch Key Editor haben eine Map-Auswahl.
- **Beobachtung:** FK-Synth-Spuren, externe MIDI-Spuren (Drumcomputer) oder Schlagzeug-Plugins mit anderem Namen öffnen ihre Clips immer im Key Editor; eine Map lässt sich nicht wählen. Der Hinweis des leeren Drum-Editors verspricht „otherwise choose one here“.
- **Erwartung:** Drum-Map im Inspector (Instrumenten- und MIDI-Spuren) oder im Spurmenü wählbar; Map-Liste auch ohne Clip für die ausgewählte Spur.
- **Handbuch:** drum-editor.md §1 (Note), §3.1 (Note).

## F-905 · UX · Eigene Drum-Maps: Import, Export und Löschen ohne Bedienung

- **Status:** offen
- **Code:** `src/services/fk/services/DrumMapService.h:59-61` (`importMap`, `exportMap`, `deleteMap`, nur in Tests aufgerufen); `src/services/fk/services/DrumMapService.cpp:244-250` (ungültige Dateien still übersprungen), `:344` (Standardname „Drum-Map“).
- **Beobachtung:** Eigene Maps gehen nur, indem man JSON von Hand in den Ordner `DrumMaps` legt. Fehlerhafte Dateien fehlen ohne Meldung. Der Ordner wird erst von `importMap` angelegt.
- **Erwartung:** Einträge „Import drum map…“, „Export…“, „Delete“; Meldung bei ungültiger Datei; Standardname englisch („Drum map“).
- **Handbuch:** drum-editor.md §3.4.

## F-906 · Unstimmigkeit · Spur merkt sich bei Drum-Maps nur die Kennung, bei Expression Maps eine Kopie

- **Status:** offen
- **Code:** `src/services/fk/services/DrumMapService.cpp:247` (Kennung „user:“ + Dateiname), `:266-273` (unbekannte Kennung → still automatisch) gegen `src/services/fk/services/ExpressionMapService.h:16`, `:33` (Kopie in der Spur).
- **Beobachtung:** Auf einem anderen Rechner oder nach Umbenennen der Datei zeigt die Spur still eine andere Map (oder keine), und Clips öffnen womöglich im Key Editor.
- **Erwartung:** Wie bei Expression Maps eine Kopie der Map in der Spur speichern.
- **Handbuch:** drum-editor.md §3.2 (Note).

## F-907 · Bug · Undo von Drum-Map- und Sample-Zuordnung

- **Status:** offen (aus dem Code, im Lauf noch nicht geprüft)
- **Code:** `src/services/fk/services/MidiClipService.cpp:625-652` (Neuladen nur bei Änderungen in MIDI-Clips, nicht an Spur-Eigenschaften `drumMap`/`fkSamplerRegions`); `src/services/fk/services/SamplerService.cpp:59-73` (`storeRegions` mit Undo) gegen `:115-140` (`writeAndLoad`: SFZ-Datei und Laden ins Plugin ohne Undo, kein Listener auf `samplerRegions`).
- **Beobachtung:** Nach Ctrl+Z von „Assign drum map“ oder „Assign sample“ zeigt der Drum-Editor bis zum nächsten Neuladen den alten Stand. Nach Undo einer Sample-Zuweisung spielt FK Sampler vermutlich weiter das neue Sample.
- **Erwartung:** Drum-Editor hört auf Spur-Eigenschaften; SFZ bei Änderung von `samplerRegions` (auch durch Undo) neu schreiben und laden.
- **Handbuch:** drum-editor.md §14 (Note).

## F-908 · UX · Sample lässt sich nicht von einer Drum-Zeile entfernen

- **Status:** offen
- **Code:** `src/services/fk/services/SamplerService.h:41` (`removeSample`, nur in Tests aufgerufen), `src/ui/fk/ui/DrumEditor.cpp` (kein Kontextmenü der Namensspalte).
- **Beobachtung:** Ein falsch abgelegtes Sample lässt sich nur ersetzen, nicht entfernen.
- **Erwartung:** Rechtsklick auf den Namen: „Remove sample“ (Undo-Schritt „Remove sample“ gibt es schon).
- **Handbuch:** drum-editor.md §11 (Liste „What to know“).

## F-909 · Text · Menüpfad „Track > Sampler Track …“ in Hinweis und Fehlermeldung

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:421` („Track > Sampler Track (empty drum kit)“), `src/services/fk/services/SamplerService.cpp:233` („Track > Sampler track creates one.“) gegen das Menü `src/ui/fk/ui/ProjectComponent.cpp:1984-1985` („Sampler track from audio file...“, „Sampler track (empty drum kit)“).
- **Erwartung:** Pfad mit „›“ und Schreibweise wie im Menü, z. B. „Track › Sampler track (empty drum kit)“.
- **Handbuch:** drum-editor.md §1 (Bild), §11, §14; Bild `drum-editor/empty`.

## F-910 · Unstimmigkeit · Raster, Einrasten, Symbole und Quantisieren anders als im Key Editor

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:352-362` (Raster als flache Liste mit 24 Einträgen) gegen `src/ui/fk/ui/KeyEditor.cpp:629-654` (Untermenüs Dotted/Triplet); `DrumEditor.cpp:99-102` (rundet zum nächsten Schritt) gegen `src/ui/fk/ui/NoteGrid.cpp:511-513` (Schritt links der Maus); `DrumEditor.cpp:323` (Audition mit `Icon::keyboard`, im Key Editor das Symbol für Step) gegen `KeyEditor.cpp:121` (`Icon::speaker`); `DrumEditor.cpp:322` (Quantize nur Symbol) gegen Key Editor mit Beschriftung; `DrumEditor.cpp:375` (Quantisieren ohne Stärke/Swing/Groove).
- **Beobachtung:** Wer zwischen den Editoren wechselt, findet dieselben Funktionen anders benannt, gezeichnet und mit anderem Verhalten.
- **Erwartung:** Gemeinsame Rasterliste (`fillNoteValueBox`), gleiches Einrasten, Lautsprecher-Symbol, beschrifteter Quantize-Button mit denselben Optionen.
- **Handbuch:** drum-editor.md §2, §5, §8; Bild `drum-editor/overview`.

## F-911 · UX · Drum-Editor ohne Auswahl, Verschieben, Kopieren und Anschlagsanzeige

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:187-245` (nur Setzen, Löschen, senkrechtes Ziehen; 1 px = 1 Anschlagsstufe ohne Zahl) gegen Key Editor (Auswahl, Verschieben, Alt-Kopie, Pfeiltasten, Velocity-Lane, Wertanzeige, 2 px je Stufe).
- **Beobachtung:** Schläge lassen sich nicht verschieben, kopieren oder gemeinsam ändern; der Anschlag ist beim Ziehen nur an der Rautengröße erkennbar. Weniger als 3 px Bewegung löscht den Schlag.
- **Erwartung:** Auswahl (Rechteck, Ctrl+A), Verschieben/Kopieren auf dem Raster, Tasten wie im Key Editor, Velocity-Lane oder Wertanzeige beim Ziehen.
- **Handbuch:** drum-editor.md §7, §13.

## F-912 · UX · Mausrad über der Namensspalte des Drum-Editors rollt nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/DrumEditor.cpp:570-579` (ohne Ctrl an die Eltern weitergereicht), `DrumNames` ohne `mouseWheelMove`; nur der Viewport des Rasters rollt.
- **Erwartung:** Mausrad über den Namen rollt die Zeilen wie über dem Raster.
- **Handbuch:** drum-editor.md §12.

## F-1001 · Bug · Klick auf einen Wert wählt die Zeile im List Editor nicht aus

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp:278`: jede Zelle ist ein `CellLabel` mit `setInterceptsMouseClicks (true, true)`. Die Maustasten erreichen die Zeilenkomponente von JUCE (`TableListBox::RowComp`, Auswahl in `mouseDown/mouseUp`) dann nicht. Auswählen geht nur rechts neben der letzten Spalte oder per Tastatur.
- **Erwartung:** Klick (mit Shift/Ctrl) auf eine Zelle wählt die Zeile aus, Doppelklick bearbeitet; z. B. Klicks im Label an `table.selectRowsBasedOnModifierKeys` weitergeben oder `cellClicked` nutzen.
- **Handbuch:** list-editor.md §4 (Hinweis „Important“), Tabelle der Auswahlaktionen, Kurzreferenz.

## F-1002 · Bug · Entf/Rücktaste im List Editor ohne ausgewählte Zeile löscht Clips im Arrangement

- **Status:** offen
- **Code:** JUCEs `ListBox::keyPressed` behandelt Delete/Backspace nur, wenn eine Zeile ausgewählt ist, sonst `false`. Die Taste wandert zu `src/ui/fk/ui/ProjectComponent.cpp:800` (`keyBindings.find`) und dort zu `Command::deleteSelection` (`:899`), das die im Arrangement ausgewählten Clips oder den Bereich löscht. Nach dem Doppelklick zum Öffnen ist meist genau der bearbeitete Clip ausgewählt.
- **Erwartung:** Der List Editor verbraucht Delete/Backspace immer (ohne Auswahl wirkungslos), z. B. `keyPressed` im ListEditor überschreiben. Gleiches Muster wie F-901 (Drum-Editor) und F-1102 (Sample Editor), am besten zusammen lösen: Editoren der unteren Zone geben Bearbeitungstasten nie ans Arrangement weiter.
- **Handbuch:** list-editor.md §6 (Hinweis „Caution“).

## F-1003 · Bug · Auswahl im List Editor hängt an Zeilennummern statt an Noten

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp` `reload()` (ab Zeile 70) sortiert neu und ruft `table.updateContent()`; `ListBox` behält dabei die ausgewählten Zeilennummern. `setClip()` (`:64`) leert die Auswahl nicht.
- **Beobachtung:** Nach einer Positionsänderung, nach Änderungen in einem anderen Editor oder nach dem Öffnen eines anderen Clips markiert die Auswahl andere Noten. Löschen trifft dann die falschen Noten.
- **Erwartung:** Auswahl über die Noten merken (Index/Inhalt) und nach `reload()` wiederherstellen; beim Clipwechsel leeren.
- **Handbuch:** list-editor.md §4 (Hinweis „Note“).

## F-1004 · UX · Spaltenköpfe des List Editors zeigen Sortierpfeil ohne Wirkung, „Auto-size“ tut nichts

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp:37-40` legt die Spalten mit den JUCE-Vorgaben an (`defaultFlags` inkl. `sortable`); `sortOrderChanged` und `getColumnAutoSizeWidth` sind nicht überschrieben.
- **Beobachtung:** Ein Klick auf den Kopf zeigt einen Sortierpfeil, die Liste bleibt nach Position sortiert. „Auto-size this column“/„Auto-size all columns“ ändern nichts. Spaltenbreite, Reihenfolge und ausgeblendete Spalten werden nicht gespeichert.
- **Erwartung:** Sortieren umsetzen oder `sortable` abschalten; Auto-size umsetzen oder abschalten (`setAutoSizeMenuOptionShown (false)`).
- **Handbuch:** list-editor.md §8 (Hinweis „Note“).

## F-1005 · UX · List Editor zeigt nur Noten, kann nichts einfügen und teilt die Auswahl nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp`/`.h`: Spalten nur für Noten. `MidiClipInfo::controllers` (`src/services/fk/services/MidiClipService.h:79`) und die Artikulation (`EditableNote::articulation`) werden nicht angezeigt. Kein Einfügen, kein Filter, kein Abgleich der Auswahl mit Key/Drum Editor.
- **Erwartung:** Controller/Pitch-Bend/Aftertouch als Zeilen mit Typspalte, Einfügen-Button, Filter nach Ereignistyp, Auswahl mit dem geöffneten Editor gekoppelt.
- **Handbuch:** list-editor.md §3 (erster Absatz), §7, §9; project-window.md §8 (Tabelle der Reiter).

## F-1006 · UX · Ungültige Eingaben im List Editor werden stumm verworfen, Länge liest nur die führende Zahl

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp:272-276` (bei `editCell == false` alter Text ohne Meldung); Länge `:197` per `getDoubleValue()` („1/8“ ergibt 1 Schlag, „2x“ ergibt 2); Anschlag `:210` klemmt 0 auf 1 und >127 auf 127 ohne Hinweis; Position (`TimelineService::parseBarsBeats`) prüft die Schlagzahl nicht gegen die Taktlänge.
- **Erwartung:** Kurze Meldung oder rote Zelle bei ungültiger Eingabe; Länge streng prüfen oder Brüche („1/8“, „1/8.“, „1/8T“) wie im Key Editor als Notenwerte verstehen.
- **Handbuch:** list-editor.md §5 („Invalid input …“ und Hinweis unter „Length (beats)“).

## F-1007 · Unstimmigkeit · Undo-Name und Kopfzeile des List Editors weichen vom Key Editor ab

- **Status:** offen
- **Code:** `src/ui/fk/ui/ListEditor.cpp:217` (jede Zellenänderung heißt „List Editor“, der Key Editor benennt die Aktion); Kopfzeile `:89` „{name} · {notes}“ gegen `src/ui/fk/ui/KeyEditor.cpp:548` „{name}  ({notes})“.
- **Erwartung:** Undo-Namen nach Spalte („Move note“, „Transpose note“, „Change note length“, „Change velocity“); eine gemeinsame Form der Kopfzeile.
- **Handbuch:** list-editor.md §11 (erster Absatz), §2 (Tabelle Zeile 1).

## F-1201 · Bug · Tonart-Blase zeigt beim neuen Tonartwechsel die Tonart eines späteren Wechsels

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `showKeyChooser`: nach `signature.insertKey` wird `keyPoints[index]` gelesen; `keyPoints` wird erst asynchron aktualisiert (`SignatureService` meldet über `AsyncUpdater`). Gibt es einen späteren Wechsel, steht an `index` noch dieser.
- **Beobachtung:** Die Blase startet mit Grundton und Skala des späteren Wechsels. Ändert man nur eine Liste, wird der falsche Wert der anderen mit übernommen.
- **Erwartung:** Werte direkt vom Service holen (`getKeyPoints()[index]`), nicht aus der zwischengespeicherten Liste.
- **Handbuch:** signature-track.md §6 (Inserting a key change).

## F-1202 · Bug · Tempo über 300 BPM wird still auf 300 begrenzt, Code nennt 400

- **Status:** offen
- **Code:** `src/services/fk/services/SignatureService.cpp:12` und `src/services/fk/services/TimelineService.cpp:14` (`maxTempoBpm = 400.0`); Tracktion `TempoSetting::set` begrenzt auf `maxBPM = 300.0` (`tracktion_TempoSetting.h:45`).
- **Beobachtung:** 350 in der Transportleiste ergibt 300 BPM, ohne Meldung.
- **Erwartung:** Grenze einheitlich 300 (Konstante aus `te::TempoSetting::maxBPM`), Hinweis bei Begrenzung.
- **Handbuch:** signature-track.md §3.7, Quick reference; transport.md §12.

## F-1203 · Bug · Tempopunkt waagerecht ziehen ändert auch das Tempo

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `mouseDrag`: `tempoDrag->bpm = bpmForY (event.position.y)` bei jeder Bewegung; Treffer in 8 px Umkreis, die Lage hat nur 10 px nutzbare Höhe für 40–200 BPM.
- **Beobachtung:** Wer einen Punkt seitlich verschiebt, ändert fast immer das Tempo, wenige Pixel neben der Mitte um Dutzende BPM (Punkt 90 BPM, Griff 1 px darunter ergibt etwa 72 BPM).
- **Erwartung:** Tempo erst bei senkrechtem Ziehen ändern (Schwelle oder Achsensperre) oder relativ zum Startpunkt rechnen.
- **Handbuch:** signature-track.md §3.4.

## F-1204 · UX · Tempowerte der Punkte nicht eintippbar, Ziehen sehr grob

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` (`laneHeight` = 18, `bpmForY`, `refresh`: Bereich 40–200 BPM ± 20 um die Punkte).
- **Beobachtung:** Tempo eines Punkts nur per Ziehen, rund 16 BPM je Pixel; über den sichtbaren Bereich hinaus nur in Schritten von 20 BPM. Exakte Werte gibt es nur für den ersten Punkt über die Transportleiste.
- **Erwartung:** Doppelklick oder Kontextmenü mit BPM-Eingabe, feineres Ziehen (z. B. Shift = 0,1 BPM, relativ) oder eine höhere Tempolage.
- **Handbuch:** signature-track.md §3.3.

## F-1205 · Unstimmigkeit · Drei verschiedene Regeln für Taktarten

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `commonSignatures()` (nur 4/4, 3/4, 2/4, 6/8, 5/4, 7/8, 12/8, 9/8); `src/services/fk/services/TimelineService.cpp` `setTimeSignature` (1–32, Nenner 2/4/8/16, nur Takt 1); `src/services/fk/services/SignatureService.cpp` `insertTimeSignature`/`setTimeSignature` (Zähler und Nenner je 1–32, also auch 5/3).
- **Beobachtung:** Taktartwechsel im Song gehen nur mit den acht Menüeinträgen; 5/8, 7/4 oder 2/2 lassen sich nur am Projektanfang eintippen. Der Service nimmt Nenner an, die die Transportleiste ablehnt.
- **Erwartung:** Menü um „Other…“ mit Eingabe erweitern, eine gemeinsame Prüfung im Service.
- **Handbuch:** signature-track.md §5.

## F-1206 · Unstimmigkeit · Tempopunkt verschieben oder entfernen lässt Clips auf ihrer Sekunde

- **Status:** offen
- **Code:** `src/services/fk/services/SignatureService.cpp` `moveTempoPoint` (`moveTempoStart (…, false)`) und `removeTempoPoint` (`removeTempo (index, false)`); `setTempoPoint` nutzt `setBpm`/`setCurve`, die den Edit umrechnen.
- **Beobachtung:** Tempo oder Sprung/Rampe ändern hält Clips, Automation, Marker und Locatoren im Takt. Einen Punkt verschieben oder löschen lässt sie an ihrer Zeit; danach liegen sie auf anderen Takten.
- **Erwartung:** Auch beim Verschieben und Entfernen umrechnen (`removeTempo (index, true)` bzw. `EditTimecodeRemapperSnapshot` um `moveTempoStart`).
- **Handbuch:** signature-track.md §4 (Note).

## F-1207 · UX · Taktart- und Tonartwechsel lassen sich nicht verschieben

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` (Ziehen nur für Tempopunkte); `SignatureService::moveKey` existiert, wird nicht benutzt; für Taktarten kein `moveTimeSignature` (Tracktion hat `moveTimeSigStart`).
- **Beobachtung:** Um einen Wechsel zu versetzen, muss man ihn löschen und neu setzen.
- **Erwartung:** Wechsel waagerecht ziehen (Taktart taktweise, Tonart am Raster), ein Undo-Schritt.
- **Handbuch:** signature-track.md §5 und §6 („Moving“).

## F-1208 · UX · Klick in die Tonart-Lage legt sofort einen Wechsel an

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `showKeyChooser`: `insertKey` vor dem Öffnen der Blase.
- **Beobachtung:** Ein Klick erzeugt einen Tonartwechsel mit der bisherigen Tonart, auch wenn man die Blase ohne Auswahl schließt; jeder neue Wechsel braucht mindestens zwei Undo-Schritte. Das Taktartmenü legt erst bei Auswahl an.
- **Erwartung:** Wechsel erst bei der ersten Auswahl anlegen (oder beim Schließen ohne Änderung entfernen), Änderungen in der Blase zu einem Schritt zusammenfassen.
- **Handbuch:** signature-track.md §6, §9.

## F-1209 · Unstimmigkeit · Key Editor kennt nur Taktart des Projektanfangs und Tonart am Clipanfang

- **Status:** offen
- **Code:** `src/ui/fk/ui/NoteGrid.cpp:175` und `:526` (`timeline.getTimeSignature()` = erste Taktart, für Taktlinien und Akkordlänge); `src/ui/fk/ui/KeyEditor.cpp:563` (`getKeyAt (clip->startSeconds)`).
- **Beobachtung:** Nach einem Taktartwechsel stimmen die Taktlinien im Key Editor nicht mit dem Arrangement überein, und Akkorde werden einen Takt der Anfangstaktart lang. Ein Tonartwechsel mitten im Clip ändert weder Abdunklung noch Akkordauswahl.
- **Erwartung:** Taktlinien aus der Signaturspur, Tonart je Position.
- **Handbuch:** signature-track.md §5 (Note) und §6 (Note); key-editor.md §2/§15 (nach dem Fix ergänzen).

## F-1210 · Bug · Zweiter Tempopunkt auf demselben Schlag

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `mouseDown` (Treffer nur in 8 px Umkreis, sonst `insertTempoPoint`); Tracktion `TempoSequence::insertTempo` rundet auf den Schlag und prüft nicht auf einen vorhandenen Punkt.
- **Beobachtung:** Ein Klick neben einen Punkt, der auf denselben Schlag gerundet wird, legt einen zweiten Punkt genau darauf; einer verdeckt den anderen.
- **Erwartung:** Liegt am gerundeten Schlag schon ein Punkt, diesen greifen statt neu anlegen (Prüfung in `SignatureService::insertTempoPoint`). Gleiches Muster wie F-903 (Drum-Editor).
- **Handbuch:** signature-track.md §3.2.

## F-1211 · UX · Tempopunkt in Zeit und Tempo ziehen ergibt zwei Undo-Schritte

- **Status:** offen
- **Code:** `src/ui/fk/ui/SignatureTrack.cpp` `mouseUp`: `setTempoPoint` („Change tempo“) und danach `moveTempoPoint` („Move tempo point“), jeder mit eigenem `beginUndoStep`.
- **Beobachtung:** Eine Mausbewegung braucht zweimal Undo; durch F-1203 trifft das fast jedes seitliche Ziehen.
- **Erwartung:** Ein Schritt je Ziehen.
- **Handbuch:** signature-track.md §9.

## F-1301 · UX · Automationspunkte nur einzeln bearbeitbar, kein „Alle löschen“, keine Werteingabe

- **Status:** offen
- **Code:** `src/services/fk/services/AutomationService.cpp:310-318` (`clearPoints`, Undo „Delete automation“) wird nirgends in der UI aufgerufen; `src/ui/fk/ui/AutomationLane.cpp:177-254` kennt nur Einzelpunkte (Klick, Ziehen, Doppel-/Rechtsklick); Wert nur beim Ziehen (`:152`).
- **Beobachtung:** Eine Kurve lässt sich nur Punkt für Punkt löschen. Punkte lassen sich nicht auswählen, verschieben, kopieren oder einfügen, auch nicht über einen Bereich. Den Wert sieht man nur beim Ziehen; eintippen geht nicht. „−“ blendet nur die Lage aus.
- **Erwartung:** Kontextmenü der Lage mit „Delete automation“, Mehrfachauswahl per Rechteck mit Entf/Verschieben/Kopieren, Wert als Tooltip beim Überfahren, Doppelklick auf einen Punkt öffnet die Werteingabe (Löschen bleibt Rechtsklick).
- **Handbuch:** automation.md §4, §5, §6.

## F-1302 · Unstimmigkeit · Kurvenform nicht einstellbar, Lage zeichnet immer gerade Linien

- **Status:** offen
- **Code:** `src/services/fk/services/AutomationService.cpp:264` (`addPoint` setzt `curve` immer 0); `src/ui/fk/ui/AutomationLane.cpp:120` (`lineTo` ohne `point.curve`); Tracktion schreibt bei Aufnahme diskreter Parameter `curve = 1.0` (`tracktion_AutomationRecordManager.cpp:326`, `:491`).
- **Beobachtung:** `AutomationPointInfo::curve` wird geliefert, aber nie gezeichnet oder angeboten. Aufgenommene Kurven diskreter Parameter springen im Klang, die Lage zeigt eine Rampe.
- **Erwartung:** Kurve gemäß `curve` zeichnen (Stufe/Biegung); Biegung z. B. per Alt+Ziehen auf der Linie einstellbar.
- **Handbuch:** automation.md §5.

## F-1303 · Unstimmigkeit · Automation folgt Clip- und Bereichsbearbeitung nur teilweise

- **Status:** offen
- **Code:** `src/services/fk/services/ClipService.cpp:662-690` (`deleteRange`: entfernt nur Clips, „close gap“ verschiebt nur Clips) gegen `:693-718` (`insertSilence` über `te::insertSpaceIntoEdit`, das auch die Kurven verschiebt); Clip verschieben/kopieren fasst Automation nicht an.
- **Beobachtung:** Nach **Remove (close gap)** passt die Automation ab dem Bereich nicht mehr zu den Clips; **Insert silence** verschiebt sie dagegen mit. **Delete (leave gap)** lässt die Kurve im Bereich stehen.
- **Erwartung:** Bereichsfunktionen behandeln Kurven der abgedeckten Spuren wie Clips; optional „Automation folgt Clips“ wie in Cubase.
- **Handbuch:** automation.md §11 (Tabelle, Tip).

## F-1304 · Bug · Duplizierte Spur: Automationslagen zeigen keinen Parameter

- **Status:** offen (aus dem Code abgeleitet, im Lauf noch nicht geprüft)
- **Code:** `src/services/fk/services/TrackService.cpp:801` (`edit->copyTrack` → `EditItemID::remapIDs` vergibt neue Plugin-IDs); die Lagen stehen als „<Plugin-ID>/<Parameter-ID>“ in `fkAutomationLanes` (`src/services/fk/services/AutomationService.cpp:75`, `:152`) und werden nicht umgeschrieben.
- **Beobachtung:** Die Kopie hat Kurven und Modus, ihre Lagen verweisen aber auf die Plugins der Originalspur und zeigen **No parameter**.
- **Erwartung:** Nach `copyTrack` die IDs in `fkAutomationLanes` über die Zuordnung aus `remapIDs` umschreiben.
- **Handbuch:** automation.md §11 (Note zur duplizierten Spur).

## F-1305 · UX · Automationsmodus nur in der ersten Lage und nur bei aufgeklappten Lagen sichtbar

- **Status:** offen
- **Code:** `src/ui/fk/ui/AutomationHeader.cpp:52` (`modeBox` nur bei `laneIndex == 0`); weder `TrackHeader` noch Inspector zeigen den Modus.
- **Beobachtung:** Ob eine Spur auf Touch/Latch/Write steht (und bei eingeschaltetem Schreiben aufnimmt), sieht man nur bei aufgeklappten Lagen.
- **Erwartung:** Modus-Anzeige im Spurkopf (z. B. Farbe/Buchstabe am Automation-Button) oder im Inspector; Moduswahl auch ohne aufgeklappte Lagen.
- **Handbuch:** automation.md §2, §7; Szene `AutomationScenes.cpp` (Bild `automation/overview`).

## F-1306 · Text · Tooltips sagen „automation track“, Verlauf und Handbuch „automation lane“

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:28` („Automation track on/off“), `src/ui/fk/ui/AutomationHeader.cpp:22`, `:23`, `:25` gegen Undo-Namen „Add automation lane“, „Remove automation lane“ (`AutomationService` `addLane`/`removeLane`).
- **Erwartung:** einheitlich „automation lane“ („Automation lanes on/off“, „Add an automation lane“, „Remove this automation lane“, „Parameter shown in this lane“).
- **Handbuch:** automation.md §2; tracks.md §4. Szenen `AutomationScenes.cpp` und `TracksScenes.cpp` suchen über diese Tooltip-Anfänge und sind mitzuziehen.

## F-1307 · UX · Write springt nach einem Durchgang still auf Latch

- **Status:** offen
- **Code:** `tracktion_AutomationRecordManager.cpp:337-339` (`punchOut`: write → latch); FK übernimmt das ohne Hinweis.
- **Beobachtung:** Nach einem Write-Durchgang steht die Spur auf **Latch**; der Tooltip („Write: writes from play start“) erwähnt das nicht.
- **Erwartung:** Write beibehalten (wie Cubase) oder im Tooltip klar sagen; mindestens Hinweis beim Umschalten.
- **Handbuch:** automation.md §7.

## F-1308 · Unstimmigkeit · Automationslage bedient sich anders als die Controller-Lanes im Key Editor

- **Status:** offen
- **Code:** `src/ui/fk/ui/AutomationLane.cpp:177-234` gegen `src/ui/fk/ui/ControllerLane.cpp:247-330`.
- **Beobachtung:** Controller-Lane: Ziehen zeichnet eine Punktreihe je Rasterschritt, Klick setzt am Rasterschritt links der Maus, Raster immer aktiv, Wert links, Doppelklick tut nichts. Automationslage: Ziehen setzt einen Punkt und verschiebt ihn, Raster nur mit Snap und zur nächsten Linie, Wert neben dem Punkt, Doppelklick löscht. Doppelklick auf leere Fläche setzt und löscht gleich wieder (zwei Verlaufseinträge).
- **Erwartung:** gemeinsame Bedienung beider Lanes; Doppelklick auf leere Fläche ohne Effekt.
- **Handbuch:** automation.md §6.

## F-1309 · Text · Send-Parameter tragen den alten Namen der FX-Spur

- **Status:** offen
- **Code:** `src/services/fk/services/SendService.cpp:44` (`setAuxBusName` nur beim Anlegen); Tracktion benennt den Send „S:<Busname>“ (`tracktion_AuxSend.cpp:62-70`); `AutomationService::infoFor` nutzt `plugin->getName()`.
- **Beobachtung:** In der Parameterliste heißt ein Send z. B. **S:FX 1: Send level**. Nach Umbenennen der FX-Spur bleibt der alte Name; „S:“ ist unklar.
- **Erwartung:** Busnamen beim Umbenennen nachführen und z. B. „Send to FX 1: Level“ anzeigen.
- **Handbuch:** automation.md §1.

## F-1310 · UX · Automationsaufnahme ohne benannten Undo-Schritt

- **Status:** offen (aus dem Code abgeleitet, im Lauf noch nicht geprüft)
- **Code:** Weder `src/services/fk/services/TransportService.cpp:29-33` (`play`) noch `AutomationService` beginnen vor dem Schreiben einen Undo-Schritt; Tracktion schreibt in die gerade offene Transaktion.
- **Beobachtung:** Aufgenommene Automation landet im zuletzt begonnenen Schritt oder als „Change“; Undo kann mehr oder weniger entfernen als erwartet. Verwandt mit F-604.
- **Erwartung:** Bei Play-Start mit eingeschaltetem Schreiben (oder beim ersten Punch-in) einen Schritt „Record automation“ beginnen.
- **Handbuch:** automation.md §9, §12.

## F-1401 · Unstimmigkeit · Kanalzug zeigt vier Insert-Slots, Inspector acht

- **Status:** offen
- **Code:** `src/ui/fk/ui/ChannelStrip.h:25` (`numInsertSlots = 4`) gegen `src/ui/fk/ui/Inspector.h:35` (`numInsertSlots = 8`); `ChannelStrip::refresh` füllt nur die ersten vier.
- **Beobachtung:** Inserts 5–8 arbeiten, sind in der MixConsole aber unsichtbar und nur im Inspector bedienbar.
- **Erwartung:** gleiche Slotzahl (8) im Kanalzug, notfalls rollbar oder mit Hinweis „+n“.
- **Handbuch:** mixconsole.md §3, §4.4, §16; Szene `MixConsoleScenes.cpp` (Bilder `mixconsole/overview`, `mixconsole/insert-slots`).

## F-1402 · Bug · Klick in die linke obere Ecke eines Insert-Slots stürzt ab (bad_function_call)

- **Status:** offen (im Code bestätigt, nicht ausprobiert)
- **Code:** `src/ui/fk/ui/InsertSlot.cpp:35-41` (`bakeMarkArea()` liefert ohne Rückruf ein leeres Rechteck), `:107-109` (`bakeMarkArea().expanded (2)` = (-2,-2,4,4) enthält die Punkte 0..1/0..1; dann wird `onBakeMarkClicked()` ohne Prüfung gerufen).
- **Beobachtung:** In der MixConsole ist `onBakeMarkClicked` nie gesetzt, im Inspector bei leeren Slots nicht. Ein Linksklick in die obersten zwei Pixel links eines solchen Slots ruft eine leere `std::function` auf: `std::bad_function_call`, Absturz.
- **Erwartung:** `if (onBakeMarkClicked != nullptr && ! bakeMarkArea().isEmpty() && …)`.
- **Handbuch:** mixconsole.md §4 (keine Textänderung).

## F-1403 · UX · Kanalzug: Pan, Fader, M, S, Ausgang und Slots ohne Tooltip, Pan ohne Wertanzeige

- **Status:** offen
- **Code:** `src/ui/fk/ui/ChannelStrip.cpp` setzt nur für Trim (`:54`) und Phase (`:68`) einen Tooltip; `panSlider`, `volumeSlider`, `muteButton`/`soloButton` (`:29-30`), `outputBox`, `nameLabel` und `InsertSlot` haben keinen, ebenso der Master-Fader (`src/ui/fk/ui/MixConsole.cpp:31-45`). Pan: `NoTextBox` ohne `setPopupDisplayEnabled` (Trim hat es).
- **Beobachtung:** Wie F-105/F-609 für Spurkopf und Inspector. Die Pan-Stellung lässt sich nirgends als Zahl ablesen.
- **Erwartung:** Tooltips; Pan mit Popup-Wert wie Trim. Zusammen mit F-105/F-609 beheben.
- **Handbuch:** mixconsole.md §3 (Absatz „Tooltips“), §8.

## F-1404 · Bug · Ziehen am Send-Regler erzeugt einen Undo-Schritt je Zwischenwert

- **Status:** offen
- **Code:** `src/ui/fk/ui/SendRow.cpp:27-32` ruft bei jeder Wertänderung `onLevelChanged` → `SendService::setSendLevel`, das jedes Mal `beginUndoStep (TRANS ("Send level"))` beginnt (`src/services/fk/services/SendService.cpp:121`). Gleich im Inspector.
- **Beobachtung:** Ein Zug am Send-Regler legt Dutzende Schritte „Send level“ an; Ctrl+Z nimmt nur ein Stück zurück. Auch das Mausrad legt je Raster einen Schritt an.
- **Erwartung:** Ein Schritt je Geste (onDragStart: Schritt beginnen, Werte ohne neuen Schritt setzen).
- **Handbuch:** mixconsole.md §6 (Note), §17.

## F-1405 · UX · Trim wirkt erst beim Loslassen

- **Status:** offen
- **Code:** `src/ui/fk/ui/ChannelStrip.cpp:55-64` und `src/ui/fk/ui/Inspector.cpp:233-243` (`onValueChange` setzt den Wert nur ohne gedrückte Maustaste, sonst erst `onDragEnd`).
- **Beobachtung:** Beim Ziehen bewegen sich Knopf und Wert, zu hören ist der alte Pegel. Gain Staging nach Gehör geht so nicht.
- **Erwartung:** Wert live setzen und den Undo-Schritt „Trim“ nur einmal je Geste beginnen.
- **Handbuch:** mixconsole.md §7 (Note).

## F-1406 · Unstimmigkeit · Klick auf gebackenen Insert: verschiedene Meldungen in MixConsole und Inspector

- **Status:** offen
- **Code:** `src/ui/fk/ui/Inspector.cpp:810` („This insert is baked. To edit it, first use "Unbake" (Inspector or track header).“) gegen `src/ui/fk/ui/ChannelStrip.cpp:179-183` (öffnet ohne Prüfung; `PluginService::createEditor` meldet über `bakeLock::lockedError`, `src/services/fk/services/BakeLock.cpp:45-57`).
- **Erwartung:** eine Meldung an beiden Stellen (z. B. die aus BakeLock, sie nennt auch Gruppen).
- **Handbuch:** mixconsole.md §4 (Baked inserts), §16.

## F-1407 · UX · Sends verkleinern den Fader, MixConsole rollt nicht senkrecht

- **Status:** offen (laut Layout-Rechnung, im Bild noch nicht geprüft)
- **Code:** `src/ui/fk/ui/ChannelStrip.cpp:222-270` (`resized`: jede Send-Zeile nimmt 22 px oben weg, der Fader bekommt den Rest); `src/ui/fk/ui/MixConsole.cpp:35` (`setScrollBarsShown (false, true)`), `:174-186`.
- **Beobachtung:** Bei der Standardhöhe der unteren Zone bleiben ohne Sends gut 60 px für Fader und Wertfeld. Ab etwa drei FX-Spuren ist der Fader kaum noch bedienbar; auch das Meter schrumpft.
- **Erwartung:** Mindesthöhe für den Fader, Sends/Inserts in einem rollbaren Bereich oder senkrechtes Rollen der MixConsole.
- **Handbuch:** mixconsole.md §3; Szene `MixConsoleScenes.cpp` (Bilder `mixconsole/overview`, `mixconsole/channel-strip`).

## F-1408 · UX · Klick auf einen Kanalzug wählt die Spur nicht aus

- **Status:** offen
- **Code:** `src/ui/fk/ui/ChannelStrip.h/.cpp` ohne Maus-Behandlung und Auswahl-Rückruf; `nameLabel` nicht editierbar.
- **Beobachtung:** Arrangement-Auswahl und Inspector bleiben bei einer anderen Spur; die ausgewählte Spur ist im Kanalzug nicht markiert.
- **Erwartung:** Klick auf Name oder freie Fläche wählt die Spur, ausgewählter Kanalzug hervorgehoben; Doppelklick auf den Namen benennt um.
- **Handbuch:** mixconsole.md §3 (Note).

## F-1409 · UX · Inserts lassen sich weder verschieben noch ersetzen

- **Status:** offen
- **Code:** `src/ui/fk/ui/InsertSlot.cpp` (`showContextMenu` nur Bypass/Activate, Remove, Sidechain, Presets; kein Drag-and-drop); `src/services/fk/services/PluginService.h` ohne Umsortieren/Ersetzen; `insertPlugin` hängt immer hinten an, egal welcher leere Slot geklickt wurde.
- **Beobachtung:** Um die Reihenfolge zu ändern, muss man Effekte entfernen und neu einsetzen (Einstellungen gehen verloren, außer über Presets). Gilt für MixConsole und Inspector.
- **Erwartung:** Slots per Ziehen umsortieren (Alt = kopieren) und „Replace“ im Kontextmenü.
- **Handbuch:** mixconsole.md §4.3.

## F-1410 · UX · Master-Kanal ohne Inserts, Mute und Ausgangswahl

- **Status:** offen
- **Code:** `src/ui/fk/ui/MixConsole.cpp:37-56` (Master nur Label, Fader, Meter); `src/services/fk/services/MixerService.h` kennt am Master nur die Lautstärke; `PluginService::insertPlugin` nur für Spuren.
- **Beobachtung:** Effekte auf der Summe gehen nur über eine Gruppe, in die alle Spuren geroutet werden. Der Master lässt sich nicht stummschalten.
- **Erwartung:** Insert-Slots und Mute am Master.
- **Handbuch:** mixconsole.md §14.

## F-1411 · UX · Pegelanzeige ohne haltende Übersteuerungsanzeige

- **Status:** offen
- **Code:** `src/ui/fk/ui/LevelMeter.cpp:13-19` (Spitze fällt rund 12 dB/s), `:61-63` (Spitzenlinie nur rot, solange ≥ 0 dB).
- **Beobachtung:** Eine kurze Übersteuerung ist nach rund einer Sekunde nicht mehr zu sehen; keine Spitzenwert-Zahl, kein haltender Clip-Anzeiger.
- **Erwartung:** Clip-LED je Kanalzug und Master, die hält, bis man sie anklickt; optional Spitzenwert in dB.
- **Handbuch:** mixconsole.md §10 (Note).

## F-1412 · Bug · Untere Zone bleibt dauerhaft klein, wenn das Fenster beim ersten Layout klein ist

- **Status:** offen (in den Szenen beobachtet; ob der Programmstart betroffen ist, im Lauf prüfen)
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:775-779` (`resized` schreibt die geklemmte Höhe nach `lowerZoneHeight` zurück: `lowerZoneHeight = juce::jlimit (minLowerZoneHeight, …, lowerZoneHeight)`).
- **Beobachtung:** Ist der verfügbare Platz beim ersten Layout klein, wird die gespeicherte Höhe auf 160 px gesetzt und bleibt so, auch wenn das Fenster danach größer wird. In den Handbuch-Szenen (Fenster 1000×900) zeigte die MixConsole deshalb nur rund 130 px, ohne Fader. Erst Doppelklick auf den Trenner stellt 360 px wieder her.
- **Erwartung:** Gewünschte Höhe getrennt von der angezeigten halten: nur beim Layout klemmen, `lowerZoneHeight` nicht überschreiben (nur Ziehen und Doppelklick ändern sie).
- **Handbuch:** project-window.md (Abschnitt zur Größe der unteren Zone, nach dem Fix prüfen); Szene `MixConsoleScenes.cpp` (setzt die Höhe über den Trenner zurück, danach überflüssig).

## F-1501 · Bug · „Clear list“ und „Remove …“ im Plugin-Manager entfernen die FK-Plugins bis zum Neustart

- **Status:** offen (im Code bestätigt, nicht ausprobiert)
- **Code:** `src/ui/fk/ui/PluginManagerWindow.cpp:48-51` (JUCEs `PluginListComponent` arbeitet auf der ganzen `knownPluginList`, in die `src/engine/fk/engine/EngineContext.cpp:66-71` auch die FK-Plugins einträgt); JUCE `juce_PluginListComponent.cpp:562-608` (`Clear list` → `list.clear()`; `removeMissingPlugins` fragt `doesPluginStillExist`, das für das Format „FK“ `false` liefert).
- **Beobachtung:** **Options › Clear list**, **Remove any plug-ins whose files no longer exist** sowie **Remove plug-in from list** oder Entf auf einer FK-Zeile werfen FK Synth, FK Sampler, FK Trim und die übrigen FK-Plugins aus der Liste; ein Scan holt sie nicht zurück. Bis zum Neustart fehlen sie in allen Menüs; Sampler-Spuren scheitern mit „FK Sampler is not registered.“, danach geöffnete Projekte finden ihre FK-Plugins vermutlich nicht.
- **Erwartung:** FK-Plugins in „Search and Scan“ nicht anbieten (gefilterte Liste) oder nach jeder Änderung `registerInternalPlugins()` erneut ausführen.
- **Handbuch:** instruments-and-effects.md §4 (Bild), §4.4 (Caution); Szene `InstrumentsAndEffectsScenes.cpp` (Bild `instruments-and-effects/plugin-manager-search-scan`).

## F-1502 · Text · Interne deutsche Kategorienamen in „Search and Scan“ und in der Suche von „My Plugins“

- **Status:** offen
- **Code:** `src/plugins/fk/plugins/*Processor.cpp`, `MidiEffectProcessors.cpp` (`info()`: Kategorien „Dynamik“, „Raum“, „Verzerrung“, „Kanal“, „MIDI-Effekt“); JUCE zeigt sie in der Spalte **Category**. `src/ui/fk/ui/PluginLibraryPanel.cpp:133-134` sucht in der deutschen Kennung `plugin.group`, nicht in `getGroupDisplayName()`. FK Trim erscheint in „Search and Scan“, weil `channelTrim::isChannelTrim` nur in `collectPlugins` filtert.
- **Beobachtung:** In der englischen Oberfläche findet „Dynamics“ in **My Plugins** keine Dynamik-Plugins, „Dynamik“ schon. „Search and Scan“ zeigt deutsche Kategorien und das sonst unsichtbare FK Trim.
- **Erwartung:** Suche auch mit `getGroupDisplayName (plugin.group)`; FK-Kategorien englisch oder neutral (Stichwortliste in `automaticGroup` anpassen); FK Trim aus der Liste (mit F-1501).
- **Handbuch:** instruments-and-effects.md §3.3 (Note), §4 (Bild), §4.1 (Note).

## F-1503 · UX · Eigene Scan-Ordner werden nicht gemerkt

- **Status:** offen
- **Code:** `src/ui/fk/ui/PluginManagerWindow.cpp:48-51` (`PluginListComponent (…, nullptr)`); ohne `PropertiesFile` liest und speichert JUCE den Suchpfad nicht (`juce_PluginListComponent.cpp:248-249`, `:347-350`).
- **Beobachtung:** Jeder Scan startet wieder mit den Standardordnern; eigene Ordner muss man vor jedem Scan neu hinzufügen.
- **Erwartung:** `PropertiesFile` übergeben oder die Pfade je Format in den Engine-Einstellungen halten.
- **Handbuch:** instruments-and-effects.md §4.2 (Note).

## F-1504 · UX · Kategorie-Auswahl in „My Plugins“ zeigt nie „automatic“

- **Status:** offen
- **Code:** `src/ui/fk/ui/PluginLibraryPanel.cpp:248-251` zeigt immer die gültige Kategorie; `PluginInfo` (`src/services/fk/services/PluginService.h:20-31`) verrät nicht, ob sie automatisch ist.
- **Beobachtung:** Man sieht nicht, ob eine Kategorie selbst gewählt oder abgeleitet ist.
- **Erwartung:** `PluginInfo` um `groupIsAutomatic` ergänzen und „automatic (Dynamics)“ zeigen.
- **Handbuch:** instruments-and-effects.md §3.3 (Note).

## F-1505 · UX · Keine Presets für Instrumente

- **Status:** offen
- **Code:** Presets nur im Kontextmenü von `InsertSlot` (`src/ui/fk/ui/InsertSlot.cpp:134-176`); das Instrument ist kein Insert (`PluginService::getInserts` lässt `isSynth()` weg, `src/services/fk/services/PluginService.cpp:425-434`); `LibraryService::getItems` (`:96-103`) listet trotzdem Instrument-Presets; Leerhinweis `src/ui/fk/ui/InstrumentBrowser.cpp:130` verweist auf Inserts.
- **Beobachtung:** Eigene Klänge eines Instruments lassen sich in der DAW weder speichern noch laden noch löschen. Die Art „Preset“ im Instrumenten-Browser bleibt praktisch immer leer.
- **Erwartung:** Preset-Menü am Instrument (Inspector, Spurkopf, auch Rack-Instrumente); Leerhinweis anpassen.
- **Handbuch:** instruments-and-effects.md §6.1, §11.

## F-1506 · Unstimmigkeit · Spurname aus dem Instrumenten-Browser ohne Nummer

- **Status:** offen
- **Code:** `src/services/fk/services/LibraryService.cpp:237` (`createInstrumentTrack (plugin.name, …)`) gegen `src/ui/fk/ui/ProjectComponent.cpp:1687` und `:2239` (Name + Spurzahl).
- **Beobachtung:** Über Menü oder Button heißt die Spur „FK Synth 5“, per Doppelklick im Browser „FK Synth“; mehrere Browser-Spuren tragen denselben Namen.
- **Erwartung:** Gleiche Benennung an beiden Stellen (Nummer im Service vergeben). Verwandt mit F-206/F-509.
- **Handbuch:** instruments-and-effects.md §6.3 (Note).

## F-1507 · UX · „Remove own tag“ bietet auch automatische Tags an

- **Status:** offen
- **Code:** `src/ui/fk/ui/InstrumentBrowser.cpp:279-284` (Untermenü aus allen `item.tags`); `src/services/fk/services/LibraryService.cpp:218-219`.
- **Beobachtung:** Wählt man ein automatisches Tag, kommt „Only your own tags can be removed.“; welche Tags eigene sind, ist nirgends zu sehen.
- **Erwartung:** Eigene Tags getrennt führen; nur diese im Untermenü.
- **Handbuch:** instruments-and-effects.md §6.4 (Note).

## F-1508 · Text · Platzhalter der Browser-Suche nennt „favorite“

- **Status:** offen
- **Code:** `src/ui/fk/ui/InstrumentBrowser.cpp:102` („… (e.g. piano, strings, favorite)“); `src/services/fk/services/LibraryService.cpp:133` durchsucht nur Name, Quelle und Tags.
- **Beobachtung:** „favorite“ findet keine Favoriten.
- **Erwartung:** Platzhalter ohne „favorite“ oder Favoriten als automatisches Tag.
- **Handbuch:** instruments-and-effects.md §6.2 (Note); Bild `instruments-and-effects/instrument-browser`.

## F-1509 · UX · FK Sampler meldet Ladefehler nur im Log

- **Status:** offen
- **Code:** `src/plugins/fk/plugins/SamplerProcessor.cpp:213` (`chooseFile`: Fehler nur ins Log) und `:163` (`setStateInformation` ebenso).
- **Beobachtung:** Scheitert **Load...**, passiert sichtbar nichts. Fehlt die SFZ-Datei beim Öffnen des Projekts, bleibt die Spur still, vermutlich mit „0 regions“ in der Statuszeile.
- **Erwartung:** Fehler in der Statuszeile anzeigen und beim Öffnen des Projekts melden (zusammen mit F-307).
- **Handbuch:** instruments-and-effects.md §14.2 (Note).

## F-1510 · UX · Notenwerte des Arpeggiators nicht nach Länge sortiert

- **Status:** offen
- **Code:** `src/plugins/fk/plugins/MidiEffectProcessors.cpp:12` (`rates`: 1/4, 1/8, 1/8 Triplet, 1/16, 1/16 Triplet, 1/32, 1/2).
- **Beobachtung:** „1/2“ steht am Ende; FK Delay sortiert nach Länge.
- **Erwartung:** Nach Länge sortieren, gespeicherte Projekte über Index-Abbildung übernehmen (Parameter-Version erhöhen).
- **Handbuch:** instruments-and-effects.md §14.13 (Tabelle).

## F-1511 · Bug · Mehr als 500 Instrumente: Menü-IDs im Track-Menü überlappen

- **Status:** offen (im Code bestätigt, nicht ausprobiert)
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:175-177` (`instrumentMenuBase = 1000`, `rackMenuBase = 1500`, `recentMenuBase = 2000`); `menuItemSelected` prüft Rack (`:2215`) vor Instrument (`:2229`).
- **Beobachtung:** Ab dem 501. Instrument legt **Track › Instrument track** ein Rack-Instrument mit einem anderen Instrument an; ab dem 501. Eintrag unter **Track › Rack instrument** öffnet sich ein zuletzt benutztes Projekt.
- **Erwartung:** ID-Blöcke mit großem Abstand oder Menüeinträge mit eigener Aktion (`PopupMenu::Item::setAction`).
- **Handbuch:** instruments-and-effects.md §5 (Note).

## F-1601 · Bug · Abwurf-Vorschau aus dem Medien-Browser behandelt MIDI-Dateien wie Audio

- **Status:** offen
- **Code:** `src/ui/fk/ui/ArrangeView.cpp:459-469` (`itemDragEnter`/`itemDragMove` rufen `previewFor (x, y)` = Spurart Audio und setzen kein Label) gegen `fileDragMove` `:414-420` (Instrumentenspur für MIDI, Label „New instrument track“); Zeichnen `:344-346` fällt ohne Label auf „New audio track“ zurück.
- **Beobachtung:** Zieht man eine MIDI-Datei aus dem Medien-Browser über eine Instrumentenspur, wird die Spur nicht hervorgehoben und unten steht „New audio track“. Der Abwurf landet trotzdem richtig. Aus dem Dateimanager stimmt die Vorschau.
- **Erwartung:** `itemDragEnter`/`itemDragMove` wie `fileDragMove` über `filesFromDragDescription` führen (gemeinsame Hilfsfunktion).
- **Handbuch:** media-browser.md §8 (Note zur Hervorhebung).

## F-1602 · Text · Vorhör-Tooltip verspricht Leertaste, die das Projekt startet

- **Status:** offen
- **Code:** `src/ui/fk/ui/MediaBrowser.cpp:21` („Audition the selected file (Space in the browser)“); kein `keyPressed`, `juce::TreeView` kennt keine Leertaste; die Taste geht an `ProjectComponent::keyPressed` (`src/ui/fk/ui/ProjectComponent.cpp:796`) → **Play / Stop**.
- **Beobachtung:** Leertaste im Dateibaum startet bzw. stoppt die Projektwiedergabe statt der Vorschau.
- **Erwartung:** Leertaste im fokussierten Medien-Browser schaltet die Vorschau (wie Cubase MediaBay), oder Tooltip ohne Tastenangabe.
- **Handbuch:** media-browser.md §5 (Note „Despite the tooltip …“).

## F-1603 · UX · Vorhören: Dateiwechsel braucht zwei Klicks, Lesefehler ohne Meldung

- **Status:** offen
- **Code:** `src/ui/fk/ui/MediaBrowser.cpp:143-156` (`togglePreview`: läuft eine Vorschau, wird nur gestoppt; Fehler von `preview.play` nur ins Log), `:102` `selectionChanged` stoppt/wechselt die Vorschau nicht; `PreviewService` ohne Lautstärke.
- **Beobachtung:** Wählt man während der Vorschau eine andere Datei, spielt die alte weiter; Play stoppt dann nur, für die neue Datei braucht es einen zweiten Klick. Eine unlesbare Datei tut einfach nichts. Keine Vorhör-Lautstärke, kein automatisches Vorhören.
- **Erwartung:** Play bei anderer Auswahl startet die neue Datei; optional Auto-Vorhören, Lautstärkeregler, Meldung in der Infozeile bei unlesbarer Datei.
- **Handbuch:** media-browser.md §5 (zweite Note).

## F-1604 · Bug · Medien-Browser: nur Ordner ausgewählt, Infozeile zeigt alte Datei, Buttons bleiben aktiv

- **Status:** offen
- **Code:** `src/ui/fk/ui/MediaBrowser.cpp:102-131` (`selectionChanged`: Infozeile nur bei `selectedFile.existsAsFile()` neu gesetzt, `playButton.setEnabled (! midi)` auch ohne Datei; `importButton` nie deaktiviert).
- **Beobachtung:** Klickt man nach einer Datei auf einen Ordner, verschwindet die Wellenform, die Infozeile nennt weiter die vorige Datei. Vorhören und Einfügen sind klickbar und tun nichts.
- **Erwartung:** Ohne ausgewählte Datei den Hinweistext zeigen und beide Buttons deaktivieren.
- **Handbuch:** media-browser.md §4 (Note).

## F-1605 · Text · Content-Manager: deutscher Abbrechen-Button, Menünamen im Hinweis weichen ab

- **Status:** offen
- **Code:** `src/ui/fk/ui/ContentManagerDialog.h:73` (`cancelButton { "Abbrechen" }`, ohne TRANS); Hinweis `src/ui/fk/ui/ContentManagerDialog.cpp:75` („Track > Instrument from Library“, „Instrument Browser“) gegen Menü `src/ui/fk/ui/ProjectComponent.cpp:1976` („Instrument from library“) und Reiter „Instruments“ (`LowerZone.cpp:25`); Menühinweis `ProjectComponent.cpp:1974` („Studio > Content Manager“) gegen `:2117` („Content Manager (free libraries)...“).
- **Beobachtung:** Im englischen Programm heißt der Button „Abbrechen“. Die Hinweise nennen Menüpfade anders als die Menüs.
- **Erwartung:** `TRANS ("Cancel")`; Hinweistexte mit den echten Menünamen (vgl. F-202).
- **Handbuch:** media-browser.md §9.2, §9.3; Bild `media-browser/content-manager`.

## F-1606 · UX · Medien-Browser ohne eigene Orte, Favoriten, Suche und ohne gemerkten Ordner

- **Status:** offen
- **Code:** `src/ui/fk/ui/MediaBrowser.cpp:12-14` (nur Home, Music, Desktop), `:72` (beim Start immer `rootHome`), Filter fest `MediaBrowser.h:47` (kein Suchfeld).
- **Beobachtung:** Sample-Ordner auf anderen Laufwerken erreicht man nur über den Baum ab Home/Musik/Desktop. Nach jedem Neustart steht der Browser wieder auf Home. Keine Suche, kein Typfilter.
- **Erwartung:** eigene Orte (Favoriten), letzten Ordner und aufgeklappte Ordner merken, Suchfeld und Typfilter (wie Cubase MediaBay).
- **Handbuch:** media-browser.md §3; Bild `media-browser/overview`.

## F-1607 · UX · Content-Paket entfernen löscht Gigabytes ohne Rückfrage

- **Status:** offen
- **Code:** `src/ui/fk/ui/ContentManagerDialog.cpp:52-57` (Klick → `removePackage`), `:128` (`content.remove` sofort).
- **Beobachtung:** **Remove** löscht das installierte Paket (bis 2,3 GB Download) sofort und unwiderruflich. Spuren mit Instrumenten daraus laden sie beim nächsten Öffnen nicht mehr.
- **Erwartung:** Rückfrage mit Paketname und Größe, ggf. Hinweis auf Projekte, die das Paket nutzen.
- **Handbuch:** media-browser.md §9.4.

## F-1701 · Bug · Bake Inserts mit Markierung: Kanal-Trim zählt als Insert

- **Status:** offen (im Code bestätigt, nicht ausprobiert)
- **Code:** `src/services/fk/services/BakeService.cpp:42-45` (`isInsert` = jedes `ExternalPlugin`, das kein Synth ist, also auch FK Trim), `:108-118`, `:215-217` (`bakedInserts`); dagegen `src/services/fk/services/PluginService.cpp:864-875` (`getExternalPlugins` lässt den Trim weg); `MixerService::ensureTrim` (`src/services/fk/services/MixerService.cpp:115-128`) setzt den Trim vor alle Inserts.
- **Beobachtung:** Wurde an der Spur je Trim oder Phase verändert, backt „Inserts“ mit Slots 1 bis n markiert nur den Trim und die Slots 1 bis n−1; Slot n bleibt live, `bakedInserts` ist um eins zu groß. Ohne Markierung stimmt es.
- **Erwartung:** `isInsert` schließt `channelTrim::isChannelTrim` aus; der Freeze-Punkt kommt hinter den n-ten echten Insert.
- **Handbuch:** track-bake.md §5.2 (Note).

## F-1702 · Bug · Bake und Unbake hinterlassen einen unbenannten Undo-Schritt „Change“

- **Status:** offen (aus dem Code abgeleitet, nicht ausprobiert)
- **Code:** `src/services/fk/services/BakeService.cpp:146-149` und `:189` (Freeze-Punkt einsetzen/entfernen mit dem UndoManager des Edits, ohne `beginUndoStep`); Tracktion `tracktion_AudioTrack.cpp:94-98` (FreezeUpdater taut auf, wenn der Freeze-Punkt fehlt); `ProjectService.cpp:509-510` („Change“).
- **Beobachtung:** Nach dem Backen steht „Change“ im Verlauf. Ctrl+Z entfernt den Freeze-Punkt, die Spur wird live; Datei und `fkBake` bleiben. Redo backt nicht wieder. ADR 0009 sagt: „Bake ist kein Undo-Schritt“.
- **Erwartung:** Freeze-Punkt ohne UndoManager setzen/entfernen, sodass Bake/Unbake keine Spur in der Historie hinterlassen.
- **Handbuch:** track-bake.md §15 (Note).

## F-1703 · Bug · Gruppen-Bake: Sends und Sidechain-Signale der zuführenden Spuren verstummen

- **Status:** offen (aus dem Code abgeleitet, nicht ausprobiert)
- **Code:** Tracktion `tracktion_EditNodeBuilder.cpp:1071-1095` (gefrorene Spur: nur Datei und Plugins, keine `getDirectInputTracks`), `:1986-2030`; `BakeService.cpp:155` (Render nur der Gruppe).
- **Beobachtung:** Ist eine Gruppe gebacken, laufen ihre zuführenden Spuren nicht. Deren Sends an FX-Spuren und ihre Rolle als Sidechain-Quelle fallen weg und sind nicht in der Datei. Fader/Pan/Mute der zuführenden Spuren sind bedienbar, wirken aber nicht.
- **Erwartung:** Sends zuführender Spuren weiter versorgen (oder Bake mit Hinweis ablehnen bzw. warnen); wirkungslose Regler sperren oder kennzeichnen.
- **Handbuch:** track-bake.md §6 (Note), §13 (Note).

## F-1704 · Bug · Nachklang nach dem letzten Clip wird gerendert, aber nicht abgespielt

- **Status:** offen (aus dem Code abgeleitet, nicht ausprobiert)
- **Code:** Tracktion `tracktion_AudioTrack.cpp:1119-1120` (Render bis `getLengthIncludingInputTracks()` plus `findEndAllowance`) gegen `tracktion_EditNodeBuilder.cpp:1077-1078` (WaveNode spielt nur bis `getLengthIncludingInputTracks()`).
- **Beobachtung:** Hall- oder Delay-Fahnen am Ende einer gebackenen Spur werden abgeschnitten.
- **Erwartung:** Abspielbereich um den gerenderten Nachlauf verlängern (Länge der Datei).
- **Handbuch:** track-bake.md §16.

## F-1705 · UX · Routing in und aus einer gebackenen Gruppe nicht gesperrt

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:201` (`setOutput` ohne `bakeLock`-Prüfung).
- **Beobachtung:** Eine zuführende Spur lässt sich aus der gebackenen Gruppe herausrouten; sie spielt dann live und steckt noch in der Datei (doppelt). Eine neu hineingeroutete Spur ist sofort gesperrt, aber nicht in der Datei (stumm).
- **Erwartung:** Ausgangswechsel von/zu einer gebackenen Gruppe mit `lockedError` ablehnen.
- **Handbuch:** track-bake.md §16.

## F-1706 · UX · Aufnahme auf gebackene Spur nicht gesperrt

- **Status:** offen (aus dem Code abgeleitet, nicht ausprobiert)
- **Code:** `src/services/fk/services/TrackService.cpp` (`setArmed`) ohne `bakeLock`; Tracktion nimmt in gefrorene Spuren auf.
- **Beobachtung:** R lässt sich an einer gebackenen Spur einschalten. Eine Aufnahme legt dort einen Clip an (an der Clip-Sperre vorbei), der nicht zu hören ist.
- **Erwartung:** Scharfschalten gebackener (und in Gruppen gebackener) Spuren mit der Bake-Meldung ablehnen.
- **Handbuch:** track-bake.md §10 (Note).

## F-1707 · UX · Automation gebackener Plugins bearbeitbar, aber wirkungslos

- **Status:** offen
- **Code:** `src/services/fk/services/AutomationService.*` (keine `bakeLock`-Prüfung); gefrorene Plugins laufen nicht (`tracktion_EditNodeBuilder.cpp:1436`).
- **Beobachtung:** Punkte in Lagen gebackener Plugins lassen sich ändern; hörbar wird das erst nach Unbake und neuem Bake.
- **Erwartung:** Solche Lagen sperren (Meldung wie bei Clips) oder als gebacken kennzeichnen.
- **Handbuch:** track-bake.md §13 (Note).

## F-1708 · UX · Offene Plugin-Fenster bleiben beim Backen offen und bedienbar

- **Status:** offen
- **Code:** `src/ui/fk/ui/MixConsole.cpp:182-188` (vorhandenes Fenster wird ohne Sperrprüfung nach vorn geholt); `BakeService::bakeUpTo` schließt keine Fenster.
- **Beobachtung:** War das Fenster eines Plugins vor dem Backen offen, bleibt es bedienbar; Änderungen wirken bis zum Unbake nicht.
- **Erwartung:** Fenster gebackener Plugins beim Backen schließen (bzw. beim Öffnen auch bestehende Fenster prüfen).
- **Handbuch:** track-bake.md §9 (Note).

## F-1709 · UX · Tempo- und Taktartänderung nach dem Backen ohne Hinweis

- **Status:** offen
- **Code:** `src/services/fk/services/SignatureService.*` (keine Prüfung auf gebackene Spuren); die Bake-Datei liegt in Sekunden.
- **Beobachtung:** Nach einer Tempoänderung folgen MIDI- und tempofolgende Clips, die Bake-Datei nicht; gebackene Spuren laufen auseinander, ohne Meldung.
- **Erwartung:** Hinweis bei Tempoänderung, wenn gebackene Spuren existieren (oder Neu-Backen anbieten).
- **Handbuch:** track-bake.md §12.

## F-1710 · Text · Tooltip „Click: unbake“ am gebackenen Bake-Button

- **Status:** offen
- **Code:** `src/ui/fk/ui/TrackHeader.cpp:130` gegen `:81` (Klick öffnet immer das Menü, `showBakeMenu`).
- **Beobachtung:** Der Tooltip verspricht Lösen per Klick, es öffnet sich aber das Bake-Menü.
- **Erwartung:** z. B. „Baked: instrument/inserts run as audio. Click: menu with Unbake“ (Szene sucht weiter über „Baked: instrument“).
- **Handbuch:** track-bake.md §8 (Note); Szene `TrackBakeScenes.cpp` (Bild `track-bake/baked-track`).

## F-1711 · UX · Bake-Feld im Insert-Slot ohne Tooltip, Markierungen auf gebackener Spur unsichtbar

- **Status:** offen
- **Code:** `src/ui/fk/ui/InsertSlot.cpp:35-41`, `:66-76`; `src/ui/fk/ui/Inspector.cpp:803-804` (`setBakeMarked (! baked && …)`, `onBakeMarkClicked` auch auf gebackenen Slots).
- **Beobachtung:** Das kleine Feld erklärt sich nicht selbst. Auf einer gebackenen Spur ändert ein Klick die Markierung unsichtbar, und das nächste „Inserts“ nutzt sie.
- **Erwartung:** Tooltip („Bake up to here: marks slots 1 to n“); Markierung auf gebackenen Spuren sichtbar halten oder das Feld dort sperren.
- **Handbuch:** track-bake.md §5.2; Szene `TrackBakeScenes.cpp` (Bilder `track-bake/inspector-marks`, `track-bake/inspector-baked`).

## F-1712 · Bug · Sidechain gebackener Inserts beim Rendern

- **Status:** offen (Verdacht aus dem Code, vor dem Fix ausprobieren)
- **Code:** Tracktion `tracktion_AudioTrack.cpp:1114` (`r.tracksToDo` nur die eigene Spur), `tracktion_Renderer.cpp:31` (`allowedTracks`); die Sidechain-Quellspur ist nicht enthalten.
- **Beobachtung:** Ein gebackener Insert mit Sidechain-Quelle (z. B. FK Compressor von Kick) bekommt beim Rendern vermutlich kein Steuersignal; die Datei klänge ohne die Sidechain-Wirkung.
- **Erwartung:** Ausprobieren; falls bestätigt, Sidechain-Quellen beim Bake mitrendern oder den Bake solcher Inserts mit Hinweis ablehnen.
- **Handbuch:** track-bake.md §13 (Tip).

## F-1801 · Bug · Abbrechen des Exports blockiert bis 10 s und beendet den Thread gewaltsam

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:28` (`ThreadWithProgressWindow` mit Standard-Timeout 10000 ms), `:34-50` (`run()` prüft `threadShouldExit()` nie); `src/services/fk/services/ExportService.cpp` `render()` (Schleife `while (task->runJob() …)` ohne Abbruch, danach `MessageManager::callSync`); JUCE `timerCallback` → `stopThread (10000)` auf dem Message-Thread, `juce_Thread.cpp:264-271` `killThread()`.
- **Beobachtung:** Nach **Cancel** wartet der Message-Thread in `stopThread`, der Render-Thread läuft weiter und hängt in `callSync`. Nach 10 s wird der Thread gewaltsam beendet; die Oberfläche steht so lange. `ScopedRenderStatus`/`ScopedTrackMuter` werden nicht abgebaut (Verdacht: Edit bleibt „rendert“ markiert, Stummschaltungen bleiben). Eine halbe Datei bleibt liegen. Auch bei schon fertigem Export erscheint „Export cancelled.“.
- **Erwartung:** Abbruch an den Render-Job weitergeben, aufräumen, unfertige Datei löschen, Meldung nur bei echtem Abbruch.
- **Handbuch:** exporting.md §15.

## F-1802 · Bug · Export-Dialog bietet Bittiefen und Sampleraten an, die das Format nicht schreiben kann

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:197-201` (32 bit float für alle verlustfreien Formate), `:208-215` (88200/96000 Hz für alle Formate); Prüfung erst in Tracktion `tracktion_RenderSpecification.cpp:415-436`.
- **Beobachtung:** FLAC/AIFF mit 32 bit float oder MP3 mit 88200/96000 Hz scheitern erst nach Export Now mit „Export not possible: flac doesn't support 32 bit. Supported bit depths: 16, 24“.
- **Erwartung:** Nur passende Einträge anbieten oder sperren, bei Formatwechsel auf einen gültigen Wert springen.
- **Handbuch:** exporting.md §5 (Note), §7 (Note).

## F-1803 · UX · Mixdown und MIDI-Datei überschreiben ohne Rückfrage, Stems nummerieren

- **Status:** offen
- **Code:** `src/services/fk/services/ExportService.cpp` `render()` (`file.deleteFile()`), `exportMidi()` (`target.deleteFile()`); Stems über `te::createPerTrackSpecifications` → `getNonexistentChildFile`.
- **Beobachtung:** Ein zweiter Mixdown mit dem vorgeschlagenen Namen ersetzt den ersten still; Stems legen dagegen neue Dateien mit Nummer an.
- **Erwartung:** Vor dem Überschreiben fragen (Replace / Keep both) oder einheitlich nummerieren.
- **Handbuch:** exporting.md §4.

## F-1804 · UX · Eigener Export-Zielort geht bei Format- oder Zielwechsel verloren

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:368-388` (`updateTargetFile` behält die Datei nur im Export-Ordner; Stems und MIDI immer neu).
- **Beobachtung:** Wer mit „Change...“ `~/Desktop/Mix.wav` wählt und danach MP3 einstellt, bekommt wieder `Export/<Projekt>.mp3`.
- **Erwartung:** Gewählten Ordner und Namen behalten, nur die Endung anpassen.
- **Handbuch:** exporting.md §4.

## F-1805 · Text · Export-Dialog: Range, Bit depth, Zielpfad und Stem-Häkchen ohne Tooltip; „Loop range“ statt Locatoren

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:191-201` (rangeBox, bitDepthBox ohne `setTooltip`), `:248-260` (Stem-Häkchen), `fileName`; Titel `"Export"` ohne `TRANS` (`:497`).
- **Beobachtung:** Vier Bedienelemente erklären sich nicht. „Loop range“ nutzt den Bereich zwischen L und R auch bei ausgeschaltetem Loop; im Transport heißen sie Locatoren.
- **Erwartung:** Tooltips (z. B. „Range: whole project or the range between the left and right locator“), Begriff angleichen.
- **Handbuch:** exporting.md §2; Szene `ExportingScenes.cpp` (Suche über Text).

## F-1806 · UX · Stems folgen Mute und Solo ohne Hinweis

- **Status:** offen (aus dem Code, vor dem Fix ausprobieren)
- **Code:** `src/services/fk/services/ExportService.cpp` `exportStems()`; Tracktion `TrackMuteState` gilt auch beim Rendern.
- **Beobachtung:** Ein stummgeschalteter Track, oder bei Solo jeder andere, ergibt eine stille Stem-Datei; der Dialog sagt nichts.
- **Erwartung:** Stems unabhängig von Mute/Solo rendern oder warnen bzw. betroffene Spuren kennzeichnen.
- **Handbuch:** exporting.md §12 (Note), §14.

## F-1807 · UX · Stem-Liste enthält Spuren ohne Audio und ausgeblendete Spuren ohne Kennzeichnung

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:248-262` (`tracks.getAudioTracks()`, Zusatz nur für group/FX).
- **Beobachtung:** MIDI-Spuren für externe Geräte, Rack-Ausgänge und ausgeblendete Spuren stehen ohne Zusatz in der Liste und sind vorgewählt; MIDI-Spuren ergeben vermutlich stille Dateien.
- **Erwartung:** MIDI-Spuren weglassen oder abwählen, Rack-Ausgänge/ausgeblendete Spuren kennzeichnen.
- **Handbuch:** exporting.md §12; Bild `exporting/stems`.

## F-1808 · Text · MP3-Hinweis nennt das Linux-Paket auf allen Systemen und fehlt bei Stems

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp:181`, `:424-425` (Hinweis nur bei Mixdown); `ExportService.cpp` („package libmp3lame0“); Tracktion `tracktion_LAMEManager.cpp` (Windows `libmp3lame.dll`, macOS `.dylib`, Rückfall `/usr/bin/lame`).
- **Beobachtung:** Unter Windows ist „install the package libmp3lame0“ falsch. Bei Stems ist MP3 ausgegraut ohne Erklärung. Dass ein Neustart nötig ist (Prüfung einmal je Sitzung), steht nirgends.
- **Erwartung:** Hinweis je System, auch bei Stems, mit „restart FK Audio Suite“.
- **Handbuch:** exporting.md §10.

## F-1809 · UX · Export-Einstellungen werden nicht gemerkt

- **Status:** offen
- **Code:** `src/ui/fk/ui/ExportDialog.cpp` Konstruktor (alle Felder auf Vorgabe), keine Ablage im Projekt oder in den Einstellungen.
- **Beobachtung:** Jedes Öffnen beginnt wieder mit WAV, 24 bit, Off …; Ziel, Format, Normalize und Stem-Auswahl müssen jedes Mal neu gesetzt werden.
- **Erwartung:** Letzte Einstellungen je Projekt merken (wie Cubase).
- **Handbuch:** exporting.md §1.

## F-2001 · Bug · „Default“ gibt eine Taste zurück, die inzwischen ein anderer Befehl hat

- **Status:** offen (aus dem Code abgeleitet, nicht ausprobiert)
- **Code:** `src/ui/fk/ui/Commands.cpp:286` (`resetToDefault` löscht nur die eigene Abweichung, ohne die Standardtasten anderen Befehlen wegzunehmen) gegen `:257` (`setKeys` nimmt sie weg und meldet es); `find` (`:226`) läuft in Enum-Reihenfolge.
- **Beobachtung:** Range Tool bekommt „1“ (Select Tool verliert sie), danach Select Tool › **Default**: Beide Befehle zeigen „1“, die Taste löst stumm nur Select Tool aus. „Eine Taste gehört höchstens einem Befehl“ (Commands.h) stimmt dann nicht mehr.
- **Erwartung:** `resetToDefault` wie `setKeys` behandeln: Standardtasten anderen Befehlen wegnehmen und im Hinweis nennen.
- **Handbuch:** keyboard-shortcuts.md §8 (Caution).

## F-2002 · UX · Werkzeugtasten 1–8 schalten im Key Editor die Werkzeuge des Arrangements

- **Status:** offen
- **Code:** `src/ui/fk/ui/NoteGrid.cpp:414-453` (`keyPressed` behandelt nur Entf/Rücktaste, Strg+A und Pfeile); die Taste wandert zu `src/ui/fk/ui/ProjectComponent.cpp:796-801` → `performCommand`. Die Werkzeuge des Key Editors haben keine Befehle.
- **Beobachtung:** Wer im Key Editor „1“ für das Auswahlwerkzeug drückt, wechselt unsichtbar das Werkzeug im Arrangement; der Key Editor bleibt beim Stift. In Cubase wirken die Werkzeugtasten im fokussierten Editor. Gleiche Ursache wie F-901/F-1002/F-1102 (Tasten wandern aus Editoren ans Projektfenster).
- **Erwartung:** Bei Fokus im Notenraster wählen 1 (Select), 8 (Pencil) und 5 (Eraser) die Werkzeuge des Key Editors, andere Werkzeugtasten werden geschluckt; oder eigene Befehle für die Editor-Werkzeuge.
- **Handbuch:** keyboard-shortcuts.md §10 (Note).

## F-2003 · UX · „Remove Key“ ohne ausgewählte Taste entfernt die erste Taste

- **Status:** offen
- **Code:** `src/ui/fk/ui/ShortcutEditor.cpp:181-196` (`removeSelectedKey`: Zeile −1 wird per `jlimit` zu 0) und `:166` (Button aktiv, sobald der Befehl eine Taste hat).
- **Beobachtung:** Bei Befehlen mit zwei Tasten (z. B. Redo) entfernt **Remove Key** ohne Auswahl stumm die erste Taste.
- **Erwartung:** Button erst aktiv, wenn eine Taste ausgewählt ist (bei genau einer Taste diese automatisch auswählen).
- **Handbuch:** keyboard-shortcuts.md §7 (Caution).

## F-2004 · UX · Tastenbelegung ohne Presets, Export, Import und Druckliste

- **Status:** offen
- **Code:** `src/services/fk/services/ShortcutService.*` (nur Abweichungen als JSON in `Settings.xml`, Eintrag `…_fkShortcuts`); `src/ui/fk/ui/ShortcutEditor.cpp` (nur „Default“ und „All to Cubase Defaults“).
- **Beobachtung:** Eigene Belegungen lassen sich nicht unter einem Namen speichern, wechseln, mitnehmen oder als Liste ausgeben. Cubase hat Presets und Import/Export.
- **Erwartung:** Presets, Export/Import als Datei, Ausgabe der Liste.
- **Handbuch:** keyboard-shortcuts.md §8 (Absatz „Presets“).

## F-2005 · Unstimmigkeit · Menü „Export“ zeigt die Taste von „Export Project“ nicht

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1860` (`menu.addItem (itemExport, TRANS ("Export (mixdown, stems, MIDI)..."), …)` ohne `keyBindings.menuText`) gegen den Tooltip des Export-Buttons (`updateShortcutTooltips`, `:1000ff`).
- **Beobachtung:** Wer **Export Project** eine Taste gibt, sieht sie im Tooltip, aber nicht im Menü **File**.
- **Erwartung:** `keyBindings.menuText (TRANS ("Export (mixdown, stems, MIDI)..."), Command::exportProject)`.
- **Handbuch:** keyboard-shortcuts.md §4 (Note).

## F-1901 · UX · MIDI-Ausgänge lassen sich nirgends ein- oder ausschalten

- **Status:** offen
- **Code:** `src/ui/fk/ui/AudioSettingsComponent.cpp:17-19` (JUCE-Auswahl mit `showMidiOutputSelector = false`, eigene Liste nur für Eingänge); `src/services/fk/services/MidiService.h` kennt keine Ausgänge. Tracktion schaltet neue MIDI-Ausgänge selbst ein (`tracktion_MidiOutputDevice.cpp:267`) und schaltet einen Ausgang ab, der sich nicht öffnen lässt (`tracktion_DeviceManager.cpp:458-469`).
- **Beobachtung:** Die Audio-Einstellungen zeigen keine MIDI-Ausgänge. Ein abgeschalteter Ausgang lässt sich in der Oberfläche nicht wieder einschalten.
- **Erwartung:** Liste **MIDI outputs** mit Schaltern unter den MIDI-Eingängen.
- **Handbuch:** studio-and-settings.md §3.11; tracks.md §5.3.

## F-1902 · UX · Studio-Verbindungen: MIDI-Ein- und Ausgänge nicht benennbar

- **Status:** offen
- **Code:** `src/services/fk/services/TrackService.cpp:256-281` (`getStudioPorts` nur Wave-Geräte), `:283-307` (`setPortAlias` ebenso); die Auswahllisten nutzen für MIDI schon `getAliasOrName()` (`:174`, `:854`).
- **Beobachtung:** Audio-Anschlüsse bekommen eigene Namen, MIDI-Geräte nicht, obwohl gerade kryptische Treibernamen („Midi Through Port-0“) davon profitieren würden.
- **Erwartung:** Abschnitte **MIDI inputs**/**MIDI outputs** im Dialog, Alias über `setAlias`.
- **Handbuch:** studio-and-settings.md §4.2; Szene `StudioAndSettingsScenes.cpp` (Bild `studio-and-settings/studio-connections`).

## F-1903 · Text · Studio-Verbindungen: „name from the driver“ ist „Input 3“

- **Status:** offen
- **Code:** `src/ui/fk/ui/StudioConnectionsDialog.cpp:37` (Tooltip „Leave empty for the name from the driver“); angezeigt wird ohne eigenen Namen `channelName` aus `src/services/fk/services/TrackService.cpp:29-38` („Input 3“, „Output 3+4“), nicht der Treibername.
- **Erwartung:** z. B. „Leave empty for the standard name (Input 3)“.
- **Handbuch:** studio-and-settings.md §4.2; getting-started.md §5.8.

## F-1904 · Unstimmigkeit · MIDI-Learn-Button ohne Projekt aktiv, Menüeintrag grau

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:2121` (`itemMidiLearn` nur mit `hasProject`), `updateToolbar` (`:2762`) setzt `midiLearnButton` nie auf `setEnabled`; `MidiLearnService::projectChanged` schaltet den Lernmodus beim Öffnen eines Projekts wieder ab.
- **Beobachtung:** Ohne Projekt lässt sich der Button einschalten (rot, „Touch a control…“), es gibt aber nichts zu verknüpfen; beim Anlegen eines Projekts geht er still aus.
- **Erwartung:** Button wie der Menüeintrag nur mit Projekt aktiv.
- **Handbuch:** studio-and-settings.md §2 (Note).

## F-1905 · Text · Testton-Frequenz mit sieben Nachkommastellen

- **Status:** offen
- **Code:** `src/ui/fk/ui/AudioSettingsComponent.cpp:45-48` (`frequencySlider` ohne `setNumDecimalPlacesToDisplay`).
- **Beobachtung:** Neben dem Regler steht „440.0000000 Hz“.
- **Erwartung:** `frequencySlider.setNumDecimalPlacesToDisplay (0)` → „440 Hz“.
- **Handbuch:** studio-and-settings.md §3.9 (Note); Szene `GettingStartedScenes.cpp` (Bild `getting-started/audio-settings`, Zeile 10).

## F-1906 · UX · Fenster „MIDI mappings“ rollt nicht und ist nicht vergrößerbar

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp` `showMidiMappings` (ab `:2656`): feste Größe 460 × 320 (`:2673`, `options.resizable = false` `:2747`), Zeilen ohne Viewport.
- **Beobachtung:** Es passen neun Zuordnungen hinein; weitere sind unsichtbar und lassen sich nicht einzeln entfernen (nur **Remove all**). Lange Parameternamen werden abgeschnitten.
- **Erwartung:** Liste in einem Viewport, Fenster vergrößerbar.
- **Handbuch:** studio-and-settings.md §5.3 (Note).

## F-1907 · Text · Deutsche Texte im Diagnosebericht und in Updates.txt

- **Status:** offen
- **Code:** `src/ui/fk/ui/ProjectComponent.cpp:1195` (Dateiname „FK-Diagnose …“), `src/services/fk/services/DiagnosticsService.cpp:41-87` (System.txt: „Erstellt“, „Betriebssystem“, „Kerne“, „Audiogerät“ …), `:102-104` (ZIP-Ordner „Haenger“, „Absturz“), `src/services/fk/services/UpdateService.cpp:35` („an“/„aus“).
- **Beobachtung:** In der englischen Oberfläche entsteht eine „FK-Diagnose“-Datei mit deutschem Inhalt.
- **Erwartung:** englische, neutrale Namen und Texte („FK-Diagnostics“, „Hangs“, „Crashes“, Updates.txt „on“/„off“ mit Lesen der alten Werte). Zusammen mit F-102/F-306 beheben.
- **Handbuch:** studio-and-settings.md §8.2; getting-started.md §8.

## F-1908 · UX · `--scan-plugins`: deutsche Ausgabe, nur Effekte, unter Windows unsichtbar

- **Status:** offen (Windows-Teil ungeprüft)
- **Code:** `src/app/Application.cpp:250-270` (`runHeadlessPluginScan`: „Plugins vorher/nachher“, „fehlgeschlagen“, Liste nur `getAvailableEffects()`); `src/app/CMakeLists.txt` (`juce_add_gui_app`, unter Windows ohne Konsole).
- **Beobachtung:** Die Ausgabe ist deutsch, Instrumente fehlen. Unter Windows erscheint `std::cout` vermutlich nicht. Ein abstürzendes Plugin beendet den Scan.
- **Erwartung:** englische Ausgabe, Instrumente und Effekte, unter Windows `AttachConsole` oder Ausgabe ins Log.
- **Handbuch:** studio-and-settings.md §11.
