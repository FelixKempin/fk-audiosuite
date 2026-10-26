#pragma once

#include "fk/dsp/filters/Biquad.h"
#include "fk/dsp/nonlinear/Waveshaper.h"
#include "fk/plugins/InternalPlugin.h"

#include <juce_dsp/juce_dsp.h>

#include <array>

namespace fk::plugins
{

/// Verzerrer: Drive → Kennlinie (2-fach überabgetastet gegen Aliasing) → Klangregler (Tiefpass)
/// → Ausgang, mit Dry/Wet-Mischung.
class DistortionProcessor final : public InternalPlugin
{
public:
    DistortionProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

private:
    static constexpr int numChannels = 2;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateToneIfNeeded();

    std::atomic<float>& driveDb;
    std::atomic<float>& shapeChoice;
    std::atomic<float>& toneHz;
    std::atomic<float>& outputDb;
    std::atomic<float>& mix;

    double currentSampleRate = 44100.0;
    float lastToneHz = -1.0f;
    dsp::nonlinear::Waveshaper waveshaper;
    std::array<dsp::filters::Biquad<float>, numChannels> toneFilters;
    juce::dsp::Oversampling<float> oversampling { numChannels,
                                                  1,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::AudioBuffer<float> dryBuffer;
};

} // namespace fk::plugins
