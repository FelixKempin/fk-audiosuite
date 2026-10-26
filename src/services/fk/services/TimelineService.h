#pragma once

#include "fk/services/NoteValue.h"
#include "fk/services/ProjectService.h"

#include <juce_events/juce_events.h>

#include <optional>
#include <vector>

namespace fk::services
{

/// Rasterauflösung für Anzeige und Snap, in musikalischen Einheiten.
/// Raster wie in Cubase: Takt, Zählzeit der Taktart, ein Notenwert (1/1 bis 1/128, gerade,
/// punktiert, triolisch), Sekunden oder Samples. Ein Wert, kein Enum, weil Notenwerte Daten tragen.
struct GridResolution
{
    enum class Kind
    {
        bar,
        beat, ///< Zählzeit der Taktart (bei 6/8 eine Achtel)
        note, ///< fester Notenwert, siehe note
        seconds,
        samples
    };

    Kind kind = Kind::beat;
    NoteValue note {}; ///< nur bei Kind::note

    [[nodiscard]] static GridResolution bar() { return { Kind::bar, {} }; }
    [[nodiscard]] static GridResolution beat() { return { Kind::beat, {} }; }
    [[nodiscard]] static GridResolution of (NoteValue value) { return { Kind::note, value }; }
    [[nodiscard]] static GridResolution seconds() { return { Kind::seconds, {} }; }
    [[nodiscard]] static GridResolution samples() { return { Kind::samples, {} }; }

    /// Musikalisch (rastet auf Schläge) oder zeitbasiert (Sekunden, Samples)?
    [[nodiscard]] bool isMusical() const noexcept { return kind != Kind::seconds && kind != Kind::samples; }

    /// Kurzname für die Rasterwahl: "Takt", "Zählzeit", "1/8T", "Sekunden", "Samples".
    [[nodiscard]] juce::String shortName() const;

    bool operator== (const GridResolution& other) const
    {
        return kind == other.kind && (kind != Kind::note || note == other.note);
    }
};

/// Taktart als Wert, z. B. { 4, 4 } oder { 6, 8 }.
struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
};

/// Eine Rasterlinie für Lineal und Spuren.
struct GridLine
{
    double seconds = 0.0;
    bool isBarStart = false;
    int barNumber = 1; ///< 1-basiert, wie Musiker zählen
};

/// Loop-Bereich in Sekunden.
struct LoopRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

/// Ein gespeicherter Zoom: wie viele Sekunden die Spurbreite zeigt. Gilt für alle Projekte (Cubase:
/// Zoom-Presets); die Stelle bleibt, nur der Maßstab wechselt.
struct ZoomPreset
{
    juce::String name;
    double visibleSeconds = 0.0;
};

/// Anwendungsfälle rund um die musikalische Zeitachse: Tempo, Taktart, Raster, Snap,
/// Loop-Bereich und Metronom. Rechnet zwischen Sekunden und Takten um, damit die
/// Oberfläche nur Sekunden kennt und trotzdem Takte anzeigen und einrasten kann.
class TimelineService final : private ProjectService::Listener,
                              private juce::ValueTree::Listener,
                              private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Tempo, Taktart, Loop oder Metronom haben sich geändert.
        virtual void timelineChanged() = 0;
    };

    explicit TimelineService (ProjectService& projectService);
    ~TimelineService() override;

    [[nodiscard]] double getTempoBpm() const;
    void setTempoBpm (double bpm);

    [[nodiscard]] TimeSignature getTimeSignature() const;
    void setTimeSignature (TimeSignature signature);

    /// Position als "Takt.Schlag.Ticks", z. B. "003.02.240" (480 Ticks pro Schlag).
    [[nodiscard]] juce::String formatBarsBeats (double seconds) const;

    /// Position in Schlägen (Viertelnoten) ab Projektanfang, über die Tempospur.
    [[nodiscard]] double secondsToBeats (double seconds) const;
    [[nodiscard]] double beatsToSeconds (double beats) const;

    /// Liest eine Position "Takt.Schlag.Tick" (wie formatBarsBeats, Ticks 0..479; Teile dürfen fehlen).
    [[nodiscard]] std::optional<double> parseBarsBeats (const juce::String& text) const;

    /// Länge eines Takts in Sekunden an dieser Position (hängt von Tempo und Taktart ab).
    [[nodiscard]] double barLengthSeconds (double atSeconds) const;

    /// Rundet auf die nächste Rasterlinie.
    [[nodiscard]] double snapSeconds (double seconds, GridResolution resolution) const;

    /// Rasterweite in Sekunden an dieser Position (für Nudge: Clips um einen Rasterschritt schieben).
    [[nodiscard]] double gridStepSeconds (double atSeconds, GridResolution resolution) const;

    /// Alle Rasterlinien im Bereich, aufsteigend. minSpacingSeconds dünnt aus: Linien, die näher an der
    /// vorigen liegen, entfallen (Taktanfänge bleiben). Sekunden- und Samples-Raster zeigen Zählzeiten.
    [[nodiscard]] std::vector<GridLine> getGridLines (double fromSeconds,
                                                      double toSeconds,
                                                      GridResolution resolution,
                                                      double minSpacingSeconds = 0.0) const;

    /// Nächster Taktanfang hinter seconds bzw. voriger davor (für Vor- und Rücklauf).
    [[nodiscard]] double getNextBarSeconds (double seconds) const;
    [[nodiscard]] double getPreviousBarSeconds (double seconds) const;

    [[nodiscard]] bool isLooping() const;
    void setLooping (bool shouldLoop);
    [[nodiscard]] LoopRange getLoopRange() const;
    void setLoopRange (LoopRange range);

    /// Eigene Zoom-Presets, in den Programmeinstellungen (nicht im Projekt). Ohne Projekt leer.
    [[nodiscard]] std::vector<ZoomPreset> getZoomPresets() const;
    /// Gleicher Name ersetzt das alte Preset.
    [[nodiscard]] core::Result<void> saveZoomPreset (const ZoomPreset& preset);
    [[nodiscard]] core::Result<void> removeZoomPreset (const juce::String& name);

    [[nodiscard]] bool isMetronomeEnabled() const;
    void setMetronomeEnabled (bool shouldBeEnabled);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    /// Schrittweite der Auflösung in Viertelnoten (Tracktions interne Beat-Einheit).
    [[nodiscard]] double resolutionInQuarterNotes (GridResolution resolution) const;
    void storeZoomPresets (const std::vector<ZoomPreset>& presets);
    [[nodiscard]] double deviceSampleRate() const;

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineService)
};

} // namespace fk::services
