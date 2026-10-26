#pragma once

#include "fk/services/TrackService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace fk::ui
{

/// Studio-Verbindungen: alle Ein- und Ausgänge des Interfaces mit eigenem Namen ("Gitarren-DI" statt
/// "Eingang 3"). Die Namen gelten für alle Projekte und erscheinen überall, wo Anschlüsse gewählt werden.
class StudioConnectionsDialog final : public juce::Component
{
public:
    explicit StudioConnectionsDialog (services::TrackService& trackService);

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (services::TrackService& trackService);

private:
    /// Eine Zeile: Treibername links, eigener Name rechts.
    struct Row
    {
        juce::String portId;
        juce::Label name;
        juce::TextEditor alias;
    };

    void applyAlias (Row& row);

    services::TrackService& tracks;
    juce::Label inputsHeading;
    juce::Label outputsHeading;
    juce::Label emptyHint;
    juce::Component rowHolder;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> inputRows;
    std::vector<std::unique_ptr<Row>> outputRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioConnectionsDialog)
};

} // namespace fk::ui
