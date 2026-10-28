# Pflege des Benutzerhandbuchs

Das Handbuch (`docs/manual/`) ist für Endnutzer und **englisch**. Diese Datei ist für uns und deutsch.
Grundsatz: **Ein Feature ist erst fertig, wenn sein Kapitel stimmt.** Text, Bilder und Code ändern sich im selben Commit.

## Aufbau

```
docs/manual/
  README.md            Inhaltsverzeichnis (Reihenfolge, Status je Kapitel)
  MAINTAINING.md       diese Datei
  FINDINGS.md          Bugs und Unstimmigkeiten, beim Schreiben gefunden (F-NNN)
  <kapitel>.md         ein Kapitel je Bereich, Dateiname = Slug (key-editor.md)
  images/<kapitel>/    PNGs, nur von Szenen erzeugt, nie von Hand
tests/manual/
  ManualShot.h         saveShot, findByTooltip, Markierungen
  <Bereich>Scenes.cpp  eine Szene je Bild, Tag [manual]
tools/manual.py        check | coverage | shots | findings | export | hook
tools/manual_export.py HTML/PDF-Erzeugung (läuft in build/manual-venv)
```

## Kapitelkopf

Jedes Kapitel beginnt mit einem Kopf. Ohne Kopf ist eine Datei kein Kapitel (README, MAINTAINING).

```yaml
---
title: Key Editor
reviewed: 2026-10-28          # zuletzt gegen den Code geprüft
sources:                      # Code, den das Kapitel beschreibt (Globs ab Repo-Wurzel)
  - src/ui/fk/ui/KeyEditor.*
  - tests/manual/KeyEditorScenes.cpp
images:                       # alle Bilder, die der Text zeigt (ohne images/ und .png)
  - key-editor/overview
---
```

- `sources` großzügig fassen: UI-Klassen, die Services mit Bedienlogik (Grenzen, Standardwerte) und die eigene Szenen-Datei.
- `reviewed` setzen, wenn ein Kapitel geprüft wurde und nichts zu ändern war. Das macht es wieder aktuell.

## Kapitel schreiben

Reihenfolge innerhalb eines Kapitels, damit alle gleich aussehen (Muster: `key-editor.md`):

1. Ein Satz, wofür der Bereich da ist. Danach **In this chapter** mit nummerierten Abschnitten.
2. **Öffnen/Finden**: jeder Weg (Doppelklick, Menü, Taste, Button).
3. **At a glance**: Übersichtsbild mit Nummern und eine Tabelle `# | Element | What it does | Details`.
4. Ein Abschnitt je Funktion: was sie tut, nummerierte Schritte, alle Optionen mit Standardwert, Grenzen und Sonderfälle („Note:“, „Tip:“, „Example:“).
5. **Undo** und Fehlermeldungen, soweit es Besonderheiten gibt.
6. **Quick reference** als Tabelle am Ende.

Regeln:

- Jede Angabe stammt aus dem Code, nicht aus der Erinnerung: Texte und Tooltips wörtlich (`TRANS(...)`), Standardwerte, Grenzen, Tastenkürzel (`Commands.cpp`).
- Bildschirmtexte **fett**, Menüpfade mit `›`, Tasten **Ctrl+A**.
- Sprache einfach und direkt, Imperativ in Schritten („Click **Quantize**.“), keine internen Begriffe (Service, Komponente).
- Abschnittsnummern sind Teil der Anker. Beim Einfügen eines Abschnitts die Querverweise (`[§12](#12-quantizing)`) mitziehen.
- Neues Kapitel: Datei anlegen, in `README.md` verlinken und Status auf *written* setzen.

## Bilder

Bilder entstehen nur aus Szenen in `tests/manual/`, damit sie jederzeit gleich neu gebaut werden können.

- Eine Szene baut über die Services ein festes Demo-Projekt (Namen, Tonart, Noten), zeichnet die echte Komponente und ruft `saveShot (component, "<kapitel>/<bild>", callouts)` auf.
- Markierungen: `calloutFor (nummer, root, findByTooltip (root, "Tooltip-Anfang"), Callout::Placement::above | inside)`. `above` für Leisten (Nummer im Rand über dem Bild), `inside` für Flächen (Rahmen und Nummer in der Ecke). Die Nummern entsprechen der Tabelle im Text.
- `findByTooltip` sucht Elemente über den Anfang ihres englischen Tooltips. Ändert sich ein Tooltip oder fehlt ein Element, schlägt die Szene fehl: Das ist gewollt, dann sind Text und Bild zu prüfen.
- Szenen starten nur mit `FK_MANUAL_IMAGE_DIR`, im normalen Testlauf werden sie übersprungen.
- Erzeugen: `tools/manual.py shots` (nutzt `build/linux-release`, anderer Ordner über `FK_BUILD_DIR`). `--build` baut vorher `fk_tests` mit `-j3`. Bauen dauert Minuten, vorher fragen.
- Danach die Bilder ansehen und mit `git diff --stat docs/manual/images` prüfen, welche sich geändert haben.

## Abgleich

```
tools/manual.py check      # veraltete Kapitel, fehlende/verwaiste Bilder, Kopf ↔ Text
tools/manual.py coverage   # UI-Quellen ohne Kapitel (zeigt, was noch fehlt)
```

Ein Kapitel ist **veraltet**, wenn eine Datei aus `sources` nach dem letzten Commit des Kapitels geändert wurde oder gerade uncommittet geändert ist, während das Kapitel unverändert ist.

Der Stop-Hook (`.claude/settings.json`, `tools/manual.py hook`) meldet sich, wenn eigene, noch nicht gepushte Änderungen ein Kapitel veraltet machen.

## Funde (Bugs, Unstimmigkeiten)

Beim Schreiben fällt viel auf. **Nicht nebenbei fixen**, sondern in `FINDINGS.md` festhalten; der Code-Agent arbeitet die Liste am Ende ab. Das Kapitel beschreibt bis dahin das aktuelle Verhalten.

- Jede betroffene Handbuchstelle bekommt direkt davor `<!-- FIX F-NNN: was sich ändern würde -->`, betroffene Szenen `// FIX F-NNN`.
- Nach dem Fix: `tools/manual.py findings` zeigt alle Stellen eines Funds; dort Text/Szene anpassen, Marker löschen, Bilder neu erzeugen, Status **behoben** (Commit).
- `check` meldet Marker ohne Fund und Marker zu bereits behobenen Funden.

## Export (HTML, PDF)

```
tools/manual.py export          # beides
tools/manual.py export pdf      # nur PDF
```

Ergebnis: `build/manual/fk-audiosuite-manual.html` (eine Datei, Bilder eingebettet) und `.pdf` (A4, Deckblatt mit Version aus `CMakeLists.txt`, Inhaltsverzeichnis, jedes Kapitel auf neuer Seite). Aufgenommen werden die Kapitel, die in `README.md` verlinkt sind, in dieser Reihenfolge. Beim ersten Lauf entsteht `build/manual-venv` (markdown, weasyprint, rund 90 MB).

Bilder sind auf A4 rund 700 px breit dargestellt: Szenen höchstens etwa 1000 px breit anlegen, breite Leisten als Ausschnitt (`saveShot (…, area)`) in mehrere Bilder teilen.

## Ablauf bei einer Änderung

1. Code ändern.
2. `tools/manual.py check` zeigt betroffene Kapitel.
3. Nur die betroffenen Abschnitte lesen und anpassen; neue Funktionen bekommen einen eigenen Abschnitt.
4. Szene anpassen, wenn sich das Bild ändert oder ein neues Bild nötig ist (Kopf `images:` ergänzen).
5. Bilder erzeugen (Build erfragen), ansehen.
6. `reviewed:` auf heute, `tools/manual.py check` ohne offene Punkte, `tools/manual.py export` läuft durch; alles in einem Commit.
7. `CHANGELOG.md` wie gewohnt; das Handbuch verweist nicht auf Versionen.
