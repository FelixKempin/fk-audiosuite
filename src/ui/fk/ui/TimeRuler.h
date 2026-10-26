#pragma once

#include "fk/services/TimelineService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/TimeMapping.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace fk::ui
{

/// Zeitlineal über den Spuren: Takte und Schläge, Loop-Bereich.
/// Klick setzt die Wiedergabeposition, Ziehen setzt den Loop-Bereich (beides eingerastet).
class TimeRuler final : public juce::Component
{
public:
    TimeRuler (services::TransportService& transportService, services::TimelineService& timelineService);

    void setTimeMapping (TimeMapping newMapping);
    void setGridResolution (services::GridResolution resolution);
    void setSnapEnabled (bool shouldSnap);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    [[nodiscard]] double secondsAt (int x) const;

    services::TransportService& transport;
    services::TimelineService& timeline;
    TimeMapping mapping;
    services::GridResolution gridResolution = services::GridResolution::beat();
    bool snapEnabled = true;
    std::optional<double> dragStartSeconds;
};

} // namespace fk::ui
