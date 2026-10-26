#pragma once

#include "fk/services/MarkerService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/TrackLane.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

namespace fk::ui
{

/// Markerspur unter der Signaturspur: benannte Positionen wie Strophe, Refrain, Solo.
/// Klick auf einen Marker springt dorthin, Ziehen verschiebt ihn (Snap gilt), Doppelklick benennt
/// ihn um, Doppelklick ins Leere setzt einen neuen, Rechtsklick bietet Loop, Umbenennen, Löschen.
/// Links steht in Spurkopf-Breite der Name und ein Plus-Button (Marker am Positionszeiger).
class MarkerTrack final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private services::MarkerService::Listener
{
public:
    static constexpr int height = 20;

    MarkerTrack (services::MarkerService& markerService,
                 services::TransportService& transportService,
                 services::TimelineService& timelineService);
    ~MarkerTrack() override;

    void setSettings (LaneSettings newSettings);

    /// Abgelehnte Aktion als Hinweis anzeigen.
    std::function<void (const juce::String& message)> onError;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    struct Drag
    {
        juce::String markerId;
        double seconds = 0.0;
        bool moved = false;
    };

    void markersChanged() override;
    [[nodiscard]] int xForSeconds (double seconds) const;
    [[nodiscard]] double secondsForX (int x, bool snap) const;
    [[nodiscard]] int findMarkerAt (int x) const;
    void showMarkerMenu (const services::MarkerInfo& marker);
    void askForName (const services::MarkerInfo& marker);
    void addMarkerAt (double seconds);
    void report (const core::Error& error) const;

    services::MarkerService& markers;
    services::TransportService& transport;
    services::TimelineService& timeline;
    LaneSettings settings;
    std::vector<services::MarkerInfo> markerList;
    std::optional<Drag> drag;
    IconButton addButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerTrack)
};

} // namespace fk::ui
