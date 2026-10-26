#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fk::plugins::ui
{

/// Schalter (Pille) für einen Bool-Parameter, Name daneben. Bindung über ButtonParameterAttachment.
class Switch final : public juce::Component
{
public:
    explicit Switch (juce::RangedAudioParameter& parameter);

    void resized() override;

    [[nodiscard]] juce::ToggleButton& getButton() noexcept;

private:
    juce::ToggleButton button;
    juce::ButtonParameterAttachment attachment;
};

} // namespace fk::plugins::ui
