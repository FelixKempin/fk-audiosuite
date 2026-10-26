#pragma once

#include "fk/services/SendService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::ui
{

/// Ein Send zu einer FX-Spur: Name, Pegelregler, Pre/Post-Schalter.
/// Tut selbst nichts am Modell; ruft nur die Callbacks.
class SendRow final : public juce::Component
{
public:
    std::function<void (float gainDb)> onLevelChanged;
    std::function<void (bool preFader)> onPreFaderChanged;

    SendRow();

    void setSend (const services::SendInfo& send);
    [[nodiscard]] const services::SendInfo& getSend() const noexcept;

    void resized() override;

private:
    services::SendInfo info;
    juce::Label nameLabel;
    juce::Slider levelSlider;
    juce::TextButton preButton;
};

} // namespace fk::ui
