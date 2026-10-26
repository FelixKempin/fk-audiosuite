#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_engine/tracktion_engine.h>

#include <memory>

namespace fk::engine
{

namespace te = tracktion;

/// Besitzt die Tracktion Engine und damit Audio-/MIDI-Geräte, Plugin-Verwaltung und
/// temporäre Dateien. Es gibt genau eine Instanz, die der Anwendung gehört und die
/// alle Services per Referenz erhalten (Konstruktor-Injektion, ARCHITECTURE.md §3).
///
/// Warum keine globale Instanz: Tests können so eine eigene Engine aufsetzen,
/// und die Lebensdauer ist am Besitzer ablesbar (→ LEARNING.md §4).
class EngineContext final
{
public:
    /// applicationName benennt Einstellungs- und Temp-Ordner der Engine.
    explicit EngineContext (const juce::String& applicationName);

    /// Prüft, ob dieser Prozess als Plugin-Scan-Helfer gestartet wurde (Kommandozeile von Tracktion).
    /// Liefert true, wenn ja: Dann übernimmt Tracktion den Prozess, und die App darf kein Fenster öffnen.
    [[nodiscard]] static bool handlePluginScanWorkerStartup (const juce::String& commandLine);
    ~EngineContext();

    [[nodiscard]] te::Engine& getEngine() noexcept;

    /// Meldet der Engine das Edit im Vordergrund (nullptr = keines). MIDI-Controller-Bewegungen
    /// erreichen nur dieses Edit (Zuordnungen, MIDI Learn). Der ProjectService ruft das.
    void setFocusedEdit (te::Edit* edit) noexcept;

    /// Lange Aufgaben der Engine (Bake-Rendern) mit Fortschrittsfenster; nur die App schaltet das ein.
    void setProgressWindowsEnabled (bool shouldShow) noexcept;
    [[nodiscard]] te::DeviceManager& getDeviceManager() noexcept;

    /// Die JUCE-Geräteverwaltung hinter Tracktion, z. B. für den Einstellungsdialog.
    [[nodiscard]] juce::AudioDeviceManager& getAudioDeviceManager() noexcept;

    /// Schaltet alle Audio-Eingänge des Interfaces frei (Mono- und Stereo-Eingänge, siehe
    /// FkEngineBehaviour), damit jede Spur einen beliebigen wählen kann (Mehrkanal-Aufnahme, FEATURES.md §3).
    void enableAllAudioInputs();

    /// Retrospektive Aufnahme: So viele Sekunden hält jeder Eingang vor, der einer Spur zugewiesen ist.
    static constexpr double retrospectiveSeconds = 30.0;

    /// ... und so viele jeder andere Audio-Eingang. Tracktion legt den Puffer im Audio-Thread neu an,
    /// sobald sich Samplerate oder Kanalzahl ändern. 30 s für jeden der bis zu 50 Eingänge eines
    /// großen Interfaces (über 500 MB) dauern dort länger, als Linux einem Echtzeit-Thread erlaubt:
    /// Der Kernel beendet die App dann ohne Absturzbericht (SIGKILL über RLIMIT_RTTIME).
    static constexpr double retrospectiveStandbySeconds = 1.0;

    /// Gibt den zugewiesenen Eingängen des Edits den vollen Puffer und allen anderen Audio-Eingängen
    /// den kleinen. Tracktion nutzt beim retrospektiven Aufnehmen ohnehin nur zugewiesene Eingänge.
    /// Nach jeder Änderung an Geräten oder Zuweisungen aufrufen (Message-Thread).
    static void sizeRetrospectiveBuffers (te::Edit& edit);

    /// Macht unsere eigenen Plugins (fk::plugins) der Engine bekannt: Sie erscheinen in der
    /// Plugin-Liste und werden beim Laden eines Projekts wie fremde Plugins instanziiert.
    void registerInternalPlugins();

private:
    te::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineContext)
};

} // namespace fk::engine
