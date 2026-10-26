#pragma once

#include "fk/plugins/ui/Lcd.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::plugins::ui
{

/// Lcd, das seinen Text regelmäßig aus einer Funktion liest (10-mal pro Sekunde): Dateiname,
/// Stimmenzahl, Hinweise. Die Funktion läuft im Message-Thread und darf Atomics lesen.
class StatusText final : public juce::Component, private juce::Timer
{
public:
    explicit StatusText (std::function<juce::String()> readText);

    void resized() override;

private:
    void timerCallback() override;

    std::function<juce::String()> readText;
    Lcd lcd;
};

} // namespace fk::plugins::ui
