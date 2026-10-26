#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::plugins::ui
{

/// Senkrechter Balken, der einen dB-Wert aus dem Prozessor anzeigt. Der Wert kommt über eine
/// Lesefunktion (z. B. CompressorProcessor::getGainReductionDb), die ein Atomic liest; der
/// Timer fragt sie 30-mal pro Sekunde im Message-Thread ab. Der Audio-Thread kennt das Meter nicht.
class Meter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Style
    {
        level,        ///< Pegel von -60 bis 0 dB, wächst von unten; Farbe nach Lautstärke
        gainReduction ///< Absenkung von 0 bis -24 dB, wächst von oben in Akzentfarbe
    };

    Meter (Style style, std::function<float()> readDb, juce::String label);

    void paint (juce::Graphics& g) override;

    [[nodiscard]] float getLastValueDb() const noexcept;

private:
    void timerCallback() override;

    Style style;
    std::function<float()> readDb;
    juce::String label;
    float lastValueDb = 0.0f;
};

} // namespace fk::plugins::ui
