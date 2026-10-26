#pragma once

#include "fk/plugins/analysis/SpectrumAnalyser.h"
#include "fk/plugins/ui/FrequencyScale.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::plugins::ui
{

/// Hintergrund jeder Frequenzansicht: Raster (Frequenz- und dB-Linien mit Beschriftung) und das
/// Spektrum des Signals als gefüllte Fläche. Fragt den SpectrumAnalyser 30-mal pro Sekunde.
/// Kurve und Griffe liegen als eigene Komponenten darüber (CurveView, BandHandles) und nutzen
/// dieselbe FrequencyScale (getScale()).
class SpectrumView final : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (analysis::SpectrumAnalyser& analyser);

    /// Pegelbereich der y-Achse; EQ ±24 dB, Spektrum allein z. B. -90..0 dB.
    void setDecibelRange (float minDb, float maxDb);

    [[nodiscard]] FrequencyScale getScale() const noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateSpectrumPath();

    analysis::SpectrumAnalyser& analyser;
    FrequencyScale scale;
    juce::Path spectrumPath;
};

} // namespace fk::plugins::ui
