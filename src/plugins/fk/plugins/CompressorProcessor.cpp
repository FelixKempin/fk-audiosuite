#include "fk/plugins/CompressorProcessor.h"

#include "fk/dsp/dynamics/GainComputer.h"
#include "fk/plugins/ui/TransferView.h"

#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto thresholdId = "threshold";
constexpr auto ratioId = "ratio";
constexpr auto attackId = "attack";
constexpr auto releaseId = "release";
constexpr auto kneeId = "knee";
constexpr auto makeupId = "makeup";
constexpr auto sidechainId = "sidechain";
} // namespace

CompressorProcessor::CompressorProcessor()
    : InternalPlugin (info(), createLayout())
    , thresholdDb (parameter (thresholdId))
    , ratio (parameter (ratioId))
    , attackMs (parameter (attackId))
    , releaseMs (parameter (releaseId))
    , kneeDb (parameter (kneeId))
    , makeupDb (parameter (makeupId))
    , externalSidechain (parameter (sidechainId))
{
}

InternalPlugin::Info CompressorProcessor::info()
{
    return { .name = "FK Compressor", .category = "Dynamik", .uniqueId = 0x464b0003, .hasSidechain = true };
}

juce::AudioProcessorValueTreeState::ParameterLayout CompressorProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { thresholdId, 1 },
                                                             TRANS ("Threshold"),
                                                             juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f),
                                                             -18.0f,
                                                             Attributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ratioId, 1 },
                                                             TRANS ("Ratio"),
                                                             juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.4f),
                                                             4.0f,
                                                             Attributes().withLabel (":1")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { attackId, 1 },
                                                             TRANS ("Attack"),
                                                             juce::NormalisableRange<float> (0.1f, 200.0f, 0.1f, 0.3f),
                                                             10.0f,
                                                             Attributes().withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { releaseId, 1 },
                                                             TRANS ("Release"),
                                                             juce::NormalisableRange<float> (5.0f, 2000.0f, 1.0f, 0.3f),
                                                             100.0f,
                                                             Attributes().withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kneeId, 1 },
                                                             TRANS ("Knee"),
                                                             juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
                                                             6.0f,
                                                             Attributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { makeupId, 1 },
                                                             TRANS ("Makeup"),
                                                             juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
                                                             0.0f,
                                                             Attributes().withLabel ("dB")));
    layout.add (
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID { sidechainId, 1 }, TRANS ("Ext. Sidechain"), false));
    return layout;
}

void CompressorProcessor::prepareToPlay (double sampleRate, int)
{
    compressor.prepare (sampleRate);
}

void CompressorProcessor::releaseResources()
{
    compressor.reset();
}

void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    compressor.setThresholdDb (thresholdDb.load());
    compressor.setRatio (ratio.load());
    compressor.setKneeDb (kneeDb.load());
    compressor.setAttackMs (attackMs.load());
    compressor.setReleaseMs (releaseMs.load());
    compressor.setMakeupGainDb (makeupDb.load());

    const auto numSamples = static_cast<std::size_t> (buffer.getNumSamples());
    const std::span<float> left (buffer.getWritePointer (0), numSamples);
    const std::span<float> right (buffer.getWritePointer (1), numSamples);

    // Externe Sidechain: messen an der anderen Spur, absenken das eigene Signal.
    auto sidechain = externalSidechain.load() > 0.5f ? sidechainOf (buffer) : std::nullopt;
    auto& detector = sidechain ? *sidechain : buffer;
    inputLevelDb.store (
        juce::Decibels::gainToDecibels (juce::jmax (detector.getMagnitude (0, 0, buffer.getNumSamples()),
                                                    detector.getMagnitude (1, 0, buffer.getNumSamples())),
                                        -100.0f));

    if (sidechain)
        compressor.process (left,
                            right,
                            std::span<const float> (sidechain->getReadPointer (0), numSamples),
                            std::span<const float> (sidechain->getReadPointer (1), numSamples));
    else
        compressor.process (left, right);

    gainReductionDb.store (compressor.getCurrentGainReductionDb());
}

float CompressorProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load();
}

float CompressorProcessor::getInputLevelDb() const noexcept
{
    return inputLevelDb.load();
}

float CompressorProcessor::transferDb (float inputDb) const noexcept
{
    dsp::dynamics::GainComputer computer;
    computer.setThresholdDb (thresholdDb.load());
    computer.setRatio (ratio.load());
    computer.setKneeDb (kneeDb.load());
    return inputDb + computer.computeGainReductionDb (inputDb) + makeupDb.load();
}

ui::PanelDescription CompressorProcessor::describePanel()
{
    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Curve"),
                   .scene =
                       [this]
                   {
                       return std::make_unique<ui::TransferView> (
                           [this] (float inputDb) { return transferDb (inputDb); },
                           ui::TransferView::Axis { -60.0f, 0.0f, TRANS ("Input dB"), { -48.0f, -36.0f, -24.0f, -12.0f } },
                           ui::TransferView::Axis {
                               -60.0f, 12.0f, TRANS ("Output dB"), { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f } },
                           [this]() -> std::optional<juce::Point<float>>
                           {
                               const auto level = getInputLevelDb();

                               if (level < -59.0f)
                                   return std::nullopt;

                               return juce::Point<float> (level, level + getGainReductionDb() + makeupDb.load());
                           });
                   },
                   .sceneHeight = 200 },
                 { .title = TRANS ("Dynamics"),
                   .controls = { ui::knob (thresholdId),
                                 ui::knob (ratioId),
                                 ui::knob (kneeId),
                                 ui::knob (
                                     sidechainId,
                                     TRANS ("Measure the level of another track (e.g. the kick ducks the bass). Source: right-click the insert slot, then Sidechain source")) } },
                 { .title = TRANS ("Time"), .controls = { ui::knob (attackId), ui::knob (releaseId) } },
                 { .title = TRANS ("Output"),
                   .controls = { ui::knob (makeupId),
                                 ui::gainReduction ([this] { return getGainReductionDb(); }) } } } };
}

} // namespace fk::plugins
