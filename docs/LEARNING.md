# C++ für Java-Entwickler, anhand der FK Audio Suite

Dieses Dokument wächst mit dem Code. Jeder Abschnitt hat einen Teil **"Im Projekt"**, der auf konkrete Zeilen verweist (`src/dsp/fk/dsp/filters/Biquad.h:42`). Solange ein Abschnitt "(folgt mit M0)" sagt, gibt es den Code noch nicht. Die Regeln selbst stehen in `CODING_GUIDELINES.md`; hier steht, *warum* sie so sind und wie das Konzept zu dem passt, was du aus Java kennst.

Lesereihenfolge: 1 → 2 → 3 → 4 → 5 → 6 vor M0. Der Rest, wenn der jeweilige Meilenstein es braucht.

Zeilenangaben gelten für den Stand nach M5b (2026-10-09). Nach Umbauten werden sie mit dem jeweiligen Meilenstein aktualisiert.

---

## 1. Das eine mentale Modell, das alles erklärt: Werte statt Referenzen

In Java ist jedes Objekt auf dem Heap, und jede Variable ist eine Referenz darauf. `Track t = new Track();` legt ein Objekt an und `t` zeigt darauf. `Track u = t;` kopiert die Referenz, nicht das Objekt. Der Garbage Collector räumt irgendwann auf.

In C++ ist eine Variable **das Objekt selbst**. `Track t;` legt das Objekt an, auf dem Stack, direkt da, wo die Variable steht. `Track u = t;` **kopiert das ganze Objekt**. Wenn `t` den Gültigkeitsbereich verlässt (die schließende `}`), wird es sofort zerstört. Kein GC, keine Verzögerung, deterministisch.

Daraus folgt alles Weitere:
- Weil Objekte Werte sind, braucht man einen Weg, sie **ohne Kopie** weiterzugeben → **Referenzen** (`Track&`).
- Weil Objekte beim Verlassen des Blocks sterben, braucht man einen Weg, sie **länger leben** zu lassen → **Heap** über `std::unique_ptr`.
- Weil Zerstörung deterministisch ist, kann man im Destruktor aufräumen (Datei schließen, Listener abmelden) → **RAII** (Abschnitt 4).

Wer das verinnerlicht hat, versteht 80 % von C++. Der Rest ist Syntax.

### Im Projekt
- `src/app/Application.h:29`: Die Anwendung **besitzt** das Hauptfenster per `unique_ptr`. Stirbt die Anwendung, stirbt das Fenster.
- `src/app/Application.cpp:32`: `reset()` zerstört das Fenster bewusst vor dem Herunterfahren von JUCE.
- `src/ui/fk/ui/ProjectWindow.h:24`: Der Fensterinhalt ist ein **Member-Wert**, kein Heap-Objekt. Er lebt genau so lange wie das Fenster, ohne dass jemand `delete` schreibt.
- `src/ui/fk/ui/TimeMapping.h:9`: `TimeMapping` ist ein kleiner **Werttyp**: Lineal und Spuren bekommen je eine Kopie (`setTimeMapping`), und niemand muss sich fragen, wem das Objekt gehört.
- `src/ui/fk/ui/TrackLane.h:22`: `LaneSettings` bündelt Zoom, Werkzeug und Raster als **einen Wert**, den das ArrangeView an jede Spur kopiert. Ändert sich etwas, bekommt jede Spur eine neue Kopie; keine Spur hält einen Zeiger auf fremden Zustand.
- `src/services/fk/services/TrackService.h:26`: `TrackInfo` und `ClipInfo` sind **Momentaufnahmen als Werte**. Die Oberfläche zeichnet daraus, statt Zeiger ins Datenmodell zu halten.

## 2. Übersetzungstabelle Java → C++

| Java | C++ (unser Stil) | Hinweis |
|---|---|---|
| `class Foo { }` | `class Foo final { };` in `Foo.h` + `Foo.cpp` | Semikolon nach der Klasse! Deklaration und Implementierung getrennt |
| `interface Foo` | `class Foo { public: virtual ~Foo() = default; virtual void bar() = 0; };` | "rein virtuelle" Klasse |
| `class Bar implements Foo` | `class Bar final : public Foo { void bar() override; };` | |
| `Foo f = new Foo(x);` | `Foo f (x);` (Wert) oder `auto f = std::make_unique<Foo> (x);` (Heap) | `new` schreiben wir nie |
| `Foo f = null;` | `Foo* f = nullptr;` oder `std::optional<Foo>` | nur rohe Zeiger dürfen null sein |
| `final int x = 5;` | `const int x = 5;` | |
| `static final int MAX = 8;` | `constexpr int maxSlots = 8;` (im Namespace oder `static constexpr` in Klasse) | |
| `String` | `juce::String` (UI/Dateien), `std::string` (core) | |
| `List<Foo>` | `std::vector<Foo>` | speichert Werte, nicht Referenzen! |
| `List<Foo>` mit Polymorphie | `std::vector<std::unique_ptr<Foo>>` | |
| `Map<K,V>` | `std::unordered_map<K, V>` | |
| `Optional<T>` | `std::optional<T>` | |
| `throw new IOException()` | `return std::unexpected (Error { ... });` | wir werfen nicht |
| `try { } catch` | `if (auto result = f(); result) { ... } else { result.error() }` | |
| `@Override` | `override` am Ende der Signatur | Pflicht |
| `package fk.dsp;` | `namespace fk::dsp { }` | |
| `import` | `#include "fk/dsp/filters/Biquad.h"` | textuelles Einfügen, deshalb `#pragma once` |
| `Object.equals` | `operator==` | |
| `toString()` | wir schreiben eine `describe()`-Methode, kein `operator<<` | |
| Lambda `(a, b) -> a + b` | `[] (int a, int b) { return a + b; }` | die `[]` ist die Capture-Liste (Abschnitt 8) |
| `Runnable` / `Function<A,B>` | `std::function<void()>` / `std::function<B (A)>` | |
| `synchronized` | gibt es so nicht. UI-Thread: `juce::CriticalSection`; Audio-Thread: Atomics + FIFO | §8 der Richtlinien |
| `AtomicInteger` | `std::atomic<int>` | |
| Generics `<T>` | Templates `template <typename T>` | zur Compile-Zeit, keine Type Erasure |
| `assert` | `jassert (cond);` | nur im Debug-Build aktiv |
| JUnit | Catch2 | |
| Maven/Gradle | CMake | |
| JAR | ausführbare Datei, statisch gelinkt | |

## 3. Kompilieren: Header, Source, Linker, CMake

Java: eine `.java`-Datei → eine `.class`-Datei, `javac` löst Imports selbst auf.

C++: Zwei Dateiarten.
- **Header** (`.h`): Deklarationen. "Es gibt eine Klasse `Biquad` mit diesen Methoden." Wird per `#include` **textuell** in jede Datei eingefügt, die sie braucht. Deshalb `#pragma once`, sonst wird sie doppelt eingefügt.
- **Source** (`.cpp`): Definitionen. "So funktioniert `Biquad::process`."

Der **Compiler** übersetzt jede `.cpp` einzeln in eine Objektdatei (`.o`). Der **Linker** fügt alle `.o` plus Bibliotheken zu einer ausführbaren Datei zusammen. Fehler "undefined reference to …" = der Linker findet die Definition nicht (Datei nicht in CMake eingetragen, oder Methode nur deklariert).

**CMake** ist unser Build-System (wie Gradle). `CMakeLists.txt` beschreibt Targets (Bibliotheken, Programme) und ihre Abhängigkeiten. CMake erzeugt daraus Ninja-Dateien, Ninja ruft den Compiler auf. Wir benutzen **CMake Presets**, damit `cmake --preset linux-debug` immer gleich konfiguriert.

### Im Projekt
- `CMakeLists.txt`: Wurzel. Setzt C++23, Optionen, und ruft die Unterverzeichnisse in Abhängigkeitsreihenfolge auf.
- `src/core/CMakeLists.txt:2`: Ein Modul = ein Target (`fk_core`) mit Alias `fk::core`. `PUBLIC`-Include-Pfad, damit `#include "fk/core/Result.h"` überall gleich lautet.
- `cmake/FkWarnings.cmake`: Warnungen als Fehler. Beim ersten Build hat genau das einen Fehler im Test gefangen: ein `[[nodiscard]]`-Rückgabewert wurde ignoriert.
- `src/dsp/fk/dsp/generators/SineOscillator.h` + `.cpp`: Erstes Dateipaar. Header = Schnittstelle, Source = Implementierung.
- `CMakePresets.json`: `cmake --preset linux-debug` konfiguriert immer gleich.

## 4. Speicher, Besitz, RAII

### Stack und Heap
```cpp
void play()
{
    Track track;                                  // Stack. Stirbt bei }
    auto engine = std::make_unique<Engine>();     // Heap. unique_ptr besitzt es, stirbt bei }, nimmt Engine mit
}
```
Beides wird am Blockende sofort freigegeben. Der Unterschied: Stack-Objekte müssen zur Compile-Zeit bekannte Größe haben und sterben mit dem Block. Heap-Objekte können größer sein, polymorph sein (Basisklassen-Zeiger auf abgeleitete Klasse) und per `std::move` den Besitzer wechseln.

### RAII: Resource Acquisition Is Initialization
Der schlimmste Name für die beste Idee in C++. Bedeutet: Eine Ressource (Speicher, Datei, Lock, Listener-Registrierung) wird im **Konstruktor** geholt und im **Destruktor** freigegeben. Weil Destruktoren garantiert und deterministisch laufen, gibt es keine Lecks und kein vergessenes `close()`. In Java ist das `try-with-resources`, aber für alles und automatisch.

```cpp
{
    const juce::ScopedLock lock (criticalSection);  // Konstruktor sperrt
    doSomething();
}                                                   // Destruktor entsperrt, auch bei frühem return
```

### Die vier Übergabeformen (§6 der Richtlinien) in Java-Begriffen
- `std::unique_ptr<T>`: "Ich bin der Eigentümer." Es gibt genau einen. Übergabe nur per `std::move`, danach ist die Quelle leer.
- `T&`: "Ich darf es benutzen, es gehört jemand anderem, und es existiert garantiert." Entspricht einer Java-Referenz, die nie null ist.
- `T*`: wie `T&`, darf aber `nullptr` sein. Entspricht einer Java-Referenz, die null sein kann. Vor Benutzung prüfen.
- `T`: Kopie. Für kleine Dinge (Zahlen, IDs, kleine Structs).

Das Gefährliche: Eine Referenz auf ein gestorbenes Objekt (Dangling Reference) ist undefiniertes Verhalten, kein NullPointerException. Deshalb die Regel: Referenzen und rohe Zeiger werden **nicht gespeichert**, wenn man nicht sicher ist, dass der Besitzer länger lebt. Im Projekt: Komponenten speichern Referenzen auf Services, weil die `Application` die Services besitzt und länger lebt als jedes Fenster.

### Im Projekt
- `src/app/Application.h:29`: `unique_ptr` = Besitz.
- `src/app/Application.h:51`: Die **Composition Root**. Die Anwendung besitzt Engine, Services und Fenster per `unique_ptr`, in Deklarationsreihenfolge aufgebaut und rückwärts abgebaut (`src/app/Application.cpp:54`).
- `src/services/fk/services/TransportService.h:52`: Ein Service hält seine Abhängigkeit als **Referenz**: geliehen, garantiert vorhanden, weil die Anwendung sie länger am Leben hält (Konstruktor-Injektion).
- `src/services/fk/services/TransportService.h:53`: Ein **roher Zeiger**, weil er `nullptr` sein darf (kein Projekt offen) und weil der Besitzer (das Edit) wechselt.
- `src/ui/fk/ui/TrackLane.h:22`: `LaneSettings` bündelt Zoom, Werkzeug und Raster als **einen Wert**, den das ArrangeView an jede Spur kopiert. Ändert sich etwas, bekommt jede Spur eine neue Kopie; keine Spur hält einen Zeiger auf fremden Zustand.
- `src/services/fk/services/TrackService.h:15`: `TrackHandle` ist Tracktions Spur, aber die Oberfläche darf darauf nichts aufrufen, nur weiterreichen. Ein Alias macht die Grenze im Code sichtbar.
- `src/services/fk/services/ProjectService.cpp:94`: **Reihenfolge beim Zerstören**: Erst alle Zuhörer informieren, dann das Edit freigeben. Sonst zeigen Handles kurz ins Leere (Dangling).
- `src/ui/fk/ui/ProjectWindow.cpp:18`: `setContentNonOwned (&content, true)` übergibt einen rohen Zeiger, der hier "nur geliehen" bedeutet. JUCEs `setContentOwned` würde einen `new`-Zeiger verlangen, den wir nicht schreiben.
- `src/ui/fk/ui/AudioSettingsComponent.cpp:47`: Destruktor meldet den Audio-Callback ab, bevor die Member sterben. RAII von Hand, weil JUCE die Registrierung nicht selbst zurücknimmt. Deshalb braucht die Klasse einen Destruktor und ist nicht kopierbar.
- `src/dsp/fk/dsp/generators/SineOscillator.cpp:48`: `std::span<float>` leiht sich den Puffer: Zeiger plus Länge, kein Besitz, keine Kopie.

### `std::optional`: "vielleicht ein Wert", ohne Zeiger
`src/ui/fk/ui/TrackLane.h:73`: Eine laufende Verschiebung gibt es nur, während die Maus gedrückt ist. Statt eines `bool isDragging` plus loser Felder steckt alles in `std::optional<DragState>`: leer = keine Verschiebung, gefüllt = alle Daten dazu. `drag.has_value()`, `drag->clip`, `drag.reset()` (`src/ui/fk/ui/TrackLane.cpp:33`). Entspricht Javas `Optional<T>`, aber ohne Heap-Objekt.

### Stolperfalle aus dem ersten App-Build: `unique_ptr` und Vorwärtsdeklaration
`src/app/Application.h` kennt `MainWindow` nur als `class MainWindow;` (Vorwärtsdeklaration), um den Header schlank zu halten. Der Compiler meldete dann in `main.cpp`: *invalid application of 'sizeof' to incomplete type*. Grund: `unique_ptr<MainWindow>` muss beim Zerstören `delete` auf ein `MainWindow` aufrufen und braucht dafür die vollständige Klasse. Ein in der Klasse definierter (`= default` im Header) Konstruktor oder Destruktor wird in jeder Datei erzeugt, die den Header einbindet, also auch in `main.cpp`, wo `MainWindow.h` nicht bekannt ist. Lösung: Konstruktor und Destruktor im Header nur **deklarieren** und in der `.cpp` mit `= default` **definieren** (`src/app/Application.h:18`, `src/app/Application.cpp:8`). Dort ist `MainWindow.h` eingebunden. Der Konstruktor ist betroffen, weil er die Member abbauen muss, falls er abbricht. Diese Regel gilt für jede Klasse mit `unique_ptr` auf einen vorwärtsdeklarierten Typ.

## 5. `const`

`const` ist ein Versprechen an Compiler und Leser: "Das wird hier nicht geändert." Vier Stellen:

```cpp
const int bufferSize = 512;                 // Variable, nie neu zugewiesen (Java: final)
void render (const AudioBuffer& buffer);    // Parameter: ich lese nur
float getGain() const;                      // Methode: ändert kein Member (hat Java nicht!)
const Track* track = findTrack (id);        // Zeiger auf etwas, das ich nicht ändere
```

Warum so wichtig: Der Compiler verhindert versehentliche Änderungen, und beim Lesen weiß man sofort, welche Funktion Zustand ändert. `const`-Methoden darf man auf `const`-Referenzen aufrufen, Nicht-`const`-Methoden nicht. Deshalb: alle Abfragen `const`, sonst kann man sie aus einem `const`-Kontext nicht nutzen.

### Im Projekt
- `src/dsp/fk/dsp/generators/SineOscillator.h:21`: `getFrequency() const` ändert nichts, deshalb `const`. Und `[[nodiscard]]`, weil ein ignorierter Rückgabewert hier immer ein Fehler wäre.
- `src/app/HelloAudioComponent.cpp:19`: `const juce::String error = ...` wird nie neu zugewiesen.
- `tests/dsp/SineOscillatorTest.cpp:15`: Parameter `const std::vector<float>&`: lesen ohne Kopie.

## 6. Klassen

### 6.1 Deklaration und Definition getrennt
```cpp
// Gain.h
class Gain final
{
public:
    explicit Gain (float initialGainDb);
    void process (std::span<float> samples) const;
    [[nodiscard]] float getGainDb() const;
private:
    float gainDb = 0.0f;
};

// Gain.cpp
Gain::Gain (float initialGainDb) : gainDb (initialGainDb) {}
void Gain::process (std::span<float> samples) const { /* ... */ }
float Gain::getGainDb() const { return gainDb; }
```

### 6.2 Rule of Zero
C++ erzeugt automatisch Kopier-, Verschiebe-Operationen und Destruktor. Solange alle Member sich selbst aufräumen (`unique_ptr`, `vector`, `juce::String`), ist das korrekt, und man schreibt **nichts** davon. Erst wenn man einen Destruktor braucht (z. B. um sich bei einem Listener abzumelden), muss man über Kopieren nachdenken, und dann ist die Antwort fast immer: Kopieren verbieten (`JUCE_DECLARE_NON_COPYABLE`).

### 6.3 `explicit`
Ohne `explicit` darf C++ `Gain g = 3.0f;` schreiben und baut stillschweigend ein `Gain` aus einem `float`. Das führt zu unlesbaren Fehlern. Deshalb jeder Einargument-Konstruktor `explicit`.

### 6.4 Virtuelle Methoden, `override`, `final`
Java: alle Methoden sind virtuell (überschreibbar), außer `final`. C++: **keine** Methode ist virtuell, außer sie ist `virtual`. Schnittstellen haben `virtual ... = 0;` (abstrakt) und einen `virtual` Destruktor, sonst wird beim Löschen über den Basiszeiger nur die Basis zerstört. `override` lässt den Compiler prüfen, dass die Signatur wirklich etwas überschreibt. `final` an der Klasse: keine Ableitung, erlaubt Compiler-Optimierungen.

### 6.5 Initialisierungsreihenfolge
Member werden in **Deklarationsreihenfolge** initialisiert, nicht in der Reihenfolge der Initialisierungsliste. Deshalb hält clang-tidy beide gleich. Member, die andere Member brauchen, stehen weiter unten.

### Im Projekt
- `src/dsp/fk/dsp/generators/SineOscillator.h:13`: Erste vollständige Klasse. `final`, Rule of Zero (kein Destruktor, kein Copy-Code), Member mit Standardwerten in der Klasse (`:39`), `private` ganz unten.
- `src/core/fk/core/Error.h:26`: Ein `struct` für reine Daten ohne Invarianten.
- `src/core/fk/core/Error.h:11`: `enum class` statt Java-Enum. Keine Methoden, dafür `toString()` als freie Funktion daneben.
- `src/ui/fk/ui/Tool.h:7` und `src/ui/fk/ui/TrackLane.cpp:75`: Ein `switch` über ein `enum class` **ohne `default`**: Kommt ein Werkzeug dazu, warnt der Compiler an jeder Stelle, die es noch nicht behandelt (`-Wswitch`, bei uns ein Fehler). Mit `default` würde der neue Fall stillschweigend durchrutschen.
- `src/services/fk/services/TimelineService.cpp:227`: dasselbe Muster mit Rückgabewerten je Fall.
- `src/app/Application.h`: Vererbung, weil JUCE es verlangt (`juce::JUCEApplication`). Jede überschriebene Methode trägt `override`.
- `src/services/fk/services/ProjectService.h:27`: Eine **Schnittstelle** in C++: `struct` mit rein virtuellen Methoden (`= 0`) und virtuellem Destruktor. Entspricht Javas `interface`.
- `src/ui/fk/ui/TrackLane.h:22`: `LaneSettings` bündelt Zoom, Werkzeug und Raster als **einen Wert**, den das ArrangeView an jede Spur kopiert. Ändert sich etwas, bekommt jede Spur eine neue Kopie; keine Spur hält einen Zeiger auf fremden Zustand.
- `src/services/fk/services/TrackService.h:52`: Erlaubte Mehrfachvererbung: nur von Schnittstellen, und `private`, weil nur der Service selbst die Rückrufe empfangen soll. Von außen sieht niemand, dass `TrackService` ein `ValueTree::Listener` ist.
- `src/app/MainWindow.h:22` und `src/app/HelloAudioComponent.h`: Member-Reihenfolge = Initialisierungsreihenfolge. `deviceSelector` steht nach `deviceManager`, weil es ihn im Konstruktor braucht.

## 7. Templates (nur das Nötige)

Java-Generics werden zur Laufzeit zu `Object` (Type Erasure). C++-Templates werden zur **Compile-Zeit** für jeden Typ neu erzeugt (`std::vector<float>` und `std::vector<Track>` sind zwei komplett verschiedene Klassen). Vorteil: keine Laufzeitkosten, Werttypen wie `float` erlaubt. Nachteil: Fehlermeldungen sind lang, und Templates müssen vollständig im Header stehen.

Wir schreiben Templates nur für DSP-Bausteine, die mit `float` und `double` arbeiten sollen, und für kleine Hilfscontainer. Sonst benutzen wir sie nur (`std::vector<T>`, `std::span<T>`).

### Im Projekt
- `src/dsp/fk/dsp/filters/Biquad.h:34`: `Biquad<SampleType>` ist unser erstes Template. Dieselbe Filtermathematik läuft als `Biquad<float>` im Audio-Pfad (`src/plugins/fk/plugins/EqProcessor.h:36`) und als `Biquad<double>` im Test (`tests/dsp/BiquadTest.cpp:68`). Alles steht im Header, weil der Compiler den Code pro Typ erzeugt.
- Was man daran sieht: `static_cast<SampleType> (b0 / a0)` am Ende von `makeCoefficients`. Die Rechnung läuft immer in `double` (Genauigkeit), erst das Ergebnis bekommt den Zieltyp.
- `std::array<Biquad<float>, 2>`: ein Container-Template, das ein Klassen-Template enthält. Liest sich von innen nach außen.

## 8. Lambdas und `std::function`

```cpp
int offset = 10;
auto add = [offset] (int x) { return x + offset; };   // [offset]: kopiert offset in die Lambda
auto addRef = [&offset] (int x) { return x + offset; }; // [&offset]: referenziert offset (gefährlich, wenn offset stirbt)
```
Die eckigen Klammern sind die **Capture-Liste**: was die Lambda von außen mitnimmt, und ob als Kopie (`=`/`name`) oder Referenz (`&`). Regel im Projekt: `[this]` oder einzelne Namen, nie `[=]` oder `[&]` pauschal.

`std::function<void()>` ist der Typ, der jede aufrufbare Sache mit dieser Signatur speichern kann (Lambda, Funktionszeiger, Methode). Kostet eine Allokation, deshalb nicht im Audio-Thread erzeugen.

### Im Projekt
- `src/ui/fk/ui/TransportBar.cpp:21`: `playButton.onClick = [this] { transport.play(); };`. Capture `[this]`, damit die Lambda auf Member zugreifen darf. Sicher, weil der Button ein Member derselben Klasse ist und nie länger lebt als `this`.
- `src/services/fk/services/TransportService.cpp:106`: Eine Lambda **ohne Capture** als kurze Funktion, die `ListenerList::call` auf jeden Zuhörer anwendet.
- `src/ui/fk/ui/TrackHeader.cpp:32`: `if (const auto result = ...; ! result)`: Initialisierung im `if` (C++17). Das Ergebnis lebt nur im `if`-Block.
- `src/ui/fk/ui/ChannelStrip.cpp:183`: Eine Lambda, die **später** läuft (wenn der Nutzer im Menü klickt). `[this]` wäre hier gefährlich: Die Komponente könnte inzwischen zerstört sein. `SafePointer` wird dann `nullptr`, und die Lambda prüft das zuerst. Faustregel: `[this]` nur für Rückrufe, die der Besitzer selbst wieder abmeldet (Buttons, Listener); für alles Asynchrone `SafePointer` oder Werte-Captures.

## 9. Fehlerbehandlung ohne Ausnahmen: `std::expected`

```cpp
Result<TrackId> TrackService::createAudioTrack (juce::String name)
{
    if (name.isEmpty())
        return std::unexpected (Error { ErrorCode::invalidArgument, "Spurname ist leer" });
    // ...
    return trackId;
}

// Aufrufer
if (auto result = trackService.createAudioTrack (name); result)
    selectTrack (*result);                     // *result ist der Wert
else
    showError (result.error().message);        // result.error() ist der Fehler
```
Warum nicht Ausnahmen wie in Java? Drei Gründe: 1. Im Audio-Thread verboten (nicht echtzeitfähig). 2. Man sieht an der Signatur, dass etwas schiefgehen kann. 3. JUCE und Tracktion benutzen auch keine. Ein Stil.

### Im Projekt
- `src/core/fk/core/Result.h:16`: `Result<T>` ist ein Alias auf `std::expected<T, Error>`.
- `src/core/fk/core/Result.h:20`: `fail (code, message)` baut den Fehlerfall. `[[nodiscard]]`, damit niemand das Ergebnis vergisst.
- `tests/core/ResultTest.cpp:14`: `parsePositive()` zeigt das Muster einer Funktion, die fehlschlagen kann. `:37` zeigt, wie der Aufrufer den Fehler liest.
- `src/services/fk/services/ProjectService.cpp:23`: Echte Verwendung: jede Vorbedingung ein `return core::fail (...)`, am Ende der Erfolg. `Result<void>` heißt "kann fehlschlagen, liefert aber keinen Wert".
- `src/services/fk/services/TrackService.cpp:146`: Tracktion liefert einen eigenen Ergebnistyp (`tl::expected`); an der Modulgrenze übersetzen wir ihn in unser `Result`, damit die Oberfläche nur eine Form kennt.
- In `core` und `dsp` gibt es kein JUCE, deshalb dort `assert` statt `jassert` (`src/dsp/fk/dsp/generators/SineOscillator.cpp`).

## 9a. Lektion aus M2: Undo-Schritte sind nicht automatisch

Der Test "deleteClip, Undo holt ihn zurück" stürzte ab: Nach `undo()` war nicht nur der gelöschte Clip zurück, sondern die ganze **Spur weg**. Tracktions `UndoManager` fasst alle Änderungen zu einem Schritt zusammen, bis jemand `beginNewTransaction()` ruft. Spur anlegen, Datei importieren und Clip löschen waren ein einziger Schritt, und der Test hielt danach einen Zeiger auf eine zerstörte Spur (Dangling, siehe §4).

Lösung: `src/services/fk/services/ProjectService.cpp:127` und ein Aufruf am Anfang **jeder** Nutzeraktion in den Services (`src/services/fk/services/ClipService.cpp:75`). Zwei Dinge daran sind verallgemeinerbar:

1. Ein Test, der abstürzt statt fehlzuschlagen, zeigt fast immer einen Lebensdauer-Fehler. Erst den Backtrace lesen (`gdb -batch -ex run -ex bt --args ./fk_tests "Name"`), dann raten.
2. Der Service ist die richtige Stelle für den Undo-Schritt, nicht die Oberfläche: So ist jede Aktion genau ein Schritt, egal ob sie per Maus, Tastatur oder später per Controller ausgelöst wird.

## 9b. Lektion aus M3: Tests zeigen Rundung und Zaunpfähle

Zwei DSP-Tests schlugen fehl, beide lehrreich:

1. **tanh(10) ist in float exakt 1.0.** Der Test verlangte `< 1.0f`. Mathematisch nähert sich tanh nur an, aber `float` hat 24 Bit Mantisse; ab etwa tanh(9) ist der Unterschied zu 1 nicht mehr darstellbar. Tests für Gleitkomma prüfen Toleranzen, nie exakte Grenzen (`tests/dsp/WaveshaperTest.cpp:17`).
2. **Der Limiter überschritt den Ceiling um 0,003 dB.** Zwei Ursachen: Die Absenkung wurde über dB gerechnet (log, dann pow in float: Rundungsfehler), und das Vorausschau-Fenster war um ein Sample zu kurz. Lösung: linear rechnen (`src/dsp/fk/dsp/dynamics/Limiter.cpp:63`) und Fenster = Lookahead + 1 (`src/dsp/fk/dsp/dynamics/Limiter.cpp:21`). "Brickwall" heißt nie darüber, und ein Test, der das wörtlich nimmt, findet solche Fehler.

## 9c. M4: Fremden Code in einen eigenen Prozess sperren

Ein fremdes Plugin kann beim Laden abstürzen, und ein Absturz in C++ ist kein `catch`-barer Fehler: Der ganze Prozess stirbt. Deshalb wird jedes Plugin beim Scan in einem **Unterprozess** untersucht. Stirbt der, ist das ein Fehler in einer Liste, kein Datenverlust.

- `src/engine/fk/engine/FkEngineBehaviour.cpp:19`: ein einziges `return true`, und Tracktion erledigt den Rest: Es startet **dieselbe ausführbare Datei** noch einmal mit einer besonderen Kommandozeile.
- `src/app/Application.cpp:40`: Die zweite Instanz erkennt diese Kommandozeile als Erstes in `initialise()`, scannt, antwortet über eine Pipe und beendet sich. Sie öffnet nie ein Fenster. Das ist dasselbe Muster wie bei Browsern: ein Prozess pro riskantem Inhalt.
- `src/ui/fk/ui/PluginManagerWindow.h:26`: Für die Liste nehmen wir JUCEs fertige Komponente statt einer eigenen. Regel: Was das Framework fertig und gut hat, bauen wir nicht nach; wir bauen, was uns unterscheidet.
- `src/services/fk/services/PluginService.cpp:35`: `std::sort` mit einer Lambda als Vergleich. Die Lambda liefert `true`, wenn `a` vor `b` gehört. Mehrstufige Sortierung = mehrere `if`, von der wichtigsten zur unwichtigsten Regel.

## 9d. Assertions sind Tests, die das Framework für dich schreibt

Im Log der App stand "JUCE Assertion failure in juce_ValueTree.cpp:513". Im Debug-Build bricht `jassert` nicht ab (nur unter dem Debugger), aber es schreibt eine Zeile ins Log. Diese Zeilen sind Gold: Jede ist ein Vertrag des Frameworks, den unser Code verletzt. Die Suche mit `./fk_tests "[services]" 2>&1 | grep -c "Assertion failure"` fand sieben Stück, drei Ursachen:

1. **Nicht-ASCII-Literale in `juce::String`.** `juce::String ("Clip löschen")` prüft, ob der Text ASCII ist, weil der Compiler bei Umlauten im Quelltext keine Kodierung garantiert. Lösung: UTF-8 bewusst deklarieren, `juce::String::fromUTF8 (...)`. Im Projekt: `src/services/fk/services/ProjectService.cpp:131` und `src/services/fk/services/ErrorText.h:12`. Regel in CODING_GUIDELINES.md §6.
2. **Ein Host muss dem Plugin sagen, dass sein Editor gelöscht wird.** `AudioProcessorEditor` erwartet vor dem Löschen `processor.editorBeingDeleted (editor)`. Unser `unique_ptr` löschte stumpf. Lösung: ein **eigener Deleter** im `unique_ptr` (`src/services/fk/services/PluginService.h:39`). Das ist die Stelle, an der `std::unique_ptr<T, Deleter>` glänzt: Der Besitz bleibt automatisch, nur das "Wie" des Freigebens ist angepasst. Das Plugin-Fenster besitzt den Editor deshalb selbst (`src/ui/fk/ui/PluginWindow.h:27`) statt ihn JUCEs `setContentOwned` zu geben.
3. **Ein Framework-Fehler, den wir umgehen.** Tracktion legt beim Einfügen der ersten Spur den Knoten SCENES innerhalb der laufenden Undo-Transaktion an; das Undo dieser Spur verwirrt JUCEs ValueTree. Nachgewiesen mit vier Mini-Experimenten als Tests (Spur anlegen und undo: mit und ohne Speichern, mit zwei Spuren, roh über Tracktion ohne unseren Service). Erst das letzte Experiment zeigte, dass es nicht an uns liegt. Workaround: `src/services/fk/services/ProjectService.cpp:181`, Historie danach leeren. Lehre: Wenn du einen Fehler nicht erklären kannst, reduziere ihn auf das kleinste Beispiel und nimm deinen eigenen Code schrittweise heraus.

Seitdem gilt die Definition of Done in ROADMAP.md: Tests grün **und** null Assertion-Zeilen im Log.

## 9e. M5: Ein Synthesizer, asynchrone Frameworks und Tests, die darauf warten

**Der Synth** (`src/plugins/fk/plugins/SubtractiveSynthProcessor.h:18`) zeigt eine Arbeitsteilung, die sich lohnt: JUCEs `Synthesiser` verwaltet Stimmen (welche Taste spielt gerade in welcher Stimme, was passiert bei der 17. Note), unsere `SynthVoice` kennt nur den Klang einer einzigen Note. Die Parameter liegen als **ein Bündel von Atomic-Zeigern** (`src/plugins/fk/plugins/SubtractiveSynthProcessor.h:31`) vor, das jede Stimme per `const&` erhält. Statt 16 Stimmen × 16 Parameter zu kopieren, lesen alle dieselben Atomics.

Zwei Details im Audio-Thread: Der Filter-Cutoff wird nur alle 32 Samples neu gerechnet (`src/plugins/fk/plugins/SubtractiveSynthProcessor.cpp:239`), weil `tan()` teuer ist und 1,5 kHz Aktualisierungsrate niemand hört. Und `using juce::SynthesiserVoice::renderNextBlock;` (`src/plugins/fk/plugins/SubtractiveSynthProcessor.h:77`) ist dasselbe Muster wie bei `processBlock`: Wer eine Überladung überschreibt, verdeckt die anderen.

**Asynchrone Frameworks.** Drei Stunden Fehlersuche in einem Satz: Tracktion baut seine MIDI-Geräteliste nicht sofort, sondern über einen Timer (`rescanMidiDeviceList()` startet ihn nur). In der App merkt man das nie, weil die Ereignisschleife läuft. Im Test läuft sie nicht, die Liste bleibt leer, und jede darauf aufbauende Prüfung scheitert mit einer irreführenden Meldung. Lösung: `tests/services/TestEnvironment.h:38` lässt die Schleife kurz laufen; der Test ruft es nach allem, was asynchron ist (`tests/services/InstrumentTrackTest.cpp:17`). Faustregel: Wenn ein Test etwas nicht sieht, was die App sieht, fehlt fast immer die Ereignisschleife.

**Eine Hilfsfunktion, die mehr prüft als gedacht.** `te::isOnTargetTrack (instance, track, 0)` vergleicht auch einen Slot-Index, der bei MIDI-Eingängen anders belegt ist; Audio-Eingänge fand sie, MIDI-Eingänge nie. `src/services/fk/services/TrackService.cpp:339` fragt nur das, was wir wissen wollen. Lehre: Fremde Hilfsfunktionen vor Gebrauch lesen, nicht nur ihren Namen.

## 9f. M6 (vorgezogen): Eigene Daten im fremden Modell, Graphen ohne Kreise

**Eigene Eigenschaften im Tracktion-Modell.** Mono/Stereo und "ist Gruppe" kennt Tracktion nicht. Statt eines parallelen Datenmodells hängen wir zwei Properties an den Spur-Zustand (`src/services/fk/services/TrackService.cpp:9`). Tracktion ignoriert unbekannte Properties, speichert sie aber mit, und Undo funktioniert, weil wir den UndoManager übergeben. Ein `juce::Identifier` ist ein internierter String: Vergleiche sind Zeigervergleiche, deshalb einmal anlegen und wiederverwenden.

**Routing als gerichteter Graph.** Spur → Gruppe → Gruppe → Master ist ein Graph, und ein Graph mit Kreis wäre eine Rückkopplung. Tracktion stellt `feedsInto()` bereit; `src/services/fk/services/TrackService.cpp:97` lehnt ein Ziel ab, das schon in die Spur mündet, und `src/services/fk/services/TrackService.cpp:91` bietet solche Ziele gar nicht erst an. Zwei Verteidigungslinien: Die Oberfläche kann es nicht wählen, der Service prüft trotzdem (Tests oder Controller rufen ihn direkt).

**Das Framework fragt uns.** Welche Kanäle des Interfaces ein "Eingang" sind, entscheidet `src/engine/fk/engine/FkEngineBehaviour.cpp:29`: Mono-Eingänge für jeden Kanal und Stereo-Paare für jedes Paar nebeneinander, damit Mono- und Stereospuren das Passende sehen. Dasselbe `EngineBehaviour`-Muster wie bei Aufnahmedateien und Plugin-Scan: Statt Tracktion zu patchen, überschreiben wir eine Methode.

**Tests, die von der Maschine abhängen.** Der Routing-Test verlangte zunächst "mindestens ein Audio-Eingang"; auf dem Rechner lief gerade ein reines Ausgabegerät, und der Test fiel um. Ein Test darf nur prüfen, was die Software garantiert (`tests/services/RoutingTest.cpp:26`).

## 9g. M5b: Key Editor und Menüleiste

**Ein Enum erweitern, und der Compiler zeigt alle Stellen.** `Tool` bekam den Wert `draw` (`src/ui/fk/ui/Tool.h:12`). Weil unsere `switch`-Anweisungen keinen `default`-Zweig haben (§6), meldete der Compiler jede Stelle, die das neue Werkzeug noch nicht behandelte. Genau dafür ist die Regel da.

**Ein Framework-Interface implementieren.** Die Menüleiste ist JUCEs `MenuBarModel`: drei Methoden (Namen der Menüs, Inhalt eines Menüs, "Eintrag gewählt"), die `ProjectComponent` **privat** erbt (`src/ui/fk/ui/ProjectComponent.h:53`). Die Einträge sind ein `enum` mit festen Nummern (`src/ui/fk/ui/ProjectComponent.cpp:69`); dynamische Einträge (die Instrumentenliste) bekommen Nummern ab einem Basiswert. Menü und Buttons rufen dieselben Methoden, damit beide Wege gleich wirken.

**Kennungen, die nur kurz gelten.** Noten im Key Editor haben keine stabile ID in Tracktion; der Service nummeriert sie nach Position in der Liste (`src/services/fk/services/MidiClipService.h:18`). Das ist nur sicher, weil die Oberfläche nach **jeder** Änderung die Liste neu holt (`midiClipChanged`) und während eines Ziehvorgangs nur eine Vorschau zeichnet (`src/ui/fk/ui/KeyEditor.cpp:120`), das Modell aber erst beim Loslassen ändert. Ein Kompromiss mit klarer Regel schlägt eine komplizierte ID-Verwaltung.

**Vorschau statt Zwischenstände.** Beim Verschieben einer Note ändert sich das Modell nicht 60-mal pro Sekunde, sondern genau einmal beim Loslassen: ein Undo-Schritt, keine Flut von Benachrichtigungen. Dasselbe Muster wie beim Clip-Ziehen im Arrangement.

## 9h. Ein Fehlerbericht, ein Durchlauf-Test, zwei Ursachen

"Die MIDI-Aufnahme klappt nicht korrekt." Statt zu raten, wurde der Weg des Nutzers als Test nachgebaut (`tests/services/MidiRecordingTest.cpp`): Spur anlegen, Keyboard zuweisen, scharfschalten, aufnehmen, Noten senden, stoppen, Clip prüfen. Solche Durchlauf-Tests sind teurer als Einzeltests, aber sie finden, was zwischen den Schichten schiefgeht.

1. **Zeitstempel.** Tracktion ordnet jede MIDI-Nachricht über ihren Zeitstempel in die Aufnahme ein, und der muss auf derselben Uhr laufen wie der Audio-Stream (`Time::getMillisecondCounterHiRes() * 0.001`). Unsere Keyboard-Nachrichten hatten Stempel 0 (`src/services/fk/services/MidiService.cpp`, `setTimeStamp`). Lehre: Wer ein Framework-Objekt nachbaut (hier: ein MIDI-Gerät), muss auch dessen unsichtbare Verträge erfüllen.
2. **Konfiguration im falschen Moment.** Der erste Versuch, das Monitoring beim Senden einzuschalten, ließ den Test mit "0 Clips" durchfallen: Ein Moduswechsel baut den Audio-Graphen neu und bricht die laufende Aufnahme ab. Konfiguration gehört in die Zuweisung (`TrackService::setAudioInput`), nie in den Signalweg.

## 9i. Export: lange Arbeit ohne eingefrorene Oberfläche

Ein Mixdown dauert Sekunden bis Minuten. Zwei Entscheidungen:

- **Der Service blockiert bewusst** (`src/services/fk/services/ExportService.cpp:76`): Er treibt Tracktions Render-Job selbst in einer Schleife und meldet den Fortschritt über eine Callback-Funktion. Dadurch läuft derselbe Code in Tests, auf der Kommandozeile und in der Oberfläche; kein Tracktion-Fortschrittsfenster, kein Thread im Service.
- **Die Oberfläche legt den Thread darüber** (`src/ui/fk/ui/ExportDialog.cpp:12`): `juce::ThreadWithProgressWindow` ruft `run()` im Hintergrund und `threadComplete()` wieder im Message-Thread. Faustregel: Threads gehören an die Stelle, die die Bedienbarkeit verantwortet, nicht in die Fachlogik.

Eine Falle dabei: Das Format wurde zuerst über den Namen gesucht ("FLAC"), JUCE nennt es aber "FLAC file". Die Dateiendung ist die verlässlichere Quelle (`src/services/fk/services/ExportService.cpp:48`). Der Test hat es gefunden, weil er die geschriebene Datei wieder einliest und den Pegel prüft, statt nur "Datei existiert".

## 9k. U1: Oberflächen als Daten, Bausteine als Komponenten

Mit dem UI-Kit (`src/plugins/fk/plugins/ui/`) bekamen alle sechs internen Plugins eine Oberfläche, ohne dass eines davon Layout-Code enthält. Vier Dinge daraus sind lehrreich:

- **Beschreibung als Daten mit designierten Initialisierern** (`src/plugins/fk/plugins/CompressorProcessor.cpp:110`): `{ .title = "Dynamik", .controls = { ui::knob (thresholdId), ... } }` erzeugt eine `Section`-Struktur, Feld für Feld benannt (C++20). Felder, die nicht genannt werden, behalten ihren Standardwert aus der Struktur (`proOnly = false`). Java kennt das nicht; am nächsten kommt ein Builder. GCC 13 warnt bei dieser Schreibweise fälschlich über fehlende Initialisierer, deshalb ist diese eine Warnung in `cmake/FkWarnings.cmake:7` abgeschaltet, mit Begründung an Ort und Stelle.
- **Komponentenbaum, `paint` und `resized`** (`src/plugins/fk/plugins/ui/PanelView.cpp:285`): Eine JUCE-Komponente besitzt Kinder (`addAndMakeVisible`), zeichnet sich selbst in `paint (Graphics&)` und verteilt in `resized()` die Flächen an die Kinder. Das ist wie Swing, nur ohne Layout-Manager: `removeFromLeft (width)` schneidet von einem Rechteck ab und gibt das Stück zurück. Besitz der Kinder liegt bei uns (`std::unique_ptr` in `BuiltControl`, Zeile 52 im Header), JUCE hält nur rohe Zeiger darauf; deshalb werden Kinder vor dem Elternteil zerstört, weil Member in umgekehrter Deklarationsreihenfolge sterben.
- **Attachments sind das Listener-Muster in beide Richtungen** (`src/plugins/fk/plugins/ui/Knob.cpp:8`): `juce::SliderParameterAttachment` hört auf den Parameter und setzt den Regler, hört auf den Regler und setzt den Parameter, und meldet Anfang und Ende einer Geste an den Host (Automation). Der Knob hat keinen eigenen Wert. Wer Oberfläche und Modell mit einem eigenen Feld synchron hält, baut früher oder später einen Zustand, der auseinanderläuft.
- **Virtuelle Funktion mit sinnvollem Standard** (`src/plugins/fk/plugins/InternalPlugin.cpp:79`): `describePanel()` ist virtuell und liefert ohne Überschreiben alle Parameter in Reihen zu sechs. Jedes neue Plugin hat damit sofort eine Oberfläche; Überschreiben verbessert nur die Gruppierung. Der Test `PanelEditorTest` prüft für jedes Plugin, dass jeder Parameter genau einmal vorkommt: Ein Parameter, der in der Beschreibung fehlt, ist ein roter Test und keine unsichtbare Funktion (Guideline 10a).

Dazu ein Werkzeug statt Augenmaß: Der Schnappschuss-Test (`tests/plugins/PanelEditorTest.cpp:170`) zeichnet jede Oberfläche mit `createComponentSnapshot` in ein Bild und speichert PNGs, wenn `FK_UI_SNAPSHOT_DIR` gesetzt ist. So lässt sich eine Oberfläche prüfen, ohne die DAW zu starten; ohne die Variable überspringt sich der Test selbst (`SKIP`).

## 9l. U2: Ein Spektrum ohne Lock, eine Kurve aus Mathematik, Griffe ohne eigenen Wert

- **Erzeuger und Verbraucher über eine FIFO** (`src/plugins/fk/plugins/analysis/SpectrumAnalyser.cpp:34`): Der Audio-Thread schreibt Samples mit `juce::AbstractFifo::write`, der Message-Thread liest sie in `update()` (Zeile 54). Die FIFO verwaltet nur zwei Indizes (atomar); die Daten liegen in einem festen Puffer. Ist sie voll, verliert der Audio-Thread Samples und wartet nicht: Für eine Anzeige ist Verlust richtig, Warten wäre ein Aussetzer im Klang (§10). Das ist das Gegenstück zu `BlockingQueue` in Java, nur dass hier niemand blockieren darf.
- **FFT in drei Zeilen, Normierung in einer** (`SpectrumAnalyser.cpp:79`): Fenster multiplizieren, `performFrequencyOnlyForwardTransform`, in dB umrechnen. Die Normierung (2/fftSize) stammt nicht aus der Dokumentation, sondern aus dem Test: Ein Vollaussteuerungs-Sinus muss im Bin seiner Frequenz 0 dB ergeben (`tests/plugins/SpectrumAnalyserTest.cpp`). Erst stand 4/fftSize da und der Test zeigte +5,4 dB, weil JUCE das Hann-Fenster auf Summe = fftSize normiert. Tests, die eine physikalische Aussage prüfen, finden solche Faktoren; Tests, die nur "läuft durch" prüfen, nicht (§9b).
- **Frequenzgang aus Koeffizienten** (`src/dsp/fk/dsp/filters/Biquad.h:144`): |H(z)| auf dem Einheitskreis, z = e^(jω), Zähler und Nenner als Real- und Imaginärteil. Das ist derselbe Filter wie im Audio-Pfad, nur gefragt "was würdest du mit dieser Frequenz tun?" statt "verarbeite dieses Sample". Der Test vergleicht beides: gemessener Pegel nach dem Filtern eines Sinus gegen die Formel, Abweichung unter 0,2 dB.
- **ParameterAttachment und Gesten** (`src/plugins/fk/plugins/ui/BandHandles.cpp:125`): Ein Griff hält keinen Wert. `juce::ParameterAttachment` ruft bei jeder Änderung (Regler, Automation, Undo) einen Callback, der nur Zahlen für die Zeichnung merkt. Beim Ziehen gilt `beginGesture` → `setValueAsPartOfGesture` → `endGesture`: Der Host fasst die Bewegung zu einem Undo-Schritt und einer Automationsgeste zusammen. Griff, Regler und Kurve laufen alle über den Parameter; deshalb können sie nicht auseinanderlaufen.
- **Layout einmal planen, zweimal anwenden** (`src/plugins/fk/plugins/ui/PanelView.cpp:157`): `planLayout()` rechnet Rechtecke und Gesamtgröße aus, ohne etwas zu setzen (const). Der Konstruktor nutzt es für `setSize`, `resized()` für `setBounds`. Eine Funktion ohne Nebenwirkungen lässt sich zweimal rufen und einmal testen; eine, die rechnet und setzt, nicht.

## 9m. M6 Automation: Fremde Daten beobachten statt kopieren, Gesten statt Einzelwerte

- **Beobachten über den ValueTree, nicht über eigene Kopien** (`src/services/fk/services/AutomationService.cpp:333`): Tracktion speichert Kurvenpunkte als Kinder im Zustandsbaum des Plugins (`AUTOMATIONCURVE` mit `POINT`-Knoten). Der Service hält keine eigene Punktliste; er hört auf den Baum und meldet jede Änderung (auch Undo, Laden, Aufzeichnen) mit einem `automationChanged()`. Die Oberfläche liest dann neu. Das ist das Muster aus §10a, nur dass die Quelle ein fremdes Modell ist: Wer bei Undo mitkommen will, kopiert nichts.
- **Ein Index, der sich beim Ziehen ändert**: `movePoint` gibt den neuen Index zurück, weil Tracktion Punkte sortiert hält. Beim ersten Entwurf stand im Test "Index 2", die Engine lieferte 1: Punkte überholen ihre Nachbarn nicht, die Zeit wird an ihnen begrenzt. Der Test hat die Annahme korrigiert, nicht umgekehrt. Fremdcode lesen (`tracktion_AutomationCurve.cpp`, `movePoint`) war schneller als raten.
- **Gesten** (`src/services/fk/services/MixerService.cpp:64`, `src/ui/fk/ui/AutomationLane.cpp:175`): Ein Fader meldet `onDragStart`/`onDragEnd`, der Service übersetzt das in `parameterChangeGestureBegin/End`. Tracktion braucht das für Touch (schreibt nur, solange angefasst) und Latch (bis Stop). Genauso fasst die Automationsspur das Ziehen eines Punkts in einen Undo-Schritt (`beginChange` bei mouseDown, kein Schritt je Bewegung). Ein Einzelwert sagt "jetzt ist es 0,5"; eine Geste sagt "der Mensch fasst an, bewegt, lässt los". Nur das Zweite lässt sich sinnvoll aufzeichnen und rückgängig machen.
- **Anzeigezustand gehört nicht ins Projekt**: Welche Spur ihre Automation aufgeklappt zeigt, merkt sich der ArrangeView in einem `std::set<TrackHandle*>` (`src/ui/fk/ui/ArrangeView.h`), bereinigt bei jedem Umbau. Welcher Parameter gezeigt wird und welcher Modus gilt, liegt dagegen im Projekt (Tracktion speichert es), weil es beim nächsten Öffnen wieder so sein soll. Die Frage "muss das die Datei überleben?" entscheidet, wo ein Zustand wohnt.
- **Parameter, die sich ohne Projektänderung bewegen** (`src/ui/fk/ui/Inspector.cpp`, `followAutomation`): Beim Abspielen setzt Tracktion die Parameter aus der Kurve, schreibt aber nichts ins Projekt (sonst wäre jede Sekunde ein Undo-Schritt). Der `mixerChanged`-Listener schweigt also; die Fader lesen den Wert im Timer nach, solange niemand sie zieht. Zwei Wege für zwei Arten von Änderung: Ereignis für Projektänderungen, Abfrage für Laufzeitwerte.

## 9n. M5c Sampler: Eine fremde Bibliothek einbinden, ohne ihre Regeln zu brechen

- **Try-Lock statt Lock im Audio-Thread** (`src/plugins/fk/plugins/SamplerProcessor.cpp`, `processBlock`): sfizz verlangt, dass `loadSfzFile` nie gleichzeitig mit `renderBlock` läuft ("OFF"-Regel in `sfizz.hpp`). Die naive Lösung, ein Mutex um beides, ließe den Audio-Thread beim Laden warten: ein Aussetzer, bei großen Bibliotheken Sekunden. Stattdessen hält `loadSfz()` einen `juce::CriticalSection`, und `processBlock()` versucht ihn nur (`ScopedTryLock`): Bekommt er ihn nicht, bleibt der Block still. Stille beim Laden fällt nicht auf, Warten schon (§10).
- **Zustand, der kein Parameter ist**: Der Dateipfad gehört ins Projekt, aber nicht in die Automation. Er liegt als Eigenschaft im ValueTree des APVTS (`parameters.state.setProperty`), den `InternalPlugin` ohnehin speichert. `setStateInformation` wird überschrieben: erst die Basis laden lassen, dann die Datei öffnen. Ein Parameter wäre hier falsch (0..1, automatisierbar, im Host sichtbar); eine Eigenschaft ist das Richtige für "welche Datei".
- **Zwei C++-Standards in einem Build** (`CMakeLists.txt`, Abschnitt sfizz): sfizz ist C++17-Code. Mit unserem 23 scheitern seine `u8"…"`-Literale (seit C++20 ein anderer Typ, `char8_t`), mit seinem eigenen 14 seine Hex-Gleitkomma-Literale (`0x1.fffffep-1`, erst seit 17 Standard). Also bekommt sein Verzeichnis 17, unser Code 23; beide binden zusammen, weil die ABI nicht am Standard hängt. Die Lektion: Ein Submodul ist nicht "einfach da", es hat eigene Annahmen über Compiler-Schalter, und die eigene `set(CMAKE_CXX_STANDARD)` eines Unterverzeichnisses kann unsere überschreiben (CMake-Policy CMP0126).
- **Ein Test, der das erste Element nahm** (`tests/services/TestEnvironment.h`, `synthId`): Vier Tests holten sich "das erste Instrument" und bekamen nach dem Sampler plötzlich ein stummes Plugin. Listen sind sortiert, Sortierungen ändern sich. Ein Test, der ein bestimmtes Objekt braucht, sucht es nach Namen; `front()` ist eine versteckte Annahme.

## 9o. Aufnahme-Politur: Vorschau beim Ziehen, Menüs mit IDs, Einstellungen außerhalb des Projekts

- **Vorschau statt Modelländerung beim Ziehen** (`src/ui/fk/ui/TrackLane.cpp`, `mouseDrag`): Ein Fade-Griff ändert während des Ziehens nur die Anzeige (`ClipView::setPreviewFades`), das Modell erst beim Loslassen. Der Grund steht im Kommentar: Jede Modelländerung löst `tracksChanged()` aus, die Spur baut ihre Clip-Ansichten neu, und der Zeiger auf den gezogenen Clip zeigte ins Leere. Wer das Modell live ändern will, muss Ansichten identitätsstabil halten (über IDs nachschlagen), sonst ist die Vorschau der sichere Weg.
- **Asynchrone Menüs arbeiten mit IDs, nie mit Zeigern** (`TrackLane::showClipMenu`): `PopupMenu::showMenuAsync` ruft den Callback später; bis dahin kann die Clip-Ansicht neu gebaut sein. Das Lambda fängt deshalb nur die Clip-ID (eine `juce::String`) und sucht die Ansicht beim Klick neu. Dasselbe gilt für `CallOutBox` mit dem Gain-Regler: `juce::CallOutBox::launchAsynchronously` übernimmt den Besitz des Reglers, das Lambda darauf fängt die ID.
- **Was ins Projekt gehört und was nicht**: Fades und Clip-Gain liegen am Clip (Projekt, Undo). Der Vorzähler liegt bei Tracktion in den Engine-Einstellungen: eine Gewohnheit des Nutzers, für alle Projekte gleich, ohne Undo. Die Spurhöhe liegt nur in der Ansicht. Drei Zustände, drei Orte; die Frage ist immer "wer muss das überleben: der Clip, der Nutzer oder nur dieses Fenster?".
- **Begrenzen im Service, nicht in der Oberfläche** (`src/services/fk/services/ClipService.cpp`, `setFades`): Fade-in auf die Cliplänge, Fade-out auf den Rest, Gain auf ±24 dB. Die Oberfläche begrenzt zusätzlich für die Vorschau, aber die Regel steht genau einmal dort, wo sie jeder Aufrufer bekommt, auch ein Test oder später ein Controller.

## 9p. M6b: Musiktheorie als Daten, Sequenzen mit Kurven, ein geteilter Arbeitsbaum

- **Musiktheorie ohne Projekt** (`src/services/fk/services/SignatureService.cpp`, `getChordNotes`): Skalentöne, Stufen und Akkorde sind statische Funktionen: Grundton, Skala, Stufe rein, MIDI-Noten raus. Sie brauchen kein Edit, deshalb testet `SignatureServiceTest` sie ohne Projekt (`C-Dur, Stufe V, Septakkord → 67, 71, 74, 77`). Alles, was reine Rechnung ist, gehört in solche Funktionen: billig zu testen, überall nutzbar (Key Editor heute, Akkordspur morgen). Tracktion liefert dafür `Scale::getSteps` und `Chord::getSteps (inversion)`; wir übersetzen nur in unsere Begriffe (Dur/Moll, Stufen in römischen Ziffern mit Kleinschreibung für Moll).
- **Kurve als eine Zahl**: Tracktions Tempopunkt hat `curve` (−1..1); 0 ist eine gerade Linie zum nächsten Punkt, ±1 ein Sprung, der hält. Unser `TempoPointInfo` macht daraus ein `bool ramp`, weil die Oberfläche genau diese zwei Fälle kennt (Cubase: "Sprung" und "Rampe"). Fremde Freiheitsgrade, die wir nicht anbieten, nicht durchreichen; sonst muss jede Oberfläche sie verstehen.
- **Taktlänge an der Position** (`TimelineService::barLengthSeconds`): Der Test für Taktartwechsel fiel durch, weil die Taktlänge die Taktart des Projektanfangs nahm. Eine Funktion, die "an dieser Position" heißt, muss alles an dieser Position nachschlagen; der Test hat den Fehler gefunden, bevor ein Nutzer es tat.
- **Zwei Agenten, ein Arbeitsbaum**: Während dieser Arbeit baute ein zweiter Agent im selben Verzeichnis das UI-Kit um. Sein halbfertiger Stand ließ die App nicht linken, obwohl mein Code fehlerfrei war. Was half: Nur die eigenen Bibliotheken bauen (`--target fk_services fk_ui`), abstimmen, wer welche Dateien anfasst, und erst nach seinem Commit die Tests laufen lassen. Git hilft erst beim Commit; davor hilft nur Reden.

## 9r. Takes und Comping: Die Engine braucht ein Projekt, und ein Test findet es heraus

- **Ein Feature, das nur im Kontext funktioniert** (`src/services/fk/services/ProjectService.cpp`, `attachProject`): Tracktion verwaltet Takes (ein Take je Loop-Durchlauf) und Comps nur für Dateien, die ein Tracktion-Projekt kennt. Unsere Projekte waren bisher reine Edit-Dateien; `WaveAudioClip::setCurrentTake` löschte den Take dann stillschweigend, weil es kein Projekt-Element dazu fand. Die Lösung ist kein Umweg um die Engine, sondern ihr Modell: Jeder FK-Projektordner ist jetzt ein **ordnerbasiertes Tracktion-Projekt**, jede Datei darin ein Element. Importe werden nach `Audio/` kopiert (wie der Pool in Cubase) und beim Projekt angemeldet, weil der Ordner-Scan neue Dateien erst nach einem Reload sieht.
- **Der Test, der die Lücke zeigte**: Erst "Takes = 0 nach dem Hinzufügen", dann "alle Stücke haben 1 Take" nach dem Schneiden. Ohne Test wäre der Fehler erst beim Nutzer mit seiner Loop-Aufnahme aufgefallen, als verschwundener Take. Jeder Zwischenstand kam aus einer gezielten Ausgabe (`std::cerr` im Service, wieder entfernt): Projekt vorhanden? Element gefunden? Erst dann die Ursache (gecachter Scan), nicht vorher raten.
- **Comping ohne Render** (`ClipService::useTakeForRange`): Tracktions Comp-Rendering verlangt Projekt-IDs, die ordnerbasierte Projekte nicht haben. Statt dessen schneidet der Service den Clip an den Bereichsgrenzen und schaltet das Mittelstück auf den Take; jedes Stück behält alle Takes. Dieselbe Wirkung, undo-fähig, keine Render-Dateien, und die Oberfläche kann jedes Stück später wieder umschalten. Eine Vereinfachung, die den Nutzen behält, schlägt die vollständige Nachbildung, die nicht läuft.
- **Semantik aus dem Fremdcode, nicht aus dem Namen**: `unpackTakes (false)` heißt bei Tracktion nicht "auf dieselbe Spur", sondern "auf die folgenden Spuren, neue nur bei Bedarf". Der Test erwartete zwei Clips auf einer Spur und bekam eine neue Spur. Wieder: Implementierung lesen (`tracktion_WaveAudioClip.cpp`, `unpackTakes`), Erwartung anpassen, Name im eigenen Service klarer wählen ("Takes auf Spuren darunter verteilen").

## 9s. Drag-and-drop aus dem Dateimanager: ein Interface, zwei Koordinatensysteme

- **`juce::FileDragAndDropTarget`** (`src/ui/fk/ui/ArrangeView.h`): Eine Komponente wird Ziel, indem sie das Interface erbt und vier Fragen beantwortet: Interessiert mich das (`isInterestedInFileDrag`, nur Audio-Endungen), wo schwebt es (`fileDragMove`, Vorschau), ist es weg (`fileDragExit`), ist es gefallen (`filesDropped`). Die Logik liegt in `importFiles (files, x, y)`, das auch der Menüpunkt und der Test aufrufen; das Interface ist nur der Adapter. Was ein Nutzer per Maus tut, soll immer auch ein Test ohne Maus tun können.
- **Koordinaten gehören jemandem**: Der Abwurf kommt in ArrangeView-Koordinaten, die Zeilen liegen im gerollten `rowsContainer`. `getLocalPoint (this, point)` übersetzt in die Zeile, `getLocalArea (&rowsContainer, bounds)` zurück für die Vorschau. Wer das vergisst, trifft nach dem ersten Rollen die falsche Spur. Der Test fand gleich den zweiten Fehler dieser Art: Er rechnete Pixel pro Sekunde selbst (40) statt die Abbildung zu fragen (`getTimeMapping()`), und die Standardauflösung war 20.
- **Ein Test, der nach einem abgebrochenen Build durchfiel** (`tests/services/MidiRecordingTest.cpp`): Nach dem neuen Compiler-Schalter `JUCE_USE_MP3AUDIOFORMAT` (eine PUBLIC-Definition an `fk_juce`, also für alle Module) brach der Build zweimal wegen Speichermangel ab (Exit 137). Die danach "fertige" Testdatei bestand aus alten und neuen Objektdateien; die MIDI-Aufnahme landete bei 0 s, fünf Läufe in Folge. Ein vollständiger Neubau mit `-j 3` statt `-j 5`, und der Test war grün, mit demselben Code. Die Lektion: Wer eine Definition ändert, die alle Übersetzungseinheiten sehen, braucht einen Build, der durchläuft; ein abgebrochener Build ist kein halber, sondern ein falscher. Vor dem Suchen im Code erst die Frage "ist das Binary konsistent?".
- **Mehrere Dateien, eine Regel**: Die erste Datei auf die Zielspur, jede weitere auf die Spur darunter, fehlt sie, entsteht sie. Eine Regel, die der Nutzer vorhersagen kann, schlägt einen Dialog mit drei Optionen.

## 9t. Medien-Browser: zwei Audio-Callbacks, zwei Arten von Drag-and-drop

- **Ein zweiter Audio-Callback neben der Engine** (`src/services/fk/services/PreviewService.cpp`): Zum Vorhören braucht es keine Spur und kein Projekt. `juce::AudioDeviceManager` erlaubt mehrere Callbacks und mischt sie; der `AudioSourcePlayer` mit `AudioTransportSource` hängt sich neben Tracktion ans Gerät. Die Datei liest ein `TimeSliceThread` im Hintergrund in einen Puffer (32768 Samples), der Audio-Thread nimmt nur aus dem Puffer (§10). Reihenfolge im Destruktor: erst vom Gerät abmelden, dann die Quelle lösen, dann den Thread beenden; andersherum liest der Audio-Thread aus einem toten Objekt.
- **Zwei Drag-and-drop-Welten in JUCE** (`src/ui/fk/ui/ArrangeView.h`): Dateien aus dem Dateimanager kommen über `FileDragAndDropTarget` (Betriebssystem), Ziehen innerhalb der App über `DragAndDropTarget` mit einem `DragAndDropContainer` als Vorfahr (hier `ProjectComponent`). Der Medien-Browser legt die Pfade in die Beschreibung des Vorgangs (`fk-media:` plus Zeilen), das Arrangement liest sie zurück; beide Wege münden in dasselbe `importFiles (files, x, y)`. Ein Adapter je Quelle, eine Logik.
- **`mutable` für eine const-Lücke in der Bibliothek**: `juce::AudioFormatManager::createReaderFor` ist nicht `const`, obwohl es nichts verändert. Statt `const` an unserer Methode zu opfern, ist das Member `mutable`, mit Kommentar. Das ist der richtige Einsatz von `mutable`: ein Detail, das von außen nicht sichtbar ist (§5).

## 9v. Clip-Bearbeitung: Auswahl als Kennungen, Kopien als Zustand, Kleben als Render

- **Auswahl als Menge von Kennungen** (`src/ui/fk/ui/ArrangeView.h`, `selectedClips`): Die Oberfläche merkt sich ausgewählte Clips als `std::set<juce::String>` ihrer Kennungen, nie als Zeiger auf `ClipView`. Die Zeilen werden nach jeder Modelländerung neu gebaut; Zeiger wären dann tot, Kennungen bleiben gültig. Beim Umbau filtert `rebuildRows` die Menge auf Clips, die es noch gibt, und reicht sie an jede Spur (`TrackLane::setSelectedClips`). Dasselbe Muster wie Clip-IDs im Service (ADR 0004), nur eine Ebene höher.
- **Eine Aktion, vier Wege** (`src/ui/fk/ui/TrackLane.h`, `ClipAction`): Werkzeugleiste, Menü Bearbeiten, Kontextmenü und Tastenkürzel rufen alle `ArrangeView::performClipAction`. Die Tasten lösen den Button aus (`triggerClick`), damit niemals ein Weg etwas anderes tut als der andere. Ein `enum class` statt sechs Methoden hält die Rückrufe schmal: ein `std::function<void (ClipAction)>` von der Spur zum Arrangement.
- **Kopieren über den Zustand** (`src/services/fk/services/ClipService.cpp`, `makeEntries`/`insertEntries`): Ein Tracktion-Clip ist ein `juce::ValueTree`; `clip->state.createCopy()` ist die ganze Kopie samt Fades, Gain, Takes und Noten. Beim Einfügen vergibt `te::EditItemID::remapIDs` neue Kennungen für alles im Baum (auch Plugins im Clip), sonst gäbe es zwei Clips mit derselben ID. Die Zwischenablage speichert zu jedem Eintrag den Abstand zum ersten Clip in Sekunden und Spuren, so behält ein Block aus mehreren Spuren seine Form. Erst alle Ziele prüfen, dann einfügen: Entweder landet alles oder nichts.
- **Trimmen ist `setStart` mit `preserveSync`** (`trimClip`): Tracktion kennt kein "trim"; `setStart (t, preserveSync = true, keepLength = false)` verschiebt den linken Rand und rechnet den Quellversatz mit, das Material bleibt an seiner Stelle im Projekt. Mit `preserveSync = false` würde es mitwandern. Der linke Rand darf nie vor den Anfang des Materials (Versatz 0): `earliestStart = start - offset`.
- **Kleben zweier Audio-Clips ist ein Render** (`mergeClips`): MIDI-Noten lassen sich zusammenführen (`te::mergeMidiClips`), Audio-Material nicht. Also rendert der Service genau diese Clips (`Renderer::Parameters::allowedClips`, eine Spur im Bitset, ohne Plugins) in eine neue Datei im Projektordner und ersetzt die Originale durch einen Clip auf diese Datei. Fades und Gain stecken dann in der Datei, Takes sind weg; die Doku sagt das. Der Render läuft synchron im Aufruf, wie der Export (`ExportService`), damit es ohne Fenster und in Tests geht.
- **Reihenfolge ist nicht Zeit**: `track->getClips()` und damit `TrackService::getClips` liefern Clips in der Reihenfolge ihrer Anlage, nicht nach Position. Zwei Tests nahmen `clips[1]` für "der zweite auf der Zeitachse" und fielen durch. Wer eine Ordnung braucht, sortiert selbst oder sucht mit `std::any_of`.
- **Ein Test, der die Uhr braucht** (`tests/services/MidiRecordingTest.cpp`): Der Test spielt Noten mit echten Wartezeiten (150 ms, 100 ms) und prüft den Abstand im Clip. Läuft daneben ein Compiler mit voller Last, hinkt der Audio-Thread des Dummy-Geräts hinterher und der Abstand schrumpft auf 70 ms: der Test fällt durch, obwohl der Code stimmt (und §9s hatte dafür noch den Build verdächtigt). Wall-Clock-Tests gehören auf eine ruhige Maschine, oder sie messen gegen die Uhr des Audio-Geräts statt gegen `pumpMessages`.

## 9w. Controller: Was die Engine schon kann, und wo sie eine Antwort von uns braucht

- **Tracktion hat MIDI Learn eingebaut** (`src/services/fk/services/MidiLearnService.cpp`): `Engine::getMidiLearnState()` ist ein Schalter, `Edit::getParameterChangeHandler()` merkt sich den zuletzt angefassten Parameter, solange der Schalter an ist, und `ParameterControlMappings` verknüpft ihn mit dem nächsten Controller, der sich bewegt (`sendChange`). Unser Service schaltet, hört zu und schreibt die Zuordnungen mit `saveToEdit` ins Projekt. Vor dem Selberbauen lohnt immer die Frage: Welche Hälfte davon kann die Bibliothek schon?
- **Eine Frage der Engine, die wir beantworten müssen** (`src/engine/fk/engine/FkUIBehaviour.h`): Controller-Bewegungen gehen an das "zuletzt fokussierte Edit", und `te::UIBehaviour::getLastFocusedEdit()` liefert in der Grundeinstellung `nullptr`. Also kam keine einzige Bewegung an. Die Engine fragt die Oberfläche über dieses Strategie-Objekt; unsere Antwort ist das eine offene Edit, eingetragen vom ProjectService. Wer eine Engine-Funktion nutzt, die "nicht reagiert", sucht zuerst nach solchen Rückfragen.
- **Hot-Plug ist ein Timer, kein Ereignis**: Tracktions `DeviceManager` prüft die MIDI-Hardware alle paar Sekunden (`restartMidiCheckTimer`) und schickt nach jedem Abgleich eine `ChangeMessage`. `MidiService` und `TrackService` hören darauf; die Spurköpfe bauen ihre Eingangslisten über `tracksChanged` neu. Ein Rescan-Button ruft nur `rescanMidiDeviceList()`, den Rest macht derselbe Weg.
- **Virtuelle Geräte haben feste Kennungen**: Das Sammelgerät "All MIDI Ins" heißt intern `all_midi_in` und bündelt alle physischen Eingänge. Wir zeigen es als "Alle MIDI-Eingänge" und weisen es neuen Instrumentenspuren zu; die Übersetzung passiert beim Lesen (`MidiService::getAllInputsName`), nicht im Gerät (kein Alias im Projekt, keine Speicherfrage).
- **Pitch-Bend und Vibrato im Synth** (`src/plugins/fk/plugins/SubtractiveSynthProcessor.cpp`): `juce::Synthesiser` ruft `pitchWheelMoved` und `controllerMoved` je Stimme; der Faktor `2^(Halbtöne/12)` wird blockweise (alle 32 Samples) auf beide Oszillatoren gelegt, zusammen mit einem Sinus-Vibrato, dessen Tiefe das Mod-Wheel setzt. Der Test misst Nulldurchgänge statt FFT: einfach, robust, und +12 % bei +2 Halbtönen ist eindeutig.
- **Abbau-Reihenfolge, zum zweiten** (`src/app/Application.cpp`): PreviewService und MidiService melden sich im Destruktor bei der Geräteverwaltung ab. Drei Services fehlten in `shutdown()`, wurden also erst nach `engineContext.reset()` zerstört und griffen ins Leere: Assertion in `tracktion_Engine.cpp` und Segfault beim Beenden. Regel: Wer in `initialise` angelegt wird, steht in `shutdown` in umgekehrter Reihenfolge, ohne Ausnahme, und ein neuer Service kommt in beide Listen.
- **Ein Test braucht die Laufzeit, auch ohne Fenster**: `juce::AudioProcessor` legt einen Timer an; ohne `ScopedJuceInitialiser_GUI` im Test meldet JUCE "startTimer ohne MessageManager" und "Timer überlebt das Ereignissystem" (acht Assertions, alle Tests grün). Die Null-Assertion-Regel fängt genau so etwas.

## 9z. A1a: Keine Daten verlieren

- **Schreiben, dann umbenennen** (`src/services/fk/services/ProjectService.cpp:46`, `writeStateAtomically`): Wer eine Datei direkt überschreibt und dabei abstürzt, hinterlässt eine halbe Datei, und das Projekt ist weg. `juce::TemporaryFile` schreibt in eine Nachbardatei und ersetzt das Ziel erst am Ende mit einem einzigen Umbenennen; das ist auf demselben Dateisystem atomar. In Java: `Files.move (tmp, ziel, ATOMIC_MOVE)`.
- **Ein Absturz ist eine Datei, die übrig bleibt** (`ProjectService::closeProject`, `findNewerAutosave`): Die automatische Sicherung liegt in `Backups/<Name>.autosave.tracktionedit`. Reguläres Schließen löscht sie, denn der Nutzer hat über seine Änderungen entschieden. Ist sie beim Öffnen noch da und neuer als die Projektdatei, war das ein Absturz. Kein Flag, kein Zustand im Speicher; der Dateisystemzustand ist die Wahrheit.
- **Verweise beim Kopieren mitnehmen** (`relinkFileReferences`, `ProjectService.cpp:59`): Clips verweisen über die Eigenschaft `source` auf ihre Datei, mal relativ (`Audio/riff.wav`), mal als Tracktion-Projekt-ID. „Speichern unter“ läuft rekursiv über den ganzen ValueTree, löst jeden Verweis zu einer echten Datei auf und schreibt ihn neu: relativ, wenn die Datei im neuen Ordner liegt, sonst absolut. So hängt die Kopie nicht mehr am alten Ordner.
- **Erst fragen, dann zerstören, und die Reihenfolge stimmt** (`src/ui/fk/ui/ProjectComponent.cpp:529`, `closeProjectThen`): Beim Öffnen zuerst die Datei wählen, dann nach ungespeicherten Änderungen fragen, dann schließen. Wer den Dateidialog abbricht, behält sein Projekt. Die Folgeaktion wird als `std::function` übergeben, weil der Dialog asynchron ist: Der Code läuft weiter, die Antwort kommt später in einem Lambda. `SafePointer` schützt davor, dass das Fenster inzwischen weg ist (in Java würde man einen `CompletableFuture` verketten).
- **Fremde Timer in Tests** (`tests/services/ProjectServiceTest.cpp`): Tracktion setzt „geändert“ asynchron und 200 ms nach dem Laden noch einmal zurück. Ein Test, der sofort nach dem Anlegen etwas ändert, sieht deshalb „unverändert“. Ursache gefunden durch Lesen des Tracktion-Quelltexts (`EditChangeResetterTimer`), nicht durch längeres Warten auf gut Glück.

## 9z2. A1b: Wünsche speichern, Wirklichkeit prüfen

- **Das Projekt will, die Engine hat** (`src/services/fk/services/ProjectSettingsService.h:34`): Tracktion kennt keine Projekt-Samplerate. Die Rate gehört dem Audiogerät, Format und Bittiefe gehören jedem Eingang. Das Projekt speichert deshalb nur seine Wünsche in einem eigenen Knoten (`FKPROJECTSETTINGS`), und der Service setzt sie beim Öffnen um (`applyRecordFormat`, Zeile 205). Die Geräte-Rate stellt er nie still um, weil das andere Programme am selben Interface treffen kann; er fragt.
- **Ein eigener Knoten im fremden Baum**: Tracktion speichert unbekannte Kinder und Properties mit und führt sie durch Undo. So brauchen eigene Daten weder eine zweite Datei noch eine Änderung an Tracktion. Dasselbe Muster wie `trackProperties` (§9f), nur auf Projektebene.
- **Vorlagen sind Projekte minus Dateien** (`ProjectService::writeTemplate`, `ProjectService.cpp:165`): Kopie des Zustands, Audio-Clips rekursiv entfernt (`removeAudioClips`), Projekt-ID gelöscht. Was der Kopie fehlt, prüft der Code vorher und sagt es (gebackene Spuren), statt eine kaputte Vorlage zu schreiben.
- **Ein Signal-Handler ist kein normaler Code** (`src/app/Application.cpp:53`, `writeCrashReport`): Nach einem Absturz läuft der Handler in einem Prozess, dessen Speicher schon kaputt sein kann. Darin sollte man eigentlich nur sehr wenige Funktionen aufrufen. Wir schreiben trotzdem einen Stack-Trace in eine Datei (wie JUCEs eigene Beispiele), weil der Nutzen hoch ist und es danach ohnehin vorbei ist. In Java fängt die JVM so etwas ab; in C++ ist es der letzte Atemzug.
- **Ein Logger, alle schreiben hinein**: `juce::Logger::setCurrentLogger` setzt eine globale Senke; Tracktion, JUCE und unser Code schreiben über `juce::Logger::writeToLog`. Der Logger wird als Erstes gebaut und als Letztes abgebaut (Member ganz oben in `Application.h`), damit auch Auf- und Abbau protokolliert werden.

## 9z3. A2: Bäume statt Listen, und Bilder als Test

- **Aus der Liste wird ein Baum** (`TrackService::getTrackList`, `src/services/fk/services/TrackService.cpp`): Mit Ordnern ist die Spurliste ein Baum. Eine Tiefensuche mit einer rekursiven Lambda (`std::function<void (te::Track*, int)> visit`) macht daraus wieder eine flache Liste mit Einrücktiefe, genau das, was eine Ansicht von oben nach unten zeichnet. Eine Lambda kann sich nur über ein `std::function` selbst aufrufen, weil ihr Typ beim Schreiben noch keinen Namen hat.
- **Zwei Listen und eine Reihenfolge** (`ArrangeView::rowOrder`): Statt jede Stelle umzubauen, die `rows[i]` mit der i-ten Spur verknüpft, bekommen Ordner ihre eigene Liste, und eine dritte Liste (`RowSlot`) legt beide ineinander. Layout und Ziehen laufen über die Reihenfolge, alles andere bleibt, wie es war. Kleinster Eingriff, der korrekt ist.
- **Ziele statt Indizes** (`ReorderTarget`): „Vor Spur X“ oder „in Ordner Y“ ist robuster als „an Position 3“: Ausgeblendete Spuren und zugeklappte Ordner verschieben Indizes, Objekte nicht.
- **Ein Bild findet, was kein Assert findet**: Der Layout-Test (`tests/ui/ProjectComponentLayoutTest.cpp`) prüft Grenzen; erst das gerenderte Bild zeigte, dass ein Button seit Tagen fehlte. Und dass `juce::FileOutputStream` an vorhandene Dateien **anhängt** (Java: `new FileOutputStream (f, true)`): Das PNG-Programm las das alte Bild vorn in der Datei. Erst löschen, dann schreiben.
- **Prüfe, was du gerade ersetzt hast**: clang-format formatiert Listen um; eine Textersetzung, die das alte Format sucht, findet nichts und meldet keinen Fehler. Deshalb nach jeder automatischen Änderung grep auf das Ergebnis.

## 9z4. A3: Reine Funktionen zuerst, Gruppen beim Umbruch

- **Formatieren ohne Zustand** (`src/services/fk/services/TimeFormat.cpp`): `formatTime` bekommt alles als Parameter (Sekunden, Format, Samplerate, Bildrate) und liefert nur einen Text. So ist sie in fünf Zeilen testbar, ohne Engine, ohne Fenster. Timecode rundet ab, nicht zum nächsten Frame: Frame 11 gilt, bis Frame 12 beginnt.
- **Was die Engine kann, was wir ergänzen** (`TransportService::record`, `timerCallback`): Tracktion kennt Punch (an den Loop-Bereich gebunden), aber kein Pre- und Post-Roll. Pre-Roll heißt: vor dem Start die Position zurücksetzen. Post-Roll heißt: ein Timer schaut 30-mal pro Sekunde auf die Position und stoppt. Beides lebt in einem eigenen Knoten `FKTRANSPORT` im Projekt.
- **Eine Rechnung für Höhe und Lage** (`TransportBar::layoutItems`): Dieselbe Funktion beantwortet „wie hoch wirst du bei dieser Breite?“ (ohne etwas zu verschieben) und setzt die Rechtecke. Zwei getrennte Rechnungen würden irgendwann voneinander abweichen.
- **Struct als Default-Argument ist ein Fallstrick**: Ein Dialog, der seine Einstellungen aus den eigenen Feldern neu zusammenbaut, setzt alle anderen Felder auf ihre Vorgabe zurück. Immer vom gespeicherten Stand ausgehen und nur ändern, was der Dialog zeigt (`ProjectSettingsDialog::applySettings`).

## 9z5. A4: Werttypen statt Enums, Gesten über Komponentengrenzen

- **Ein Enum, das Daten braucht, wird ein Struct** (`GridResolution` in `src/services/fk/services/TimelineService.h`): Solange es nur Takt, Viertel, Achtel gab, reichte ein `enum class`. Ein Raster „1/8 triolisch“ trägt aber einen Notenwert mit. Jetzt ist es ein kleiner Werttyp mit `Kind` und optionalem `NoteValue`, statische Fabriken (`GridResolution::bar()`) ersetzen die alten Enum-Konstanten, und `operator==` vergleicht nur, was zählt. In Java wäre das eine `record`-Klasse mit statischen Fabrikmethoden.
- **Nicht destruktiv, wo es geht** (`ClipService::normalizeClips`): Normalisieren misst die Spitze und setzt das Clip-Gain, die Datei bleibt unberührt und Undo ist trivial. Nur „Phase umkehren“ schreibt eine neue Datei, und auch dann zeigt Undo einfach wieder auf die alte (`SourceFileReference::setToFile`).
- **Erst messen, dann ändern**: Normalisieren und Phase umkehren lesen und schreiben erst alle Dateien und hängen dann in einem Undo-Schritt um. Bricht etwas mittendrin ab, ist nichts halb geändert.
- **Eine Geste über viele Komponenten** (`TrackLane::onMarqueeStart/Drag/End`, `ArrangeView::marqueeMoved`): Ein Lasso beginnt in einer Spur, endet aber in einer anderen. JUCE schickt alle Zieh-Ereignisse an die Komponente, in der die Maus gedrückt wurde. Die Spur gibt nur die Bildschirmposition weiter; das Arrangement, das alle Spuren kennt, rechnet in seine Koordinaten um, zeichnet und wertet aus.
- **Tests gegen die richtige Uhr** (`tests/services/MidiRecordingTest.cpp`): Der Test verglich Notenabstände mit der Wanduhr und schlug unter Last fehl. Gemeint ist aber „die Note liegt, wo die Engine beim Spielen war“. Also misst er jetzt die Engine-Position. Ein wackelnder Test ist meist ein Test, der das Falsche misst.

## 9z6. A5: Selbst messen, was die Bibliothek versteckt

- **Wenn die Bibliothek es nicht herausgibt, selbst bauen** (`src/dsp/fk/dsp/analysis/TempoEstimator.cpp`): Tracktions Tempo-Erkennung steckt in einem internen Header. Statt ihn hineinzuschmuggeln, schätzt eine kleine reine Funktion das Tempo: Energie je Block, nur der Anstieg (Einsatzstärke), leicht geglättet, dann Autokorrelation über die möglichen Schlagdauern. Die Glättung war nötig: 150 BPM entsprechen 37,5 Blöcken, der Gipfel verteilt sich auf zwei Werte, und sonst gewann das halbe Tempo (75 Blöcke, glatt). Eine sanfte Vorliebe für Tempi um 120 BPM verhindert Oktavfehler.
- **Eine Schwelle relativ zur Sache** (`minimumPeriodicity`): Ob Material Rhythmus hat, entscheidet nicht ein absoluter Wert, sondern wie viel der Gesamtenergie (Korrelation bei Verzögerung 0) sich nach einer Schlagdauer wiederholt.
- **Zeit ist mehrdeutig** (`ClipService::processAudio`): Clip-Zeit, Projektzeit, Material-Zeit. Ohne Time-Stretch ist Material = Versatz + Clip-Zeit · Geschwindigkeit. Folgt der Clip dem Tempo, hängt die Umrechnung von jeder Tempoänderung ab; dort lehnt der Sample-Editor ab, statt falsch zu rechnen.
- **Eine Funktion, viele Knöpfe** (`AudioProcess`): Stille, Verstärken, Normalisieren, Fades, Umkehren, Phase unterscheiden sich nur in einer Zeile (`applyProcess`). Lesen, Datei schreiben und Quelle umhängen gibt es einmal. In Java wäre das ein Strategie-Enum mit einer Methode je Konstante; hier ein `switch`, der vom Compiler auf Vollständigkeit geprüft wird (`-Wswitch`).
- **Ein Test, der nichts messen kann, darf nicht lügen**: Steht das Audiogerät der Testumgebung, bleibt die Engine-Position bei 0, und Zeitstempel lassen sich nicht prüfen. Der MIDI-Test erkennt das und warnt (`WARN`), statt grün durchzuwinken oder rot Alarm zu schlagen. Gefunden durch Ausschalten der verdächtigen Änderung (Signalsmith) und erneutes Messen: Der Fehler blieb, also lag es nicht am Code.

## 9z7. A6: Signalwege und kleine unsichtbare Bausteine

- **Feedback Delay Network** (`src/dsp/fk/dsp/reverb/FdnReverb.cpp`): Acht Verzögerungsleitungen, deren Ausgänge über eine Householder-Matrix (I − 2/N · 1) gemischt zurückfließen. Die Matrix ist verlustfrei (orthogonal), deshalb steuert allein die Dämpfung je Leitung die Nachhallzeit: g = 10^(−3 · Länge / (RT60 · fs)). Ohne Orthogonalität klingt es metallisch oder schwingt sich auf.
- **Glätten, aber nicht beim Start** (`StereoDelay`): Eine Zeitänderung gleitet, sonst knackt es. Nach `prepare` und `reset` springt der Wert aber sofort ans Ziel, sonst fehlt das erste Echo. Ein Flag (`snapToTarget`) trennt die beiden Fälle.
- **Normierte Parameter** (`MixerService::setTrimDb`): Tracktion führt Plugin-Parameter als 0..1. Den Wertebereich kennt nur der JUCE-Parameter (`convertTo0to1`/`convertFrom0to1`). Über `setValueNotifyingHost` gesetzt, landet der Wert auf einem Weg in Projekt, Automation und Export.
- **Ein unsichtbares Plugin statt Sonderweg** (`ChannelTrim.h`): Trim und Phase sind ein internes Plugin vor den Inserts. Damit gelten Undo, Speichern, Export und Bake ohne eine Zeile Extra-Code; nur die Listen blenden es aus.
- **Stabile ID, wechselnder Name** (`TrackService::setPortAlias`): Projekte speichern die Geräte-ID, die Oberfläche zeigt den eigenen Namen. Das ist dasselbe Prinzip wie ein Primärschlüssel neben einem Anzeigefeld in einer Java-Entity: Umbenennen bricht keine Verweise.
- **Präfix in der ID statt zweitem Typ** (`"device:<ID>"` in `OutputInfo`): Gruppen und Hardware-Ausgänge stehen in einer Liste. Ein Präfix unterscheidet sie, ohne die Oberfläche anzufassen, die nur IDs zurückreicht.

## 9z8. A7: Bibliotheken nutzen, ohne sie zu linken

- **Erst schauen, was die Bibliothek kann** (`ExportService.cpp`): Tracktion hat mit `RenderSpecification` schon Formate, Stems, Quellspuren und LUFS. Der Service übersetzt nur noch unsere Optionen hinein. Weniger eigener Code heißt weniger eigene Fehler.
- **Zur Laufzeit laden statt linken** (`TRACKTION_ENABLE_LIBLAME`): libmp3lame steht unter LGPL. Tracktion öffnet sie per `dlopen` (in Java vergleichbar mit `Class.forName` auf ein optionales JAR). Fehlt sie, ist MP3 einfach nicht da; die App startet trotzdem, und unsere GPL-Lizenz bleibt sauber getrennt.
- **Ein Thread-Wechsel auf Bestellung** (`juce::MessageManager::callSync`): Der Export rechnet im Hintergrund, Spuren stummschalten ändert aber das Edit und gehört auf den Message-Thread. `callSync` schickt die Funktion hin und wartet; läuft man schon dort (Tests), ruft es direkt. In Java entspricht das `SwingUtilities.invokeAndWait`.
- **Zwei Ergebnisarten, ein Feld** (`std::variant` in `ExportTask`): Mixdown liefert eine Datei, Stems eine Liste. `std::variant` hält genau eines davon, `std::visit` ruft die passende Überladung von `report`. Das ersetzt eine Klassenhierarchie mit einer virtuellen Methode.
- **Einheiten genau lesen** (`quarterNotesAt`): Tracktion zählt Schläge im Nenner der Taktart (6/8: Achtel), MIDI-Dateien in Vierteln. Ein Test mit Tickpositionen findet so einen Faktor-2-Fehler sofort; Hören würde ihn erst im 6/8-Takt zeigen.

## 9z9. A8: Befehle als Daten

- **Eine Tabelle statt einer if-Kette** (`src/ui/fk/ui/Commands.cpp`): Jeder Befehl ist ein Eintrag aus Kennung, Name, Kategorie und Standardtasten. Daraus entstehen Tastenverarbeitung, Menütexte, Tooltips und der Editor. Vorher stand jede Taste an drei Stellen (Abfrage, Menütext, Tooltip) und lief auseinander: F5 stand im Menü, war aber nirgends abgefragt. In Java wäre das ein `enum` mit Feldern; in C++ ein `enum class` plus eine konstante Tabelle, weil Enums hier keine Felder tragen.
- **`switch` über ein `enum class` ohne `default`** (`ProjectComponent::performCommand`): Mit `-Wswitch -Werror` meldet der Compiler jeden neuen Befehl, der noch keinen Fall hat. Ein `default` würde genau diese Prüfung abschalten.
- **Nur Abweichungen speichern** (`KeyBindings::store`, `ShortcutService`): Gespeichert wird nur, was der Nutzer geändert hat. Ändert sich später ein Standard, bekommen alle anderen Befehle ihn automatisch.
- **Stabile Schlüssel** (`CommandInfo::key`): Der Anzeigename darf sich ändern, der Schlüssel in der Einstellungsdatei nie. Wie eine Spaltenbezeichnung in einer Datenbank-Tabelle.
- **Lebensdauer in asynchronen Rückrufen** (`juce::Component::SafePointer` in `showZoomMenu`, `InsertSlot::askForPresetName`): Ein Menü oder Dialog antwortet später. Ist das Fenster bis dahin zu, wäre `this` ein hängender Zeiger. `SafePointer` wird dann `nullptr`; der Rückruf prüft das zuerst. Javas GC verhindert dieses Problem, C++ nicht.
- **Ein Baustein, drei Menüs** (`buildPluginMenu`): Inspector, MixConsole und Instrumentwahl bauten ihr Plugin-Menü je selbst, mit kleinen Unterschieden. Jetzt eine Funktion, die nur Daten (`PluginInfo`) bekommt und ein Menü liefert; testbar ohne Fenster.

## 9z10. B1: Signalwege aus Bausteinen der Engine

- **Ein Rack, viele Türen** (`RackService`): Tracktion rechnet ein Rack einmal und mischt, was alle Rack-Instanzen hineinschicken; jede Instanz holt sich gezielt Ausgänge zurück. MIDI-Spuren schicken nur hinein, Ausgangskanäle holen nur heraus. So entsteht Cubases Rack-Instrument aus vorhandenen Teilen, ohne eigenen Audio-Code.
- **Pin 0 ist MIDI**: In Tracktion-Racks ist Anschluss 0 der MIDI-Pin, Audio zählt ab 1. Der erste Versuch hat beim Aufräumen der Vorgabe-Anschlüsse auch den MIDI-Pin entfernt; das Rack blieb still, ohne Fehlermeldung. Gefunden mit einem Test, der die Verbindungen des Racks ausgibt: drei erwartet, null vorhanden. Lehre: bei stiller Ausgabe zuerst die Verdrahtung zählen, nicht am Klang raten.
- **Ein Baustein für zwei Fälle** (`MidiChannelRouting.h`): Rack-MIDI-Spuren und MIDI-Spuren an Geräten brauchen beide „alles auf Kanal N“. Tracktions MIDI Patch Bay vor dem Ziel erledigt das für Clips und Live-Spiel gleichermaßen. Ein freier Funktionsblock (`namespace midiChannelRouting`) statt einer Klasse: Es gibt keinen Zustand zu kapseln.
- **Was eine Bibliothek nicht kann, sauber benennen**: sfizz wertet MIDI-Kanäle nicht aus (`lochan` wird ignoriert). Der Test prüft den Kanal deshalb an der Zuordnung selbst, nicht am Klang, und der Kommentar sagt warum.
- **Neue Enum-Werte zeigen alle Stellen** (`TrackKind::rackOutput`, `TrackKind::midi`): Mit `-Wswitch -Werror` meldet der Compiler jeden `switch`, der den neuen Wert nicht behandelt. Für Vergleiche wie `kind == instrument` hilft er nicht; dafür gibt es jetzt `playsMidi (kind)`, eine Stelle statt sieben.

## 9z11. B3: MIDI als Datenstrom und als Rechnung

- **Reine Funktionen für Bearbeitungen** (`MidiClipService::transform`, `quantisedStart`): Die Rechnung kennt kein Projekt, nur Noten rein, Noten raus. Testbar ohne Engine, und der Service darum herum kümmert sich nur um Undo und Speicherung. In Java würde man dasselbe mit einer statischen Methode auf Werte-Objekten machen.
- **Designated Initializers** (`{ .function = MidiFunction::transpose, .semitones = 12 }`): C++20 erlaubt benannte Felder bei Aggregaten. Lesbarer als lange Konstruktoren; Felder ohne Angabe behalten ihren Standardwert. Reihenfolge muss der Deklaration folgen.
- **Note-Offs merken, nicht neu rechnen** (`Transposer`, `ChordGenerator`): Wird ein Regler bewegt, während eine Taste gehalten ist, würde ein neu berechnetes Note-Off die falsche Note treffen und eine hängen lassen. Darum merkt sich jeder Effekt beim Note-On, was er ausgelöst hat, und löst genau das beim Note-Off wieder.
- **Zeit im Takt statt in Samples** (`Arpeggiator`): Der Schritt wird aus der Songposition (ppq) berechnet, nicht durch Zählen von Samples. So bleibt das Arpeggio beim Springen, Loopen und Tempowechsel im Raster; ohne laufende Wiedergabe tickt eine eigene Uhr im gleichen Maß.
- **Die Engine fragen, was sie schon kann**: Retrospektive Aufnahme und das Mithören gespielter Tasten (`MidiKeyChangeDispatcher`) bringt Tracktion mit. Unser Code sind ein paar Zeilen drumherum: Prüfen, ob es etwas zu holen gibt, und eine verständliche Meldung statt einer englischen Fehlermeldung.

## 9z12. B4: In die Engine einhaken statt nachbauen

- **Virtuelle Haken der Bibliothek nutzen** (`FkEngineBehaviour::createPlaybackMidiSequence`): Tracktion fragt die EngineBehaviour, wie aus einem MIDI-Clip die Wiedergabefolge wird. Wir überschreiben genau diese eine Methode. Alles andere (Loop, Quantisierung, Groove, Export, Bake) läuft dadurch automatisch mit Spielweisen, weil es dieselbe Methode aufruft.
- **Zerlegen und den Standardweg wiederverwenden** (`createSequenceWithArticulations`): Statt die Zeitrechnung von Tracktion nachzubauen, teilen wir die Noten nach Spielweise auf, lassen Tracktion jede Gruppe auf ihrem Kanal erzeugen und mischen die Ergebnisse. Weniger eigener Code, gleiche Genauigkeit.
- **Zustand an der Note statt auf der Zeitachse**: Ein Keyswitch gilt eigentlich bis zum nächsten. Wer mitten im Song startet, müsste ihn nachsuchen. Weil wir ihn vor jede Note setzen, gibt es diesen Fall nicht. Das kostet ein paar zusätzliche MIDI-Ereignisse und spart eine ganze Klasse von Fehlern.
- **Defaulted `operator<=>`** (`ArticulationOutput`): `auto operator<=> (const T&) const = default;` erzeugt alle Vergleiche feldweise. So taugt ein Struct als Schlüssel in `std::map`. Ein selbst geschriebener `operator<` in einem anonymen Namespace wird von `std::less` nicht gefunden (Name Lookup über ADL sucht im Namespace des Typs).
- **Eine Kopie im Projekt** (Map als JSON an der Spur): Wie Cubase speichert das Projekt seine Expression Maps selbst. Ändert sich die Map in der Bibliothek, klingt ein altes Projekt trotzdem wie beim Speichern.

## 9z13. B5: Netz, Dateien und Hintergrund-Threads

- **Streamen statt auf einmal laden** (`ContentService::install`): `juce::URL::createInputStream` liefert den Download blockweise. So gibt es Fortschritt und Abbruch, und ein 2-GB-Archiv liegt nie ganz im Speicher. Vorher prüfen, ob die Platte das Archiv plus den entpackten Inhalt fasst.
- **Aufräumen auf jedem Fehlerweg** (`cleanUp` als Lambda): Abbruch, Schreibfehler und kaputtes Archiv löschen alle die Teildateien. Ein Lambda, das alle Wege rufen, statt Code an fünf Stellen. In Java wäre das ein `finally`, nur hier gezielt für den Fehlerfall.
- **Ein Thread, ein Timer** (`ContentManagerDialog::Worker`): Der Download läuft in einem eigenen `juce::Thread`; Fortschritt und Ende teilt er über `std::atomic`. Der Message-Thread fragt das zehnmal pro Sekunde per Timer ab und fasst die Oberfläche nur dort an. Abbrechen ist `signalThreadShouldExit()`, das der Download bei jedem Block prüft.
- **Fertig erst mit Quittung** (`fk-package.json`): Ein Paket gilt erst als installiert, wenn nach dem Entpacken die Manifest-Datei geschrieben ist. Bricht etwas mittendrin ab, sieht das Programm beim nächsten Start kein halbes Paket als fertig an.
- **Tests ohne Netz** (`file://`-URL): Die Tests bauen sich ein kleines ZIP und installieren es über eine lokale Adresse. Derselbe Code wie beim echten Download, aber schnell, ohne Internet und ohne Gigabytes auf der Platte.
- **Kein SQLite, solange es nicht nötig ist** (`LibraryService`): Der Index entsteht bei jeder Suche neu aus Plugins, Bibliotheken und Presets. Für ein paar tausend Einträge ist das schnell genug und hat keinen veralteten Zwischenstand. Eine Datenbank kommt erst, wenn Messungen sie verlangen.

## 9z14. Phase C: Bauen für zwei Systeme und ausliefern

- **Eine Quelle für die Version** (`configure_file` mit `BuildInfo.h.in`): CMake ersetzt `@FK_VERSION@` und Co. beim Konfigurieren und legt die fertige Header-Datei in den Build-Ordner. Der Code liest `fk::core::buildInfo::version` wie eine Konstante. Ähnlich wie in Java ein von Gradle erzeugtes `build-info.properties`, nur zur Übersetzungszeit statt zur Laufzeit.
- **`inline constexpr`** für Konstanten in Headern: `constexpr` macht den Wert zur Übersetzungszeit bekannt, `inline` erlaubt, dass mehrere .cpp-Dateien den Header einbinden, ohne dass der Linker doppelte Definitionen meldet.
- **Generator-Ausdrücke** (`$<$<CXX_COMPILER_ID:MSVC>:/utf-8>`): Die Option gilt nur, wenn der Compiler MSVC ist. So bleibt eine CMake-Datei für beide Systeme ohne `if`-Blöcke.
- **MSVC-Eigenheiten**: `/utf-8` (Quelltext und Strings als UTF-8, sonst werden Umlaute zerlegt), `/Zc:__cplusplus` (sonst meldet MSVC immer C++98), `/bigobj` (Tracktion erzeugt mehr Abschnitte, als eine Objektdatei normal fasst), `/permissive-` (strenger Standard wie bei GCC/Clang).
- **CMake-Presets** (`CMakePresets.json`): Generator, Build-Ordner und Optionen je Plattform an einer Stelle. `"condition"` blendet die Windows-Presets auf Linux aus. CI, Skripte und Entwickler benutzen dieselben Namen.
- **Plattformcode klein halten**: Nur `HangWatchdog` braucht Linux-Header, eingeschlossen in `#if JUCE_LINUX`. Alles andere läuft über JUCE und den C++-Standard. Je weniger `#if`, desto weniger Code, der nur auf einem System getestet ist.
- **Netz nie im Message-Thread** (`checkForUpdates`): `juce::Thread::launch` startet die Prüfung im Hintergrund, `MessageManager::callAsync` bringt das Ergebnis zurück. Die Lambda bekommt eine Kopie des Dienstes, damit nichts auf ein Objekt zeigt, das beim Schließen schon weg ist.
- **AppImage**: Ein Dateiformat, das ein ganzes Programmverzeichnis (AppDir) samt Bibliotheken in eine ausführbare Datei packt. `linuxdeploy` sucht die benötigten `.so`-Dateien und kopiert sie hinein; Grundbibliotheken wie glibc und ALSA kommen vom Nutzer-System.
- **Range-for über ein Temporary** (`for (auto& n : getClip (id)->notes)`): `getClip` liefert ein `Result` als Wert. Die Schleife verlängert nur die Lebensdauer des letzten Ausdrucks (`notes`), nicht die des `Result` davor, das schon vor dem ersten Durchlauf zerstört wird. C++23 behebt das (P2718), GCC 13 noch nicht. Der Debug-Build merkt nichts, erst der Release-Build mit Optimierung warnt. Abhilfe: Ergebnis erst in eine Variable legen. Deshalb baut die CI auch Release.
- **Manifest statt fester Links**: Website und App lesen `latest-<Plattform>.json`. Ein neues Release ist nur ein neuer Upload, keine Code-Änderung. rclone macht den Upload unabhängig vom Anbieter des Buckets.
- **Mitgelieferte Plugins** (`src/services/fk/services/BundledPlugins.cpp:27`): Ein AppImage hängt sich bei jedem Start unter einem neuen `/tmp/.mount_…`-Pfad ein. Ein Plugin direkt daraus hätte jedes Mal einen anderen Pfad, und die Plugin-Liste würde veralten. Deshalb wird das Bundle einmal je Version an einen festen Ort kopiert; eine kleine Kennzeichen-Datei unterscheidet „von uns installiert“ (darf ersetzt werden) von „selbst installiert“ (nie anfassen). `juce::KnownPluginList::scanAndAddFile` (`PluginService.cpp`, `addPluginFiles`) scannt dann nur diese Datei, über denselben Unterprozess wie der große Scan.
- **Eine feste Adresse vor wechselndem Speicher**: Die Update-Prüfung fragt fk-licensing (`UpdateService::parseManifest` versteht beide Formate), nicht den Bucket. Die Adresse im Build bleibt gültig, auch wenn der Speicher umzieht, und es gilt nur, was ein Mensch veröffentlicht hat. Das ist dasselbe Prinzip wie ein Interface vor einer Implementierung in Java.
- **PowerShell 5.1 und Umlaute**: Ohne BOM liest Windows PowerShell 5.1 Skripte in der ANSI-Codepage; aus „ü“ wird „Ã¼“. Mit UTF-8-BOM lesen PowerShell 5 und 7 richtig. Batch-Dateien brauchen CRLF.

## 10. Echtzeit-Audio: warum der Audio-Thread anders ist

Das Audio-Interface fordert alle ~2,7 ms (128 Samples bei 48 kHz) einen fertigen Puffer. Kommt er zu spät, hört man einen Knackser. Alles, was unbestimmt lange dauern kann, ist deshalb verboten: Speicher anfordern (kann das Betriebssystem blockieren), Locks (ein anderer Thread hält sie vielleicht gerade), Dateien, Logging. Java kann das nicht garantieren, weil der GC jederzeit anhalten darf. C++ kann es, wenn man sich an §8 der Richtlinien hält.

Die Struktur jedes Audio-Bausteins:
```cpp
void prepareToPlay (double sampleRate, int maxBlockSize);  // UI-Thread: hier allokieren
void processBlock (juce::AudioBuffer<float>& buffer);      // Audio-Thread: nur rechnen
void releaseResources();                                   // UI-Thread: freigeben
```
Parameter kommen über `std::atomic<float>` hinein, Meter-Werte gehen über `std::atomic<float>` oder einen Lock-free-FIFO hinaus.

### Im Projekt
- `src/app/SineToneSource.h:37`: Zwei `std::atomic`-Member tragen Ein/Aus und Frequenz vom Message-Thread in den Audio-Thread. Kein Lock, keine Allokation.
- `src/app/SineToneSource.cpp:29`: Der Audio-Thread liest die Atomics, füllt den Puffer, fertig. Kein `juce::String`, kein Logging, nichts aus der Verbotsliste.
- `src/app/SineToneSource.cpp:41`: Der JUCE-Puffer wird als `std::span` an den reinen DSP-Baustein gereicht. So bleibt `dsp` frei von JUCE.
- `src/dsp/fk/dsp/generators/SineOscillator.cpp:41`: Phase in Umdrehungen statt Radiant, der Überlauf ist ein `-= 1.0`. `:43` wandelt `double` nach `float` mit `static_cast`, nie mit C-Cast.
- `src/plugins/fk/plugins/InternalPlugin.cpp:113`: Jeder Plugin-Parameter ist im Audio-Thread ein `std::atomic<float>&`. Die Oberfläche schreibt, `processBlock()` liest mit `.load()`, kein Lock dazwischen (`src/plugins/fk/plugins/CompressorProcessor.cpp:89`).
- `src/plugins/fk/plugins/EqProcessor.cpp:149`: Teure Mathematik (sin, cos, pow) nur, wenn sich ein Parameter geändert hat; danach pro Sample nur Multiplikationen.
- `src/dsp/fk/dsp/delay/DelayLine.h:16`: Speicher wird in `prepare()` angefordert, `pushAndRead()` berührt nur noch den fertigen Puffer. Dasselbe Muster in `Limiter::prepare` und `DistortionProcessor::prepareToPlay` (Oversampling, Dry-Puffer).
- `src/plugins/fk/plugins/LimiterProcessor.cpp:48`: Ein Plugin mit Vorausschau verzögert das Signal und **meldet** das dem Host. Der verzögert alle anderen Spuren gleich mit (Plugin Delay Compensation), sonst wäre die Spur nicht mehr synchron.

### Wenn der Kernel die App einfach beendet
Unter PipeWire/JACK bekommt der Audio-Thread Echtzeit-Priorität (über rtkit). Damit ein hängender Echtzeit-Thread nicht den ganzen Rechner blockiert, setzt Linux eine Grenze (`RLIMIT_RTTIME`, hier 200 ms): Rechnet der Thread so lange am Stück, ohne zu schlafen, schickt der Kernel **SIGKILL**. Das lässt sich nicht abfangen, es gibt keinen Absturzbericht und im Debugger nur „Program terminated with signal SIGKILL“. Unter ALSA ohne Echtzeit-Recht wäre derselbe Fehler nur ein Knackser gewesen.

Um die Stelle zu finden, setzt man das weiche Limit unter das harte (PipeWire: `rt.time.soft`/`rt.time.hard` in `jack.conf`). Dann kommt erst **SIGXCPU**, und gdb hält genau im schuldigen Thread an. Im Projekt war das Tracktions Puffer für die retrospektive Aufnahme: 30 s je Eingang, angelegt und mit Nullen gefüllt im Audio-Thread, sobald sich die Samplerate ändert. Lösung siehe `EngineContext::sizeRetrospectiveBuffers` (`src/engine/fk/engine/EngineContext.cpp`).

## 10a. Das Listener-Muster: Wie Oberfläche und Services miteinander reden

Java-Entwickler kennen es als Observer / `addListener`. In C++ mit JUCE sieht es so aus:

1. Der Service definiert eine kleine Schnittstelle (`struct Listener`) und hält eine `juce::ListenerList<Listener>` (`src/services/fk/services/TransportService.h:54`).
2. Eine Komponente erbt **privat** von dieser Schnittstelle, meldet sich im Konstruktor an und im Destruktor ab (`src/ui/fk/ui/TransportBar.cpp:37`, `src/ui/fk/ui/TransportBar.cpp:44`). Das Abmelden ist Pflicht, sonst ruft der Service später ein zerstörtes Objekt auf.
3. Der Service ruft `listeners.call (...)` mit einer Lambda, die jeden Zuhörer benachrichtigt.

Warum nicht einfach `std::function`-Callbacks? Weil mehrere Komponenten zuhören (Transportleiste und Arrangement reagieren beide auf Play/Stop) und weil eine Schnittstelle mehrere Ereignisse bündeln kann.

Zwei Verfeinerungen im Projekt:
- **Bündeln mit `AsyncUpdater`** (`src/services/fk/services/TrackService.h:54`): Tracktion meldet bei einer Spuränderung Dutzende Einzel-Ereignisse. `triggerAsyncUpdate()` sammelt sie und ruft `handleAsyncUpdate()` genau einmal, sobald die Ereignisschleife frei ist.
- **Pollen per Timer** (`src/ui/fk/ui/TransportBar.cpp:39`): Die Wiedergabeposition ändert sich dauernd. Ein Ereignis pro Änderung wäre Verschwendung; die Leiste fragt 30-mal pro Sekunde nach.

## 11. Werkzeuge, die dir Fehler zeigen, bevor der Nutzer sie sieht

- **Compiler-Warnungen** (`-Wall -Wextra -Werror`): Der Compiler kennt die meisten Anfängerfehler. Wir behandeln jede Warnung als Fehler. Nur für **unseren** Code: JUCE hängt seine Modul-Quellen in unser App-Target, darunter eine C-Datei, die mit C++-Flags nicht baut. Deshalb setzt `cmake/FkWarnings.cmake` die Flags für die App pro Quelldatei (`fk_set_warnings_for_sources`), für `core`/`dsp`/Tests pro Target.
- **clang-tidy**: findet Stilverstöße und Fehlerquellen (fehlendes `override`, Member in falscher Reihenfolge, unnötige Kopien).
- **AddressSanitizer** (`-fsanitize=address`): findet Zugriffe auf freigegebenen Speicher und Pufferüberläufe zur Laufzeit. In Java unmöglich, in C++ der häufigste Fehler. Preset `linux-asan`.
- **UndefinedBehaviorSanitizer**: findet Überläufe, falsche Casts, Null-Dereferenzierung.
- **ThreadSanitizer**: findet Datenrennen zwischen UI- und Audio-Thread.
- **gdb**: Debugger. In VS Code oder CLion grafisch.

## 12. Glossar

| Begriff | Bedeutung |
|---|---|
| **UB** (Undefined Behavior) | Der Standard sagt nicht, was passiert. Kann funktionieren, abstürzen oder Daten zerstören. Z. B. Zugriff hinter Array-Ende. |
| **Dangling** | Zeiger/Referenz auf ein bereits zerstörtes Objekt. UB beim Zugriff. |
| **Translation Unit** | Eine `.cpp` plus alles, was sie inkludiert. Einheit des Compilers. |
| **ODR** (One Definition Rule) | Jede Funktion/Klasse darf im ganzen Programm nur eine Definition haben. Verletzung durch fehlendes `#pragma once` oder Definition im Header ohne `inline`. |
| **Move** | Besitz übertragen statt kopieren. `std::move (x)` macht aus `x` eine "verschiebbare" Quelle; danach `x` nicht mehr benutzen. |
| **RAII** | Siehe Abschnitt 4. |
| **PDC** (Plugin Delay Compensation) | Engine verzögert alle Spuren so, dass Plugins mit interner Latenz synchron bleiben. |
| **ValueTree** | JUCE-Datenstruktur: Baum aus Knoten mit Properties, mit Undo und Listenern. Tracktions Projektmodell. |
| **Message-Thread** | JUCEs UI-Thread. Alles GUI und alle Services laufen hier. |
| **Lock-free FIFO** | Warteschlange, die zwei Threads ohne Lock benutzen können (ein Schreiber, ein Leser). `juce::AbstractFifo`. |
