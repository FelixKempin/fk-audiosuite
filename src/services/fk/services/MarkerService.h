#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <optional>
#include <vector>

namespace fk::services
{

/// Ein Marker für die Anzeige: eine benannte Position im Song (Strophe, Refrain, Solo).
struct MarkerInfo
{
    juce::String id; ///< stabile Kennung für MarkerService
    int number = 0;  ///< fortlaufende Nummer, wie Tracktion sie vergibt
    juce::String name;
    double startSeconds = 0.0;
};

/// Markerspur: Marker setzen, verschieben, umbenennen, löschen, zum nächsten/vorigen springen.
///
/// Tracktion speichert Marker als Clips auf seiner Markerspur und gibt ihnen immer eine Länge
/// (ohne Angabe einen Takt). Wir behandeln sie wie Cubase als Position; ein Abschnitt reicht
/// bis zum nächsten Marker. Marker folgen dem Takt, wenn sich das Tempo ändert.
class MarkerService final : private ProjectService::Listener, private juce::ChangeListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void markersChanged() = 0;
    };

    explicit MarkerService (ProjectService& projectService);
    ~MarkerService() override;

    /// Alle Marker, nach Position sortiert.
    [[nodiscard]] std::vector<MarkerInfo> getMarkers() const;

    /// Setzt einen Marker; ohne Namen heißt er "Marker <Nummer>". Liefert die Kennung.
    [[nodiscard]] core::Result<juce::String> addMarker (double seconds, const juce::String& name = {});
    [[nodiscard]] core::Result<void> moveMarker (const juce::String& markerId, double seconds);
    [[nodiscard]] core::Result<void> renameMarker (const juce::String& markerId, const juce::String& name);
    [[nodiscard]] core::Result<void> deleteMarker (const juce::String& markerId);

    /// Position des nächsten Markers hinter seconds, falls es einen gibt.
    [[nodiscard]] std::optional<double> getNextMarkerSeconds (double seconds) const;

    /// Position des vorigen Markers. Steht man knapp hinter einem Marker (unter 0,25 s, z. B. weil
    /// der Transport läuft), geht es zum davor, damit wiederholtes Drücken weiterspringt.
    [[nodiscard]] std::optional<double> getPreviousMarkerSeconds (double seconds) const;

    /// Abschnitt eines Markers: von ihm bis zum nächsten (oder einen Takt lang beim letzten).
    [[nodiscard]] std::optional<std::pair<double, double>> getSection (const juce::String& markerId) const;

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    [[nodiscard]] te::MarkerClip* findMarker (const juce::String& markerId) const;

    ProjectService& projectService;
    te::Edit* observedEdit = nullptr; ///< dessen MarkerManager wir beobachten; nur zum Abmelden
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerService)
};

} // namespace fk::services
