#include "fk/plugins/ui/PluginLookAndFeel.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

PluginLookAndFeel::PluginLookAndFeel()
{
    setColourScheme ({ theme::backgroundBase,     // windowBackground
                       theme::backgroundPanel,    // widgetBackground
                       theme::backgroundElevated, // menuBackground
                       theme::borderStrong,       // outline
                       theme::textPrimary,        // defaultText
                       theme::backgroundInput,    // defaultFill
                       theme::textPrimary,        // highlightedText
                       theme::accent,             // highlightedFill
                       theme::textPrimary });     // menuText

    setColour (juce::ResizableWindow::backgroundColourId, theme::backgroundBase);
    setColour (juce::TextButton::buttonColourId, theme::backgroundElevated);
    setColour (juce::TextButton::buttonOnColourId, theme::accent);
    setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    setColour (juce::TextButton::textColourOnId, theme::textPrimary);
    setColour (juce::ToggleButton::textColourId, theme::textPrimary);
    setColour (juce::ToggleButton::tickColourId, theme::accent);
    setColour (juce::ComboBox::backgroundColourId, theme::backgroundInput);
    setColour (juce::ComboBox::outlineColourId, theme::borderSubtle);
    setColour (juce::ComboBox::textColourId, theme::textPrimary);
    setColour (juce::ComboBox::arrowColourId, theme::textSecondary);
    setColour (juce::Label::textColourId, theme::textPrimary);
    setColour (juce::Slider::backgroundColourId, theme::backgroundInput);
    setColour (juce::Slider::trackColourId, theme::accent);
    setColour (juce::Slider::thumbColourId, theme::textPrimary);
    setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, theme::borderStrong);
    setColour (juce::Slider::textBoxTextColourId, theme::textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::accentSoft);
    setColour (juce::ScrollBar::thumbColourId, theme::borderStrong);
    setColour (juce::PopupMenu::backgroundColourId, theme::backgroundElevated);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (juce::TooltipWindow::backgroundColourId, theme::backgroundElevated);
    setColour (juce::TooltipWindow::textColourId, theme::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, theme::borderStrong);
}

void PluginLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                          int x,
                                          int y,
                                          int width,
                                          int height,
                                          float sliderPosProportional,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& slider)
{
    // Drehregler als drei Vektorformen: grauer Bogen (Weg), Akzentbogen (Wert), Zeiger.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto lineWidth = juce::jmax (2.0f, radius * 0.12f);
    const auto arcRadius = radius - lineWidth * 0.5f;
    const bool enabled = slider.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolare Regler (z. B. Gain in dB von -24..+24) füllen von der Mitte aus, nicht vom Anfang.
    const auto range = slider.getRange();
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    const auto zeroAngle = bipolar ? rotaryStartAngle + static_cast<float> (-range.getStart() / range.getLength()) *
                                                            (rotaryEndAngle - rotaryStartAngle)
                                   : rotaryStartAngle;

    if (enabled)
    {
        juce::Path value;
        value.addCentredArc (centre.x,
                             centre.y,
                             arcRadius,
                             arcRadius,
                             0.0f,
                             juce::jmin (zeroAngle, angle),
                             juce::jmax (zeroAngle, angle),
                             true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value,
                      juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto knobRadius = arcRadius - lineWidth * 1.5f;
    g.setColour (enabled ? theme::backgroundElevated : theme::backgroundPanel);
    g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));
    g.setColour (theme::borderStrong);
    g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (
        -lineWidth * 0.5f, -knobRadius + 3.0f, lineWidth, knobRadius * 0.45f, lineWidth * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (enabled ? theme::textPrimary : theme::textDisabled);
    g.fillPath (pointer);
}

void PluginLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool)
{
    // Schalter als Pille: links aus, rechts an. Der Text steht daneben.
    const auto area = button.getLocalBounds().toFloat();
    const auto pillHeight = juce::jmin (18.0f, area.getHeight() - 4.0f);
    const auto pill = juce::Rectangle<float> (pillHeight * 1.8f, pillHeight)
                          .withCentre ({ area.getX() + pillHeight * 0.9f + 2.0f, area.getCentreY() });
    const bool on = button.getToggleState();
    const bool enabled = button.isEnabled();

    g.setColour (on && enabled ? theme::accent : theme::backgroundInput);
    g.fillRoundedRectangle (pill, pillHeight * 0.5f);
    g.setColour (shouldDrawButtonAsHighlighted ? theme::textSecondary : theme::borderStrong);
    g.drawRoundedRectangle (pill, pillHeight * 0.5f, 1.0f);

    const auto thumbDiameter = pillHeight - 4.0f;
    const auto thumbX = on ? pill.getRight() - 2.0f - thumbDiameter : pill.getX() + 2.0f;
    g.setColour (enabled ? theme::textPrimary : theme::textDisabled);
    g.fillEllipse (thumbX, pill.getY() + 2.0f, thumbDiameter, thumbDiameter);

    g.setColour (enabled ? theme::textPrimary : theme::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawFittedText (button.getButtonText(),
                      area.withTrimmedLeft (pill.getRight() + theme::spacingSmall).toNearestInt(),
                      juce::Justification::centredLeft,
                      1);
}

void PluginLookAndFeel::drawComboBox (
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto area = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (area, theme::cornerRadius);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (area, theme::cornerRadius, 1.0f);

    // Pfeil nach unten als kleiner Pfad rechts.
    juce::Path arrow;
    const auto arrowArea = area.removeFromRight (static_cast<float> (height)).reduced (6.0f);
    arrow.startNewSubPath (arrowArea.getX(), arrowArea.getCentreY() - 2.0f);
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    arrow.lineTo (arrowArea.getRight(), arrowArea.getCentreY() - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

juce::Font PluginLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

} // namespace fk::plugins::ui
