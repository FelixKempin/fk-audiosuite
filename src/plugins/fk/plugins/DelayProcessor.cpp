#include "fk/plugins/DelayProcessor.h"

#include <algorithm>
#include <array>
#include <span>

namespace fk::plugins
{

namespace
{
constexpr auto syncId = "sync";
constexpr auto timeId = "time";
constexpr auto noteId = "note";
constexpr auto feedbackId = "feedback";
constexpr auto pingPongId = "pingpong";
constexpr auto lowCutId = "lowcut";
constexpr auto highCutId = "highcut";
constexpr auto mixId = "mix";

// Notenwerte: Name und Länge in Viertelnoten (punktiert ×1,5, triolisch ×2/3).
struct NoteChoice
{
    const char* name;
    double quarters;
};

constexpr std::array<NoteChoice, 12> noteChoices { { { "1/32", 0.125 },
                                                     { "1/16T", 0.25 * 2.0 / 3.0 },
                                                     { "1/16", 0.25 },
                                                     { "1/16.", 0.375 },
                                                     { "1/8T", 0.5 * 2.0 / 3.0 },
                                                     { "1/8", 0.5 },
                                                     { "1/8.", 0.75 },
                                                     { "1/4T", 2.0 / 3.0 },
                                                     { "1/4", 1.0 },
                                                     { "1/4.", 1.5 },
                                                     { "1/2", 2.0 },
                                                     { "1/1", 4.0 } } };
constexpr int defaultNote = 6; // 1/8 punktiert: der Klassiker
} // namespace

DelayProcessor::DelayProcessor()
    : InternalPlugin (info(), createLayout())
    , sync (parameter (syncId))
    , timeMs (parameter (timeId))
    , note (parameter (noteId))
    , feedbackPercent (parameter (feedbackId))
    , pingPong (parameter (pingPongId))
    , lowCutHz (parameter (lowCutId))
    , highCutHz (parameter (highCutId))
    , mixPercent (parameter (mixId))
{
}

InternalPlugin::Info DelayProcessor::info()
{
    return { "FK Delay", "Raum", 0x464b0007 };
}

double DelayProcessor::noteLengthInQuarters (int choiceIndex) noexcept
{
    return noteChoices[static_cast<std::size_t> (
                           std::clamp (choiceIndex, 0, static_cast<int> (noteChoices.size()) - 1))]
        .quarters;
}

juce::AudioProcessorValueTreeState::ParameterLayout DelayProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray noteNames;

    for (const auto& choice : noteChoices)
        noteNames.add (choice.name);

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { syncId, 1 }, TRANS ("Sync"), true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { timeId, 1 },
                                                             TRANS ("Time"),
                                                             juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f),
                                                             375.0f,
                                                             Attributes().withLabel ("ms")));
    layout.add (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { noteId, 1 }, TRANS ("Note"), noteNames, defaultNote));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { feedbackId, 1 },
                                                             TRANS ("Feedback"),
                                                             juce::NormalisableRange<float> (0.0f, 95.0f, 1.0f),
                                                             35.0f,
                                                             Attributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pingPongId, 1 }, TRANS ("Ping-Pong"), false));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { lowCutId, 1 },
                                                     TRANS ("Low Cut"),
                                                     juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f),
                                                     120.0f,
                                                     Attributes().withLabel ("Hz")));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { highCutId, 1 },
                                                     TRANS ("High Cut"),
                                                     juce::NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f),
                                                     6000.0f,
                                                     Attributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { mixId, 1 },
                                                             TRANS ("Mix"),
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
                                                             30.0f,
                                                             Attributes().withLabel ("%")));
    return layout;
}

void DelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    delay.prepare (sampleRate);
    wetLeft.assign (static_cast<std::size_t> (std::max (samplesPerBlock, 1)), 0.0f);
    wetRight.assign (wetLeft.size(), 0.0f);
}

void DelayProcessor::releaseResources()
{
    delay.reset();
}

double DelayProcessor::getTailLengthSeconds() const
{
    // Bis die Rückkopplung unter −60 dB ist: Anzahl Wiederholungen mal Verzögerung.
    const auto feedback = std::clamp (static_cast<double> (feedbackPercent.load()) * 0.01, 0.0, 0.95);
    const auto repeats = feedback > 0.001 ? std::log (0.001) / std::log (feedback) : 1.0;
    return static_cast<double> (currentDelaySeconds.load()) * (repeats + 1.0);
}

float DelayProcessor::getDelaySeconds() const noexcept
{
    // Vor der ersten Wiedergabe gibt es noch kein Host-Tempo: dann aus den Reglern mit 120 BPM.
    const auto current = currentDelaySeconds.load();
    return current > 0.0f ? current : computeDelaySeconds (120.0);
}

float DelayProcessor::computeDelaySeconds (double bpm) const noexcept
{
    return sync.load() > 0.5f ? static_cast<float> (noteLengthInQuarters (static_cast<int> (note.load())) * 60.0 / bpm)
                              : timeMs.load() * 0.001f;
}

void DelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2 || wetLeft.empty())
        return;

    // Tempo vom Host; ohne Wiedergabeposition 120 BPM.
    double bpm = 120.0;

    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto hostBpm = position->getBpm(); hostBpm && *hostBpm > 0.0)
                bpm = *hostBpm;

    const auto seconds = computeDelaySeconds (bpm);
    currentDelaySeconds.store (seconds);

    delay.setSettings ({ .delaySeconds = seconds,
                         .feedback = feedbackPercent.load() * 0.01f,
                         .pingPong = pingPong.load() > 0.5f,
                         .lowCutHz = lowCutHz.load(),
                         .highCutHz = highCutHz.load() });

    const auto wet = mixPercent.load() * 0.01f;
    const auto dry = 1.0f - wet;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto total = static_cast<std::size_t> (buffer.getNumSamples());

    for (std::size_t start = 0; start < total; start += wetLeft.size())
    {
        const auto count = std::min (wetLeft.size(), total - start);
        delay.process (std::span<const float> (left + start, count),
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

ui::PanelDescription DelayProcessor::describePanel()
{
    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Time"),
                   .controls = { ui::knob (syncId, TRANS ("Time in the project's tempo (note value) instead of milliseconds")),
                                 ui::knob (noteId, TRANS ("Note value when synced: . = dotted, T = triplet")),
                                 ui::knob (timeId, TRANS ("Time in milliseconds when Sync is off")),
                                 ui::status (
                                     [this] {
                                         return juce::String (juce::roundToInt (getDelaySeconds() * 1000.0f)) + " ms";
                                     }) } },
                 { .title = TRANS ("Repeats"),
                   .controls = { ui::knob (feedbackId, TRANS ("How much of the echo is fed back: more = more repeats")),
                                 ui::knob (pingPongId, TRANS ("Echoes alternate between left and right")),
                                 ui::knob (lowCutId, TRANS ("Each repeat gets thinner: keeps the bass clean")),
                                 ui::knob (highCutId, TRANS ("Each repeat gets duller, like a tape echo")) } },
                 { .title = TRANS ("Output"),
                   .controls = { ui::knob (mixId, TRANS ("Amount of echo. On an FX track (send) use 100 %")) } } } };
}

} // namespace fk::plugins
