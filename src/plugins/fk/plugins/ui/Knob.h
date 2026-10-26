#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fk::plugins::ui
{

/// Drehregler für einen Parameter: Name oben, Regler, Wert mit Einheit unten. Doppelklick
/// setzt den Standardwert, Strg+Ziehen regelt fein. Die Bindung an den Parameter übernimmt
/// juce::SliderParameterAttachment (Listener-Muster, LEARNING.md §10a): Der Knob hält keinen
/// eigenen Zustand, er zeigt den Parameter und schreibt ihn.
class Knob final : public juce::Component
{
public:
    explicit Knob (juce::RangedAudioParameter& parameter);

    void resized() override;

    /// Anzeigename statt des Parameternamens (z. B. Regler eines eigenen Pedals: "Drive" statt "Knob 1").
    void setDisplayName (const juce::String& name);

    /// Der innere Slider, z. B. für Tests oder um Werte programmgesteuert zu setzen.
    [[nodiscard]] juce::Slider& getSlider() noexcept;

private:
    juce::Label nameLabel;
    juce::Slider slider;
    juce::SliderParameterAttachment attachment;
};

} // namespace fk::plugins::ui
