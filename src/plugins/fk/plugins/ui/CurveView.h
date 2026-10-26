#pragma once

#include "fk/plugins/ui/FrequencyScale.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::plugins::ui
{

/// Zeichnet einen Frequenzgang als Linie über eine FrequencyScale (meist über einer SpectrumView).
/// Die Kurve kommt aus einer Funktion Hz → dB, die der Prozessor liefert (z. B. aus seinen
/// Filterkoeffizienten); die Ansicht tastet sie je Pixelspalte ab. refresh() nach Parameteränderungen.
class CurveView final : public juce::Component
{
public:
    using ResponseFunction = std::function<float (float hz)>;

    explicit CurveView (ResponseFunction response);

    void setScale (const FrequencyScale& newScale);
    void refresh();

    void paint (juce::Graphics& g) override;

private:
    void updatePath();

    ResponseFunction response;
    FrequencyScale scale;
    juce::Path curve;
    juce::Path fill;
};

} // namespace fk::plugins::ui
