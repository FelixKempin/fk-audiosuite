#pragma once

#include "fk/plugins/ui/FrequencyScale.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

namespace fk::plugins::ui
{

/// Ziehbare Punkte über einer Frequenzansicht, je einer pro Band: x ist die Frequenz, y der Gain,
/// das Mausrad ändert Q. Jeder Griff bindet über juce::ParameterAttachment an seine Parameter;
/// er hält keinen eigenen Wert. Griffe ohne Gain-Parameter (Hochpass) bewegen sich nur waagerecht.
/// Die Regler daneben bleiben (Guideline 10a): Der Griff ist der schnelle Weg, nicht der einzige.
class BandHandles final : public juce::Component, public juce::SettableTooltipClient
{
public:
    BandHandles();
    ~BandHandles() override;

    /// gain und q dürfen null sein. Die Farbe unterscheidet die Bänder.
    void addHandle (juce::RangedAudioParameter& frequency,
                    juce::RangedAudioParameter* gain,
                    juce::RangedAudioParameter* q,
                    juce::Colour colour,
                    juce::String label);

    void setScale (const FrequencyScale& newScale);

    /// Wird nach jeder Parameteränderung gerufen (CurveView::refresh).
    std::function<void()> onChanged;

    [[nodiscard]] int getNumHandles() const noexcept;
    [[nodiscard]] juce::Point<float> getHandleCentre (int index) const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    static constexpr float radius = 7.0f;

    struct Handle
    {
        juce::RangedAudioParameter& frequency;
        juce::RangedAudioParameter* gain;
        juce::RangedAudioParameter* q;
        juce::Colour colour;
        juce::String label;
        float frequencyHz = 1000.0f;
        float gainDb = 0.0f;
        float qValue = 0.707f;
        std::unique_ptr<juce::ParameterAttachment> frequencyAttachment;
        std::unique_ptr<juce::ParameterAttachment> gainAttachment;
        std::unique_ptr<juce::ParameterAttachment> qAttachment;
    };

    [[nodiscard]] int handleAt (juce::Point<float> position) const;
    void changed();

    FrequencyScale scale;
    std::vector<std::unique_ptr<Handle>> handles;
    int draggedHandle = -1;
    int hoveredHandle = -1;
};

} // namespace fk::plugins::ui
