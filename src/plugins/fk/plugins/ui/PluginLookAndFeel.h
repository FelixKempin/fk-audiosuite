#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::plugins::ui
{

/// Ein Stil für alle Plugin-Oberflächen und die DAW: Theme-Tokens als JUCE-Standardfarben,
/// dazu eigene Vektorzeichnungen für Drehregler, Schalter und Auswahllisten. Keine Bitmaps:
/// Alles skaliert mit der Fenstergröße.
class PluginLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PluginLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool isButtonDown,
                       int buttonX,
                       int buttonY,
                       int buttonW,
                       int buttonH,
                       juce::ComboBox& box) override;

    juce::Font getLabelFont (juce::Label& label) override;
};

} // namespace fk::plugins::ui
