#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::plugins::ui
{

/// Anzeige für Zahl oder kurzen Text in Monospace auf dunklem Grund (Tuner, Latenz, Hinweis).
class Lcd final : public juce::Component
{
public:
    explicit Lcd (juce::String initialText = {});

    void setText (const juce::String& newText);
    [[nodiscard]] const juce::String& getText() const noexcept;

    void paint (juce::Graphics& g) override;

private:
    juce::String text;
};

} // namespace fk::plugins::ui
