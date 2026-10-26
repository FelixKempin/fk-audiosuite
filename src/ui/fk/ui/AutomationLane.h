#pragma once

#include "fk/services/AutomationService.h"
#include "fk/services/TimelineService.h"
#include "fk/ui/TrackLane.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Automationsspur unter einer Spur: zeichnet die Kurve des angezeigten Parameters über der
/// Zeitachse. Klick setzt einen Punkt (am Raster, wenn Einrasten an ist), Ziehen verschiebt ihn,
/// Doppelklick oder Rechtsklick entfernt ihn. Punkte überholen ihre Nachbarn nicht.
/// Alle Änderungen laufen über den AutomationService; die Spur selbst hält nur Kopien zum Zeichnen.
class AutomationLane final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private services::AutomationService::Listener
{
public:
    std::function<void()> onSelected;

    AutomationLane (services::AutomationService& automationService,
                    services::TimelineService& timelineService,
                    services::TrackHandle& track,
                    int laneIndex,
                    juce::Colour trackColour);
    ~AutomationLane() override;

    void setSettings (LaneSettings newSettings);
    void refresh();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    static constexpr float pointRadius = 5.0f;
    static constexpr int verticalMargin = 6;

    void automationChanged() override;
    [[nodiscard]] float yForValue (float value) const;
    [[nodiscard]] float valueForY (float y) const;
    [[nodiscard]] double secondsForX (int x) const;
    [[nodiscard]] int findPointAt (juce::Point<float> position) const;
    void removePointAt (int index);
    void reportError (const core::Error& error) const;

    services::AutomationService& automation;
    services::TimelineService& timeline;
    services::TrackHandle& track;
    int laneIndex;
    juce::Colour colour;
    LaneSettings settings;
    std::optional<services::AutomationParameterInfo> parameter;
    std::vector<services::AutomationPointInfo> points;
    int draggedPoint = -1;
    int hoveredPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationLane)
};

} // namespace fk::ui
