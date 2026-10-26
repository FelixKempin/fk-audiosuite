#include "fk/plugins/MidiEffectProcessors.h"

#include <array>

namespace fk::plugins
{

namespace
{
constexpr auto midiCategory = "MIDI-Effekt";

const std::array<std::pair<const char*, double>, 7> rates { { { "1/4", 1.0 },
                                                              { "1/8", 0.5 },
                                                              { NEEDS_TRANS ("1/8 Triplet"), 1.0 / 3.0 },
                                                              { "1/16", 0.25 },
                                                              { NEEDS_TRANS ("1/16 Triplet"), 1.0 / 6.0 },
                                                              { "1/32", 0.125 },
                                                              { "1/2", 2.0 } } };

int intOf (const std::atomic<float>& value)
{
    return juce::roundToInt (value.load());
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Transpose

TransposeProcessor::TransposeProcessor()
    : InternalPlugin (info(), createLayout())
    , semitones (parameter ("semitones"))
    , velocityOffset (parameter ("velocity"))
{
}

InternalPlugin::Info TransposeProcessor::info()
{
    return { .name = "FK Transpose", .category = midiCategory, .uniqueId = 0x464b0201, .isMidiEffect = true };
}

juce::AudioProcessorValueTreeState::ParameterLayout TransposeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "semitones", 1 }, TRANS ("Transpose"), -24, 24, 0,
                                                           juce::AudioParameterIntAttributes().withLabel (TRANS ("st"))));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "velocity", 1 }, TRANS ("Velocity"), -64, 64, 0));
    return layout;
}

void TransposeProcessor::prepareToPlay (double, int)
{
    transposer.reset();
}

void TransposeProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    transposer.process (midi, intOf (semitones), intOf (velocityOffset));
}

ui::PanelDescription TransposeProcessor::describePanel()
{
    return { .title = info().name,
             .sections = { { .title = "MIDI",
                             .controls = { ui::knob ("semitones", TRANS ("Semitones, +/-2 octaves")),
                                           ui::knob ("velocity", TRANS ("Shift the velocity of all notes")) } } } };
}

// ---------------------------------------------------------------------------------------------
// Chord

ChordProcessor::ChordProcessor()
    : InternalPlugin (info(), createLayout())
    , kind (parameter ("chord"))
{
}

InternalPlugin::Info ChordProcessor::info()
{
    return { .name = "FK Chord", .category = midiCategory, .uniqueId = 0x464b0202, .isMidiEffect = true };
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "chord", 1 }, TRANS ("Chord"),
                                                              midi::ChordGenerator::kindNames(), 0));
    return layout;
}

void ChordProcessor::prepareToPlay (double, int)
{
    generator.reset();
}

void ChordProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    generator.process (midi, static_cast<midi::ChordGenerator::Kind> (intOf (kind)));
}

ui::PanelDescription ChordProcessor::describePanel()
{
    return { .title = info().name,
             .sections = { { .title = TRANS ("Chord"),
                             .controls = { ui::knob ("chord", TRANS ("A chord above every played note")) } } } };
}

// ---------------------------------------------------------------------------------------------
// Arpeggiator

ArpeggiatorProcessor::ArpeggiatorProcessor()
    : InternalPlugin (info(), createLayout())
    , rate (parameter ("rate"))
    , mode (parameter ("mode"))
    , octaves (parameter ("octaves"))
    , gate (parameter ("gate"))
{
}

InternalPlugin::Info ArpeggiatorProcessor::info()
{
    return { .name = "FK Arpeggiator", .category = midiCategory, .uniqueId = 0x464b0203, .isMidiEffect = true };
}

double ArpeggiatorProcessor::rateBeats (int choiceIndex)
{
    return rates[static_cast<std::size_t> (std::clamp (choiceIndex, 0, static_cast<int> (rates.size()) - 1))].second;
}

juce::AudioProcessorValueTreeState::ParameterLayout ArpeggiatorProcessor::createLayout()
{
    juce::StringArray rateNames;

    for (const auto& [name, beats] : rates)
        rateNames.add (juce::translate (name));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, TRANS ("Rate"), rateNames, 3));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, TRANS ("Direction"),
                                                              midi::Arpeggiator::modeNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "octaves", 1 }, TRANS ("Octaves"), 1, 4, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gate", 1 }, TRANS ("Gate"),
                                                             juce::NormalisableRange<float> (5.0f, 100.0f, 1.0f), 50.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("%")));
    return layout;
}

void ArpeggiatorProcessor::prepareToPlay (double sampleRate, int)
{
    arpeggiator.prepare (sampleRate);
}

void ArpeggiatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    double bpm = 120.0;
    std::optional<double> ppq;

    // Tempo und Songposition vom Host; ohne laufende Wiedergabe die eigene Uhr.
    if (auto* host = getPlayHead())
        if (const auto position = host->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (const auto hostPpq = position->getPpqPosition(); position->getIsPlaying() && hostPpq)
                ppq = *hostPpq;
        }

    midi::Arpeggiator::Settings settings;
    settings.rateBeats = rateBeats (intOf (rate));
    settings.mode = static_cast<midi::Arpeggiator::Mode> (intOf (mode));
    settings.octaves = intOf (octaves);
    settings.gate = gate.load() / 100.0;
    arpeggiator.process (midi, buffer.getNumSamples(), bpm, ppq, settings);
}

ui::PanelDescription ArpeggiatorProcessor::describePanel()
{
    return { .title = info().name,
             .sections = { { .title = TRANS ("Arpeggio"),
                             .controls = { ui::knob ("rate", TRANS ("Note value of the steps, in the project tempo")),
                                           ui::knob ("mode", TRANS ("Order of the held notes")),
                                           ui::knob ("octaves", TRANS ("Over how many octaves")),
                                           ui::knob ("gate", TRANS ("How long each note sounds, in % of the step")) } } } };
}

} // namespace fk::plugins
