#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::plugins::ui
{

/// Kennlinie Eingang → Ausgang (Kompressor in dB, Verzerrer als Wellenform-Übertragung).
/// Die Kurve kommt aus einer Funktion, die die aktuellen Parameter liest; die Ansicht tastet sie
/// je Pixelspalte ab und zeichnet 30-mal pro Sekunde neu, damit Reglerbewegungen sofort sichtbar
/// sind. Dazu die Diagonale "Ausgang = Eingang" als Bezug und optional ein Live-Punkt (aktueller Pegel).
class TransferView final : public juce::Component, private juce::Timer
{
public:
    struct Axis
    {
        float minimum = 0.0f;
        float maximum = 1.0f;
        juce::String label;       ///< z. B. "Eingang dB"
        std::vector<float> ticks; ///< beschriftete Rasterwerte
    };

    using Transfer = std::function<float (float input)>;
    using LivePoint = std::function<std::optional<juce::Point<float>>()>; ///< (Eingang, Ausgang) in Achseneinheiten

    TransferView (Transfer transfer, Axis xAxis, Axis yAxis, LivePoint livePoint = {});
    ~TransferView() override;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    [[nodiscard]] juce::Rectangle<float> plotArea() const;
    [[nodiscard]] float xFor (float value) const;
    [[nodiscard]] float yFor (float value) const;

    Transfer transfer;
    Axis xAxis;
    Axis yAxis;
    LivePoint livePoint;
};

} // namespace fk::plugins::ui
