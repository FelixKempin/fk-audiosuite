#include "fk/plugins/DistortionProcessor.h"

#include "fk/plugins/ui/TransferView.h"

#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto driveId = "drive";
constexpr auto shapeId = "shape";
constexpr auto toneId = "tone";
constexpr auto outputId = "output";
constexpr auto mixId = "mix";

dsp::nonlinear::ShapeType shapeForChoice (float choice)
{
    switch (static_cast<int> (choice))
    {
        case 1:
            return dsp::nonlinear::ShapeType::hard;
        case 2:
            return dsp::nonlinear::ShapeType::tube;
        default:
            return dsp::nonlinear::ShapeType::soft;
    }
}
} // namespace

DistortionProcessor::DistortionProcessor()
    : InternalPlugin (info(), createLayout())
    , driveDb (parameter (driveId))
    , shapeChoice (parameter (shapeId))
    , toneHz (parameter (toneId))
    , outputDb (parameter (outputId))
    , mix (parameter (mixId))
{
}

InternalPlugin::Info DistortionProcessor::info()
{
    return { "FK Distortion", "Verzerrung", 0x464b0005 };
}

juce::AudioProcessorValueTreeState::ParameterLayout DistortionProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { driveId, 1 },
                                                             TRANS ("Drive"),
                                                             juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f),
                                                             12.0f,
                                                             Attributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { shapeId, 1 }, TRANS ("Shape"), juce::StringArray { TRANS ("Soft"), TRANS ("Hard"), TRANS ("Tube") }, 0));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { toneId, 1 },
                                                     TRANS ("Tone"),
                                                     juce::NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.3f),
                                                     8000.0f,
                                                     Attributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { outputId, 1 },
                                                             TRANS ("Output"),
                                                             juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f),
                                                             -6.0f,
                                                             Attributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { mixId, 1 },
                                                             TRANS ("Mix"),
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
                                                             100.0f,
                                                             Attributes().withLabel ("%")));
    return layout;
}

void DistortionProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    oversampling.initProcessing (static_cast<std::size_t> (samplesPerBlock));
    oversampling.reset();
    dryBuffer.setSize (numChannels, samplesPerBlock);
    lastToneHz = -1.0f;

    for (auto& filter : toneFilters)
        filter.reset();

    setLatencySamples (static_cast<int> (oversampling.getLatencyInSamples()));
    updateToneIfNeeded();
}

void DistortionProcessor::releaseResources()
{
    oversampling.reset();
}

void DistortionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < numChannels)
        return;

    updateToneIfNeeded();
    waveshaper.setShape (shapeForChoice (shapeChoice.load()));
    waveshaper.setDriveDb (driveDb.load());
    waveshaper.setOutputDb (outputDb.load());

    const int numSamples = buffer.getNumSamples();

    // Trockenes Signal sichern, bevor wir es verändern.
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    // Verzerrung bei doppelter Abtastrate: Die neu entstehenden Obertöne liegen dann unterhalb
    // der halben (hohen) Abtastrate und falten sich nicht ins Hörbare zurück.
    juce::dsp::AudioBlock<float> block (buffer);
    auto oversampledBlock = oversampling.processSamplesUp (block);

    for (std::size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
        waveshaper.process (
            std::span<float> (oversampledBlock.getChannelPointer (channel), oversampledBlock.getNumSamples()));

    oversampling.processSamplesDown (block);

    const float wet = mix.load() * 0.01f;
    const float dry = 1.0f - wet;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        toneFilters[static_cast<std::size_t> (channel)].process (
            std::span<float> (buffer.getWritePointer (channel), static_cast<std::size_t> (numSamples)));
        buffer.applyGain (channel, 0, numSamples, wet);
        buffer.addFrom (channel, 0, dryBuffer, channel, 0, numSamples, dry);
    }
}

void DistortionProcessor::updateToneIfNeeded()
{
    const float tone = toneHz.load();

    if (tone == lastToneHz)
        return;

    lastToneHz = tone;
    const auto coefficients = dsp::filters::Biquad<float>::makeCoefficients (
        dsp::filters::FilterType::lowPass, currentSampleRate, tone, 0.707);

    for (auto& filter : toneFilters)
        filter.setCoefficients (coefficients);
}

ui::PanelDescription DistortionProcessor::describePanel()
{
    // Übertragungskennlinie der Sättigung: Eingangsamplitude → Ausgangsamplitude mit Drive, Form und Output.
    const auto transfer = [this] (float input)
    {
        const auto drive = juce::Decibels::decibelsToGain (driveDb.load());
        const auto output = juce::Decibels::decibelsToGain (outputDb.load());
        return output * dsp::nonlinear::Waveshaper::shape (drive * input, shapeForChoice (shapeChoice.load()));
    };

    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Curve"),
                   .scene =
                       [transfer]
                   {
                       return std::make_unique<ui::TransferView> (
                           transfer,
                           ui::TransferView::Axis { -1.0f, 1.0f, TRANS ("Input"), { -1.0f, 0.0f, 1.0f } },
                           ui::TransferView::Axis { -1.0f, 1.0f, TRANS ("Output"), { -1.0f, 0.0f, 1.0f } });
                   },
                   .sceneHeight = 180 },
                 { .title = TRANS ("Distortion"), .controls = { ui::knob (driveId), ui::knob (shapeId), ui::knob (toneId) } },
                 { .title = TRANS ("Output"), .controls = { ui::knob (outputId), ui::knob (mixId) } } } };
}

} // namespace fk::plugins
