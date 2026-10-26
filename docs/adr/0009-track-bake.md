# ADR 0009: Track Bake auf Tracktions Freeze, mit eigener Sperre und Gruppen-Bake

Datum: 2026-10-11 · Status: angenommen

## Kontext
"Track Bake" soll ein Alleinstellungsmerkmal werden: Teile einer Spur werden zu Audio gerendert ("gebacken"), damit sie keine Rechenleistung mehr kosten, und mit "Bake lösen" wieder live. Anforderungen:

1. **Bake Track** (nur Instrumentenspuren): Instrument mit aktuellen Einstellungen zu einer WAV rendern. Inserts bleiben live und bearbeitbar.
2. **Bake Inserts** (Audio- und Instrumentenspuren): Insert-Kette bis Slot N rendern. Auf Instrumentenspuren wird das Instrument automatisch mitgebacken.
3. **Einzelne Slots wählen, aber linear:** Wer Slot 1 und 4 wählt, backt 1 bis 4, sonst stimmt die Reihenfolge der Kette nicht mehr.
4. **Gruppen-Bake:** Eine Gruppe wird mit allen Spuren gebacken, die (auch über Zwischengruppen) in sie münden.
5. **Gesperrt solange gebacken:** Gebackene Teile (Instrument, Inserts, Clips, und bei Gruppen die zuführenden Spuren) sind nicht bearbeitbar, bis "Bake lösen" alles freigibt.

Tracktion bringt Freeze mit: Ein `FreezePointPlugin` in der Plugin-Liste markiert, bis wohin gerendert wird. `AudioTrack::setFrozen (true, individualFreeze)` rendert Clips (und zuführende Spuren) durch alle Plugins vor dem Punkt in eine Datei im Temp-Ordner des Edits. Danach spielt die Spur diese Datei ab; Plugins vor dem Punkt sind `isFrozen()` und laufen nicht mehr im Graphen, Plugins dahinter bleiben live. Der Temp-Ordner ist bei uns der `Audio/`-Ordner des Projekts (`ProjectService`), die Datei bleibt also mit dem Projekt erhalten.

Zwei Grenzen von Tracktion: (a) Eine Spur, die in eine andere Spur (Gruppe) ausgibt, lehnt Tracktion ab, weil sein Renderer nur Spuren mit Ausgabegerät rendert. Beim Abspielen funktioniert eine gefrorene Spur dagegen auch vor einer Gruppe. (b) Tracktion kennt keine Bearbeitungssperre.

## Entscheidung
1. **Tracktions Freeze ist die Engine.** Kein eigener Renderer, kein eigener Abspielknoten. Wir setzen den Freeze-Punkt selbst: hinter das Instrument (Bake Track) oder hinter den N-ten Insert (Bake Inserts).
2. **Spuren vor einer Gruppe** werden für die Dauer des Renderns auf den Master gelegt und danach zurück. Das geschieht ohne Undo-Schritte.
3. **Ein `BakeService`** besitzt den Anwendungsfall: `bakeTrack`, `bakeInserts (track, slotCount)`, `unbake`, `getBakeState`, `isLocked`. Er speichert die Bake-Art als Spureigenschaft `fkBake` (`track` oder `inserts`) und die Slot-Zahl als `fkBakeSlots`, damit die Oberfläche sie nach dem Laden zeigt.
4. **Die Sperre prüfen die Services**, nicht die Oberfläche: `PluginService` (einsetzen, entfernen, umgehen, Instrument tauschen, Editor öffnen), `ClipService` und `MidiClipService` lehnen Änderungen an gesperrten Spuren mit einer verständlichen Meldung ab. Die Oberfläche zeigt die Sperre (Schloss, ausgegraute Slots) und erklärt sie im Tooltip.
5. **Linearität erzwingt der Service:** Die Oberfläche markiert Slots; der Service backt immer 1 bis zum höchsten markierten Slot.
6. **Gruppen-Bake** friert die Gruppe ein (Tracktion rendert die zuführenden Spuren mit). Die zuführenden Spuren werden nicht selbst eingefroren; sie sind gesperrt, solange eine Gruppe, in die sie (auch indirekt) münden, gebacken ist. Weil der Graph der gefrorenen Gruppe die Zuführungen nicht mehr anfordert, kosten auch sie keine Rechenleistung.
7. **Bake ist kein Undo-Schritt.** Rendern ist teuer und erzeugt Dateien; Rückgängig ist "Bake lösen".

## Folgen
- Neuer Service `services::BakeService`, Tests in `tests/services/BakeServiceTest.cpp`.
- Bake-Buttons im Spurkopf und im Inspector (mit Symbol und Tooltip, Guideline 10a); im Inspector je Insert-Slot eine Markierung "bis hier backen".
- Plugins vor einem Freeze-Punkt bleiben geladen (Speicher), laufen aber nicht. Ein späteres "Entladen beim Bake" wäre eine eigene Entscheidung.
- Sends vor dem Freeze-Punkt verhindern den Bake (Tracktion meldet das); unsere Sends liegen hinter den Inserts, das betrifft also nur Sonderfälle.
