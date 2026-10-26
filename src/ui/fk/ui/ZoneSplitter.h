#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::ui
{

/// Waagerechter Trenner zwischen Projektzone und unterer Zone: Ziehen ändert die Höhe der
/// unteren Zone (Key Editor mit mehreren Controller-Lanes braucht Platz), Doppelklick setzt
/// sie zurück.
class ZoneSplitter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int thickness = 6;

    /// Neuer Abstand in Pixeln seit Beginn des Ziehens (positiv = nach unten).
    std::function<void (int distanceY)> onDragged;
    std::function<void()> onDragStarted;
    std::function<void()> onReset;

    ZoneSplitter();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
};

} // namespace fk::ui
