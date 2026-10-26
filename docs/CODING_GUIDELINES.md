# Code-Richtlinien: der eine Stil

C++ bietet für fast alles mehrere Wege. Dieses Dokument legt für jedes Thema **genau einen** fest. Wer einen anderen Weg will, ändert erst dieses Dokument (mit ADR), dann den Code. Die Regeln sind so gewählt, dass sie zu JUCE und Tracktion Engine passen, damit unser Code und der Framework-Code gleich aussehen.

Kurzfassung für Ungeduldige: **Werte statt Zeiger, `unique_ptr` statt `new`, `const` überall, Rule of Zero, Konstruktor-Injektion, `Result` statt Ausnahmen, nichts Dynamisches im Audio-Thread, clang-format entscheidet die Formatierung.**

---

## 0. Grundsätze

1. **Lesbarkeit vor Cleverness.** Code wird zehnmal öfter gelesen als geschrieben.
2. **Eine Lösung pro Problem.** Steht hier ein Weg, gibt es keinen zweiten.
3. **Framework-Konvention schlägt persönlichen Geschmack.** Wo JUCE eine Konvention hat, übernehmen wir sie.
4. **Werkzeuge statt Diskussion.** clang-format formatiert, clang-tidy prüft, Compiler-Warnungen sind Fehler.
5. **SOLID**, angewandt:
   - **S**ingle Responsibility: eine Klasse, ein Grund sich zu ändern. Wenn der Klassenname ein "und" braucht, sind es zwei Klassen.
   - **O**pen/Closed: Erweiterung über neue Klassen hinter einer Schnittstelle, nicht über `if (type == ...)`.
   - **L**iskov: Wer eine Schnittstelle implementiert, hält ihren Vertrag vollständig ein.
   - **I**nterface Segregation: Kleine Schnittstellen. Ein `TransportListener` statt ein `EverythingListener`.
   - **D**ependency Inversion: Höhere Schichten hängen von Schnittstellen ab, die sie selbst definieren. Konkrete Klassen werden im `app`-Modul zusammengesteckt.

## 1. Sprache und erlaubte Features

Standard: **C++23**, aber nur dieser Feature-Satz. Alles andere braucht eine ADR.

| Erlaubt und erwünscht | Verboten |
|---|---|
| `auto` (siehe Regel 4.3), `constexpr`, `enum class`, `nullptr` | `new`/`delete` von Hand, `malloc` |
| `std::unique_ptr`, `std::optional`, `std::expected`, `std::span`, `std::string_view` | C-Casts `(int) x`, `reinterpret_cast` außer in `core` mit Kommentar |
| Range-based for, strukturierte Bindungen `auto [a, b] = ...` | `goto`, Makros (außer JUCE-eigene wie `JUCE_DECLARE_NON_COPYABLE`) |
| Lambdas, `std::function` an Schnittstellen | Mehrfachvererbung außer von reinen Schnittstellen |
| `[[nodiscard]]`, `override`, `final`, `explicit`, `noexcept` wo sinnvoll | `using namespace` in Headern |
| Einfache Templates für Container/DSP-Bausteine | Template-Metaprogrammierung, SFINAE, Concepts (vorerst) |
| `std::array`, `std::vector`, `std::unordered_map` | Rohe C-Arrays, außer `constexpr` Tabellen |
| `std::atomic`, `juce::AbstractFifo` | `std::mutex` im Audio-Pfad, `volatile` |
| `static_assert` | Globale Variablen, Singletons (außer JUCE-Pflicht) |

Ausnahmen (`throw`) werfen wir **nie** selbst. Wir fangen sie nur an Grenzen zu Fremdcode und wandeln sie in `Result` um.

## 2. Dateien und Struktur

- Eine Klasse pro Dateipaar: `Biquad.h` und `Biquad.cpp`. Dateiname = Klassenname.
- Header beginnen mit `#pragma once`.
- Include-Pfad immer vom Modul-Wurzel: `#include "fk/dsp/filters/Biquad.h"`. Nie relativ (`../`).
- Include-Reihenfolge (clang-format sortiert automatisch): 1. eigene `fk/…`, 2. JUCE/Tracktion, 3. Catch2, 4. Standardbibliothek.
- Namespaces: `fk::<modul>` entsprechend Ordner. `fk::dsp`, `fk::ui::arrange`. Nie tiefer als drei Ebenen.
- Header enthalten nur, was die Schnittstelle braucht. Implementierungsdetails in die `.cpp`. Vorwärtsdeklarationen statt Includes, wo möglich.
- Dateien bleiben unter 400 Zeilen. Darüber: aufteilen.

## 3. Namensgebung

| Was | Stil | Beispiel |
|---|---|---|
| Klassen, Structs, Enums, Typaliase | `PascalCase` | `AudioTrack`, `MeterValues` |
| Funktionen, Methoden | `camelCase`, Verb zuerst | `createTrack()`, `isPlaying()`, `getSampleRate()` |
| Variablen, Parameter, Member | `camelCase`, kein Präfix (JUCE-Stil) | `sampleRate`, `trackCount` |
| Konstanten (`constexpr`) | `camelCase` | `constexpr int maxInsertSlots = 8;` |
| Enum-Werte | `camelCase` | `enum class ToolMode { select, split, draw };` |
| Namespaces | `lowercase` | `fk::dsp` |
| Template-Parameter | `PascalCase` | `template <typename SampleType>` |
| Schnittstellen (rein virtuell) | **ohne** `I`-Präfix, Substantiv | `AudioDevice`, Implementierung `JuceAudioDevice` |
| Dateien | wie die Klasse | `JuceAudioDevice.h` |
| Tests | `<Klasse>Test.cpp` | `BiquadTest.cpp` |
| Boolesche Funktionen | `is…`, `has…`, `can…` | `isArmed()`, `hasSolo()` |

Namen sagen, **was** etwas ist, nicht wie es gespeichert ist. `tracks`, nicht `trackVector`. Keine Abkürzungen außer etablierten: `id`, `midi`, `dsp`, `db`, `ui`.

Konstruktorparameter, die einen Member setzen, heißen wie der Member mit Präfix `new…` oder sprechend: `explicit Track (juce::String trackName)`. Niemals `this->name = name` als Krücke.

## 4. Formatierung und Deklarationen

### 4.1 Formatierung
clang-format mit der `.clang-format` im Repo. Punkt. Allman-Klammern, 4 Leerzeichen, 120 Zeichen, Zeiger am Typ (`Type* ptr`), Leerzeichen vor nicht-leeren Klammern wie bei JUCE: `setBounds (area)`, aber `resized()`. Vor jedem Commit: `clang-format -i` auf geänderte Dateien.

### 4.2 `const`
Alles ist `const`, was nicht geändert werden muss: lokale Variablen, Parameter (bei Referenzen und Zeigern), Member-Funktionen, die nichts ändern. `const` zuerst lesen lernen ist Thema 5 in LEARNING.md.

### 4.3 `auto`
`auto` **nur**, wenn der Typ auf der rechten Seite sichtbar ist oder irrelevant ist:
```cpp
auto track = std::make_unique<AudioTrack> (name);   // Typ steht rechts
auto it = tracks.begin();                            // Iterator-Typ ist egal
for (const auto& clip : clips)                       // Element-Typ klar aus Kontext
const double gainDb = computeGain (input);           // Typ NICHT sichtbar → explizit
```

### 4.4 Initialisierung
Member werden **in der Klasse** mit Standardwert initialisiert, nicht im Konstruktor:
```cpp
class Fader
{
    float gainDb = 0.0f;      // hier, nicht im Konstruktor
    bool muted = false;
};
```
Konstruktor-Initialisierungslisten nur für Werte, die vom Konstruktorparameter abhängen, in Deklarationsreihenfolge. Geschweifte Klammern für Werte `int x { 5 };` vermeiden wir zugunsten von `= 5` (eine Schreibweise).

## 5. Klassen

- **Rule of Zero**: Klassen deklarieren weder Destruktor noch Copy/Move, wenn alle Member sich selbst verwalten (`unique_ptr`, `vector`, …). Braucht eine Klasse doch einen Destruktor (JUCE-Listener abmelden!), dann wird sie nicht kopierbar: `JUCE_DECLARE_NON_COPYABLE (ClassName)`.
- Jeder Einargument-Konstruktor ist `explicit`.
- Klassen sind `final`, außer sie sind als Basisklasse gedacht.
- Basisklassen mit virtuellen Methoden haben `virtual ~Base() = default;`.
- Jede überschreibende Methode hat `override`, nie zusätzlich `virtual`.
- Vererbung nur für Schnittstellen (rein virtuelle Klassen) und wo JUCE es verlangt (`juce::Component`, `juce::AudioProcessor`). **Nie** zur Code-Wiederverwendung, dafür: Komposition.
- Abfragen (Getter) sind `[[nodiscard]]` und `const`. Keine reflexhaften Getter/Setter: Ein Member bekommt nur dann Zugriffsmethoden, wenn ein Aufrufer sie braucht.
- Reihenfolge in der Klasse: `public` (Typen, Konstruktoren, Methoden) → `private` (Methoden, dann Member). Member ganz unten.
- Structs nur für reine Datenbündel ohne Invarianten: `struct TimeRange { double start; double end; };`

## 6. Besitz und Übergabe (Ownership)

Das wichtigste Thema für Java-Umsteiger. Genau diese vier Formen, nichts anderes:

| Form | Bedeutung | Beispiel |
|---|---|---|
| `std::unique_ptr<T>` | **besitzt** das Objekt, genau ein Besitzer | `std::unique_ptr<Engine> engine;` |
| `T&` / `const T&` | **leiht** sich das Objekt, muss vorhanden sein, kein Besitz | `void render (const AudioBuffer& buffer)` |
| `T*` | **leiht** sich das Objekt, darf `nullptr` sein, kein Besitz | `Track* findTrack (TrackId id)` |
| `T` (Wert) | **Kopie**, für kleine Typen und wenn Übergabe Besitz überträgt | `void setName (juce::String name)` |

Regeln:
- Ein roher Zeiger besitzt **nie** etwas. Wer `delete` schreibt, hat einen Fehler gemacht.
- `std::shared_ptr` nur, wenn Lebensdauer wirklich geteilt ist. Das ist selten. Jede Verwendung bekommt einen Kommentar mit Begründung.
- Funktionsparameter: kleine Typen (≤ 16 Byte: `int`, `double`, `TrackId`) als Wert; alles andere als `const T&`. Wenn die Funktion das Objekt behält, als Wert übernehmen und per `std::move` einlagern.
- Rückgabe: als Wert (Compiler optimiert), `Result<T>` bei Fehlerfall, `T*` wenn "nicht gefunden" normal ist.
- Puffer: `std::span<float>` statt `float*, int size`.
- Strings: `juce::String` überall, wo JUCE beteiligt ist (UI, Dateien). `std::string_view` nur in `core`/`dsp` als Parameter. Nie `const char*` als Parameter.
- **Umlaute in Literalen** gehen nie direkt in `juce::String` (das prüft auf ASCII und warnt): `juce::String::fromUTF8 ("...")` benutzen, Fehlertexte über `services::errorText()`. Reine `std::string`-Pfade (core) sind unproblematisch. Sichtbare Texte sind seit ADR 0010 englisch (§15) und damit ASCII.
- `std::unique_ptr<T, Deleter>` mit eigenem Deleter, wenn ein Framework beim Freigeben einen Vertrag verlangt (Beispiel `services::EditorDeleter`). Das ist der einzige erlaubte Ort für ein `delete`.

## 7. Fehlerbehandlung

```cpp
// core/Result.h
using Result<T> = std::expected<T, Error>;
```
- Erwartbare Fehler (Datei fehlt, Gerät belegt, Plugin lädt nicht) → `Result<T>` zurückgeben. Aufrufer **muss** prüfen (`[[nodiscard]]`).
- Programmierfehler (Index außerhalb, `nullptr` wo keiner sein darf) → `jassert (bedingung);` Das ist kein Fehlerpfad, das ist ein Bug. In `core` und `dsp` gibt es kein JUCE, dort heißt es `assert (bedingung);` aus `<cassert>`. Gleiche Bedeutung, gleiche Regel.
- Nie: Rückgabe von `-1`, `false` oder leerem String als Fehlerkennzeichen.
- Nie: Fehler verschlucken. Entweder behandeln oder nach oben weiterreichen.

## 8. Echtzeit-Regeln (Audio-Thread)

Gilt für `processBlock`, alles in `dsp`, alles, was Tracktion aus dem Graphen aufruft. Verstöße sind die Ursache für Knackser und Aussetzer.

**Verboten im Audio-Thread:**
- Speicher anfordern oder freigeben: kein `new`, `make_unique`, `vector::push_back`, `juce::String`, `std::function`-Erzeugung, Lambdas mit Capture-by-Value großer Objekte.
- Sperren: kein `std::mutex`, `juce::CriticalSection`, `MessageManagerLock`.
- I/O: keine Dateien, kein Logging, kein `DBG`, keine Konsole.
- Ausnahmen, `dynamic_cast`, Systemaufrufe.
- Blockierendes Warten auf einen anderen Thread.

**Erlaubt:**
- Rechnen auf vorab allokierten Puffern (in `prepareToPlay` anlegen).
- `std::atomic<float>::load/store` für Parameter.
- `juce::AbstractFifo` für Nachrichten in beide Richtungen.
- `juce::SmoothedValue` für knackfreie Parameteränderungen.

Jede Klasse mit Audio-Thread-Code hat oben im Header einen Kommentar: `// Echtzeit: processBlock() läuft im Audio-Thread, siehe CODING_GUIDELINES.md §8`.

## 9. Funktionen

- Kurz. Eine Funktion tut eine Sache auf einer Abstraktionsebene. Über 40 Zeilen ist ein Signal zum Aufteilen.
- Maximal 4 Parameter. Mehr → ein `struct` mit benannten Feldern.
- Keine Ausgabeparameter (`void f (int& result)`). Ergebnis wird zurückgegeben, mehrere Ergebnisse als `struct`.
- Keine booleschen Parameter: `setLoop (true)` sagt nichts. Stattdessen `enum class LoopMode { off, on }`.
- Keine Default-Argumente bei virtuellen Funktionen.
- Frühe Rückkehr (Guard Clauses) statt tief verschachtelter `if`.

## 10. Kommentare

- **Sprache: Deutsch** (ADR 0006). Bezeichner bleiben Englisch.
- Kommentare erklären **warum**, nicht was. `i++; // erhöht i` ist verboten.
- Jede öffentliche Klasse und Methode hat einen Doxygen-Kommentar mit `///`:
  ```cpp
  /// Ein Biquad-Filter zweiter Ordnung (Direct Form II transposed).
  /// Verarbeitet einen Kanal; für Stereo zwei Instanzen anlegen.
  class Biquad final
  ```
- Lernhinweise, die über das Verständnis des Codes hinausgehen, gehören in `LEARNING.md`, nicht in den Code. Der Code verlinkt dorthin: `// → LEARNING.md §6.2 (Rule of Zero)`.
- Kein auskommentierter Code. Git merkt sich alles.
- `// TODO(felix): Beschreibung` für offene Punkte, mit Namen.

## 10a. Oberfläche: jede Funktion sichtbar

- **Keine Funktion nur per Tastenkürzel.** Alles hat einen Button mit Symbol (`ui::IconButton`) und Tooltip; der Tooltip nennt das Kürzel in Klammern, z. B. "Inspector ein/aus (F2)".
- **Leere Zustände erklären sich.** Ein Bereich ohne Inhalt (Inspector ohne Spur, Projekt ohne Spuren) zeigt in ein, zwei Sätzen, was der Nutzer tun kann. Nie eine leere Fläche.
- **Auswahl sinnvoll vorbelegen.** Neue oder einzige Objekte sind sofort ausgewählt (neue Spur → Inspector zeigt sie).
- Symbole sind Vektorpfade in `ui/Icons.cpp`, eingefärbt über Theme-Tokens; keine Bitmaps.
- **Plugin-Oberflächen aus Bausteinen.** Ein Plugin beschreibt seine Oberfläche in `describePanel()` als Daten (Abschnitte mit Parameter-IDs, `plugins/ui/PanelDescription.h`); Knob, Switch, Choice, Meter und ModulePanel kommen aus `plugins/ui/`. Kein `GenericAudioProcessorEditor`, kein Layout-Code im Plugin. Visuelle Szenen (Raum, Spektrum) ergänzen die Regler, ersetzen sie nie.

## 11. Tests

- Framework: Catch2 v3. Jede Datei in `dsp` und `services` hat eine Testdatei unter `tests/` im gleichen Pfad.
- Testnamen beschreiben Verhalten: `TEST_CASE ("Biquad: Tiefpass dämpft Frequenzen über dem Cutoff", "[dsp]")`.
- Ein Test, eine Aussage. Vorbereitung → Aktion → Prüfung, getrennt durch Leerzeilen.
- DSP-Tests prüfen mit Toleranz (`Catch::Approx`), nie exakte Gleitkomma-Gleichheit.
- UI wird nicht automatisiert getestet (vorerst). Dafür muss die Logik aus Komponenten heraus in Services.

## 12. Git

- Commit-Nachrichten nach **Conventional Commits**: `feat(dsp): Biquad-Filter mit Tiefpass und Hochpass`, `fix(ui): Clip-Auswahl bei Zoom`, `docs:`, `refactor:`, `test:`, `build:`.
- Kleine Commits, die bauen und deren Tests grün sind.
- Branch `main` ist immer baufähig.

## 13. Werkzeuge

| Werkzeug | Zweck | Wann |
|---|---|---|
| `clang-format` | Formatierung | vor jedem Commit |
| `clang-tidy` | Statische Analyse (Checks in `.clang-tidy`) | im Editor live, in CI |
| `-Wall -Wextra -Wpedantic -Werror` | Compiler-Warnungen als Fehler | immer |
| AddressSanitizer, UBSanitizer | Speicherfehler, undefiniertes Verhalten | Preset `linux-asan`, vor jedem Meilenstein |
| ThreadSanitizer | Datenrennen | wenn Audio/UI-Kommunikation geändert wurde |
| `gdb` / IDE-Debugger | | |

## 14. Review-Checkliste

Vor jedem Commit, in dieser Reihenfolge:

1. Baut ohne Warnungen? clang-format gelaufen?
2. Jeder rohe Zeiger: ist er wirklich nur geliehen und darf `nullptr` sein?
3. Jede Funktion: ein Zweck, unter 40 Zeilen, keine bool-Parameter?
4. Jedes `const`, das fehlt?
5. Audio-Thread-Code: nichts aus der Verbotsliste in §8?
6. Fehlerfälle als `Result`, nicht als magische Werte?
7. Tests für Logik vorhanden und grün? Und im Testlauf **keine** Zeile "JUCE Assertion failure" (`./fk_tests 2>&1 | grep -c "Assertion failure"` muss 0 liefern)?
8. Kommentare sagen "warum"? Lernstoff in LEARNING.md, nicht im Code?
9. Neue sichtbare Texte englisch mit `TRANS` und deutsche Zeile in der Tabelle (§15)?

## 15. Texte und Übersetzung (ADR 0010)

Jeder Text, den ein Nutzer sieht, steht im Code **auf Englisch** und läuft durch die Übersetzung. Genau diese Formen:

| Fall | Schreibweise |
|---|---|
| fester Text | `TRANS ("Save project")` |
| Text mit Werten | `i18n::format (TRANS ("Track {name} deleted"), { { "name", track.name } })` |
| Anzahl | `i18n::plural (count, "{count} track", "{count} tracks")` (erstes Argument eine einfache Variable) |
| Text, der erst später angezeigt wird (Tabellen, Choice-Listen) | im Literal `NEEDS_TRANS ("Off")`, beim Anzeigen `juce::translate (text)` |
| Anzeigetext aus Content-JSON (`description`, Katalognamen) | JSON englisch, beim Anzeigen `juce::translate (model.description)`, Feld im Übersetzungstest eintragen |
| englischer Text mit Nicht-ASCII-Zeichen | `TRANS (juce::String::fromUTF8 ("4×12 cabinet"))`, sparsam |
| Fehlermeldung eines Service | `core::fail (code, i18n::format (TRANS ("Could not open {file}"), { { "file", path } }).toStdString())`, also übersetzt, bevor sie in `Error` geht |

Regeln:
- **Ein Literal je Text**, keine zusammengehängten Literale (`"a" "b"`) und keine zusammengesetzten Sätze. Lange Sätze bleiben eine Zeile; clang-format bricht Literale nicht um.
- Die deutsche Zeile kommt im selben Commit in die Tabelle der Ebene (`src/plugins/lang/de.txt`, `resources/lang/de.txt`, `common/lang/de.txt`, `<produkt>/lang/de.txt`); ein Text, den schon eine vorher gesetzte Tabelle hat (Fundament), steht nicht noch einmal da. Der Test `Übersetzungen: …` prüft das.
- **Nie übersetzen:** Kennungen (Parameter-IDs, Plugin-Codes, `fk-rig`, Preset-Namen, Dateinamen, JSON-Schlüssel, Werte, die gespeichert werden), Produktnamen ("FK Rig"), Log-Zeilen, Testnamen. Gespeichert wird nie ein Anzeigetext.
- Deutsch: knapp und ohne Anrede wie bisher (Infinitiv für Befehle: "Projekt speichern", Tooltips beschreibend: "Öffnet die Kaufseite"), Fachbegriffe wie in Cubase.
- Undo-Namen sind Anzeigetexte und laufen durch `TRANS`.
- Audio-Thread: nie `TRANS`, nie `juce::translate` (sperrt kurz).
