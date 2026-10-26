#pragma once

#include "fk/services/AutomationService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace fk::ui
{

/// Kopf einer Automationslage, links neben der Kurve: Wahl des Parameters, Plus für eine weitere
/// Lage, Minus zum Entfernen; in der ersten Lage zusätzlich der Modus (Read, Touch, Latch, Write)
/// der Spur. Alles landet im Projekt (Service).
class AutomationHeader final : public juce::Component, private services::AutomationService::Listener
{
public:
    std::function<void()> onSelected;

    AutomationHeader (services::AutomationService& automationService, services::TrackHandle& track, int laneIndex);
    ~AutomationHeader() override;

    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    static constexpr int noParameterItemId = 1;

    void automationChanged() override;

    services::AutomationService& automation;
    services::TrackHandle& track;
    int laneIndex;
    juce::ComboBox parameterBox;
    juce::ComboBox modeBox;
    IconButton addButton;
    IconButton removeButton;
    std::vector<services::AutomationParameterInfo> parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationHeader)
};

} // namespace fk::ui
