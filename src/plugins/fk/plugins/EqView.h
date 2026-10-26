#pragma once

#include "fk/plugins/ui/BandHandles.h"
#include "fk/plugins/ui/CurveView.h"
#include "fk/plugins/ui/SpectrumView.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fk::plugins
{

class EqProcessor;

/// Szene des EQ: Spektrum des Ausgangssignals, darüber der Frequenzgang aus den aktuellen
/// Parametern, darüber ein Griff je Band. Drei Lagen, eine Skala.
class EqView final : public juce::Component
{
public:
    explicit EqView (EqProcessor& processor);

    void resized() override;

private:
    ui::SpectrumView spectrum;
    ui::CurveView curve;
    ui::BandHandles handles;
};

} // namespace fk::plugins
