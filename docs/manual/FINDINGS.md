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
