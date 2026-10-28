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
