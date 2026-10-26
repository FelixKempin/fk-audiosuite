#include "fk/plugins/ReverbProcessor.h"

#include <algorithm>
#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto preDelayId = "predelay";
constexpr auto decayId = "decay";
constexpr auto sizeId = "size";
constexpr auto dampingId = "damping";
constexpr auto lowCutId = "lowcut";
constexpr auto diffusionId = "diffusion";
constexpr auto modulationId = "modulation";
constexpr auto widthId = "width";
constexpr auto mixId = "mix";
} // namespace

ReverbProcessor::ReverbProcessor()
    : InternalPlugin (info(), createLayout())
    , preDelayMs (parameter (preDelayId))
    , decaySeconds (parameter (decayId))
    , sizePercent (parameter (sizeId))
    , dampingHz (parameter (dampingId))
    , lowCutHz (parameter (lowCutId))
    , diffusionPercent (parameter (diffusionId))
    , modulationPercent (parameter (modulationId))
    , widthPercent (parameter (widthId))
    , mixPercent (parameter (mixId))
{
}

InternalPlugin::Info ReverbProcessor::info()
{
    return { "FK Reverb", "Raum", 0x464b0006 };
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverbProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add =
        [&layout] (
            const char* id, const char* name, juce::NormalisableRange<float> range, float value, const char* unit)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, juce::translate (name), range, value, Attributes().withLabel (unit)));
    };

    add (preDelayId, NEEDS_TRANS ("Pre-Delay"), { 0.0f, 250.0f, 0.1f, 0.5f }, 20.0f, "ms");
    add (decayId, NEEDS_TRANS ("Decay"), { 0.2f, 20.0f, 0.01f, 0.35f }, 2.0f, "s");
    add (sizeId, NEEDS_TRANS ("Size"), { 50.0f, 150.0f, 1.0f }, 100.0f, "%");
    add (dampingId, NEEDS_TRANS ("Damping"), { 1000.0f, 20000.0f, 1.0f, 0.4f }, 8000.0f, "Hz");
    add (lowCutId, NEEDS_TRANS ("Low Cut"), { 20.0f, 1000.0f, 1.0f, 0.4f }, 80.0f, "Hz");
    add (diffusionId, NEEDS_TRANS ("Diffusion"), { 0.0f, 100.0f, 1.0f }, 70.0f, "%");
    add (modulationId, NEEDS_TRANS ("Modulation"), { 0.0f, 100.0f, 1.0f }, 30.0f, "%");
    add (widthId, NEEDS_TRANS ("Width"), { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    add (mixId, NEEDS_TRANS ("Mix"), { 0.0f, 100.0f, 1.0f }, 25.0f, "%");
    return layout;
}

void ReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate);
    wetLeft.assign (static_cast<std::size_t> (std::max (samplesPerBlock, 1)), 0.0f);
    wetRight.assign (wetLeft.size(), 0.0f);
}

void ReverbProcessor::releaseResources()
{
    reverb.reset();
}

double ReverbProcessor::getTailLengthSeconds() const
{
    return static_cast<double> (decaySeconds.load()) + static_cast<double> (preDelayMs.load()) * 0.001;
}

void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2 || wetLeft.empty())
        return;

    reverb.setSettings ({ .preDelayMs = preDelayMs.load(),
                          .decaySeconds = decaySeconds.load(),
                          .size = sizePercent.load() * 0.01f,
                          .dampingHz = dampingHz.load(),
                          .lowCutHz = lowCutHz.load(),
                          .diffusion = diffusionPercent.load() * 0.01f,
                          .modulation = modulationPercent.load() * 0.01f,
                          .width = widthPercent.load() * 0.01f });

    const auto wet = mixPercent.load() * 0.01f;
    const auto dry = 1.0f - wet;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto total = static_cast<std::size_t> (buffer.getNumSamples());

    // In Stücken der vorbereiteten Puffergröße: Der Host darf ausnahmsweise größere Blöcke schicken.
    for (std::size_t start = 0; start < total; start += wetLeft.size())
    {
        const auto count = std::min (wetLeft.size(), total - start);
        reverb.process (std::span<const float> (left + start, count),
                        std::span<const float> (right + start, count),
                        std::span<float> (wetLeft.data(), count),
                        std::span<float> (wetRight.data(), count));

        for (std::size_t i = 0; i < count; ++i)
        {
            left[start + i] = dry * left[start + i] + wet * wetLeft[i];
            right[start + i] = dry * right[start + i] + wet * wetRight[i];
        }
    }
}

ui::PanelDescription ReverbProcessor::describePanel()
{
    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Room"),
                   .controls = { ui::knob (preDelayId, TRANS ("Gap before the first reflections: separates the voice from the room")),
                                 ui::knob (decayId, TRANS ("Reverb time: how long until the tail has decayed by 60 dB")),
                                 ui::knob (sizeId, TRANS ("Room size: spacing of the reflections")),
                                 ui::knob (diffusionId, TRANS ("Density: low = single echoes, high = smooth cloud")) } },
                 { .title = TRANS ("Tone"),
                   .controls = { ui::knob (dampingId, TRANS ("Highs decay faster: lower = darker, warmer")),
                                 ui::knob (lowCutId, TRANS ("Keeps bass out of the reverb so it does not boom")),
                                 ui::knob (modulationId, TRANS ("Slight movement against a metallic sound")),
                                 ui::knob (widthId, TRANS ("Stereo width of the reverb")) } },
                 { .title = TRANS ("Output"),
                   .controls = { ui::knob (mixId, TRANS ("Amount of reverb. On an FX track (send) use 100 %")) } } } };
}

} // namespace fk::plugins
