#pragma once

#include "fk/core/Result.h"
#include "fk/engine/EngineContext.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TimeFormat.h"

#include <vector>

namespace fk::services
{

/// Dateiformat neuer Aufnahmen.
enum class RecordFormat
{
    wav,
    flac,
    aiff
};

/// Was eine Audioaufnahme mit vorhandenen Clips an derselben Stelle macht.
enum class AudioRecordMode
{
    keepTakes, ///< neuer Clip darüber, die alten bleiben als Takes erreichbar (Cubase: Verlauf behalten)
    replace    ///< vorhandene Clips im aufgenommenen Bereich werden ersetzt
};

/// Was eine MIDI-Aufnahme mit vorhandenen MIDI-Clips macht.
enum class MidiRecordMode
{
    newClip, ///< eigener neuer Clip
    merge,   ///< Noten in den vorhandenen Clip mischen (Overdub)
    replace  ///< vorhandene Noten im Bereich ersetzen
};

/// Einstellungen, die zum Projekt gehören und mit ihm gespeichert werden (wie Cubase' Projekteinstellungen).
struct ProjectSettings
{
    double sampleRate = 48000.0;
    RecordFormat recordFormat = RecordFormat::wav;
    int bitDepth = 24; ///< 32 = 32 Bit Float (nur WAV)
    AudioRecordMode audioRecordMode = AudioRecordMode::keepTakes;
    MidiRecordMode midiRecordMode = MidiRecordMode::merge;
    int timecodeFps = 25;                           ///< Bilder pro Sekunde für die Timecode-Anzeige (24, 25, 30)
    TimeDisplay timeDisplay = TimeDisplay::seconds; ///< zweite Positionsanzeige in der Transportleiste
    bool autoCrossfades = true;                     ///< überlappende Audio-Clips blenden automatisch über
};

/// Liest und schreibt die Projekteinstellungen und setzt sie in der Engine um.
///
/// Tracktion kennt keine Projekt-Samplerate und kein Projekt-Aufnahmeformat: Die Rate gehört dem
/// Audiogerät, Format und Bittiefe gehören jedem Eingangsgerät. Das Projekt speichert deshalb seine
/// Wünsche (Knoten FKPROJECTSETTINGS im Edit), und dieser Service überträgt Format und Bittiefe beim
/// Öffnen auf alle Eingänge. Die Geräte-Rate stellt er nur auf ausdrücklichen Wunsch um.
class ProjectSettingsService final : private ProjectService::Listener, private juce::ValueTree::Listener
{
public:
    ProjectSettingsService (engine::EngineContext& engineContext, ProjectService& projectService);
    ~ProjectSettingsService() override;

    /// Einstellungen des offenen Projekts; ohne Projekt die Vorgaben mit der Rate des Audiogeräts.
    [[nodiscard]] ProjectSettings getSettings() const;

    /// Speichert die Einstellungen als Undo-Schritt und überträgt Format und Bittiefe auf die Eingänge.
    /// Die Geräte-Rate bleibt unverändert (setDeviceSampleRate).
    [[nodiscard]] core::Result<void> setSettings (const ProjectSettings& settings);

    /// Mögliche Bittiefen eines Formats, aufsteigend.
    [[nodiscard]] static std::vector<int> getBitDepths (RecordFormat format);
    [[nodiscard]] static juce::String getFormatName (RecordFormat format);

    /// Samplerate des laufenden Audiogeräts, 0 ohne Gerät.
    [[nodiscard]] double getDeviceSampleRate() const;
    [[nodiscard]] std::vector<double> getAvailableSampleRates() const;

    /// Stellt das Audiogerät auf diese Rate um. Unter JACK/PipeWire bestimmt der Server die Rate;
    /// dann kommt eine Fehlermeldung mit Erklärung.
    [[nodiscard]] core::Result<void> setDeviceSampleRate (double sampleRate);

    /// Lässt sich das laufende Audiogerät auf diese Rate umstellen? Unter JACK/PipeWire meist nur die
    /// eine Rate des Servers.
    [[nodiscard]] bool canDeviceRunAt (double sampleRate) const;

    /// Läuft das Audiogerät mit der Rate des Projekts?
    [[nodiscard]] bool deviceMatchesProject() const;

private:
    void projectChanged() override;
    void applyToInputs (const ProjectSettings& settings);
    void applyAutoCrossfades (bool enabled);

    /// Neue Audio-Clips (Import, Aufnahme, Einfügen) bekommen die Crossfade-Einstellung des Projekts.
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    [[nodiscard]] ProjectSettings defaultSettings() const;

    engine::EngineContext& engineContext;
    ProjectService& projectService;
    juce::ValueTree observedEditState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSettingsService)
};

} // namespace fk::services
