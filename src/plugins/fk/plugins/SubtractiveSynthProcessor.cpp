#include "fk/plugins/SubtractiveSynthProcessor.h"

#include <cmath>

namespace fk::plugins
{

namespace
{
constexpr auto osc1WaveId = "osc1Wave";
constexpr auto osc2WaveId = "osc2Wave";
constexpr auto osc2DetuneId = "osc2Detune";
constexpr auto oscMixId = "oscMix";
constexpr auto cutoffId = "cutoff";
constexpr auto resonanceId = "resonance";
constexpr auto filterEnvAmountId = "filterEnvAmount";
constexpr auto ampAttackId = "ampAttack";
constexpr auto ampDecayId = "ampDecay";
constexpr auto ampSustainId = "ampSustain";
constexpr auto ampReleaseId = "ampRelease";
constexpr auto filterAttackId = "filterAttack";
constexpr auto filterDecayId = "filterDecay";
constexpr auto filterSustainId = "filterSustain";
constexpr auto filterReleaseId = "filterRelease";
constexpr auto masterGainId = "masterGain";

juce::StringArray waveformNames()
{
    return { TRANS ("Sine"), TRANS ("Saw"), TRANS ("Square"), TRANS ("Triangle") };
}

dsp::generators::Waveform waveformForChoice (float choice)
{
    switch (static_cast<int> (choice))
    {
        case 0:
            return dsp::generators::Waveform::sine;
        case 2:
            return dsp::generators::Waveform::square;
        case 3:
            return dsp::generators::Waveform::triangle;
        default:
            return dsp::generators::Waveform::saw;
    }
}

std::unique_ptr<juce::AudioParameterFloat> timeParameter (const char* id, const juce::String& name, float defaultMs)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                        name,
                                                        juce::NormalisableRange<float> (1.0f, 5000.0f, 1.0f, 0.3f),
                                                        defaultMs,
                                                        juce::AudioParameterFloatAttributes().withLabel ("ms"));
}
} // namespace

//==============================================================================
SubtractiveSynthProcessor::SubtractiveSynthProcessor()
    : InternalPlugin (info(), createLayout())
{
    synthParameters.osc1Wave = &parameter (osc1WaveId);
    synthParameters.osc2Wave = &parameter (osc2WaveId);
    synthParameters.osc2Detune = &parameter (osc2DetuneId);
    synthParameters.oscMix = &parameter (oscMixId);
    synthParameters.cutoff = &parameter (cutoffId);
    synthParameters.resonance = &parameter (resonanceId);
    synthParameters.filterEnvAmount = &parameter (filterEnvAmountId);
    synthParameters.ampAttack = &parameter (ampAttackId);
    synthParameters.ampDecay = &parameter (ampDecayId);
    synthParameters.ampSustain = &parameter (ampSustainId);
    synthParameters.ampRelease = &parameter (ampReleaseId);
    synthParameters.filterAttack = &parameter (filterAttackId);
    synthParameters.filterDecay = &parameter (filterDecayId);
    synthParameters.filterSustain = &parameter (filterSustainId);
    synthParameters.filterRelease = &parameter (filterReleaseId);
    synthParameters.masterGainDb = &parameter (masterGainId);

    // JUCE besitzt Stimmen und Klang über rohe Zeiger (Framework-Vertrag).
    for (int i = 0; i < numVoices; ++i)
        synthesiser.addVoice (new SynthVoice (synthParameters));

    synthesiser.addSound (new SynthSound());
}

InternalPlugin::Info SubtractiveSynthProcessor::info()
{
    return { "FK Synth", "Instrument", 0x464b0101, true };
}

juce::AudioProcessorValueTreeState::ParameterLayout SubtractiveSynthProcessor::createLayout()
{
    using Attributes = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { osc1WaveId, 1 }, TRANS ("Osc 1 Wave"), waveformNames(), 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { osc2WaveId, 1 }, TRANS ("Osc 2 Wave"), waveformNames(), 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { osc2DetuneId, 1 },
                                                             TRANS ("Osc 2 Detune"),
                                                             juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f),
                                                             7.0f,
                                                             Attributes().withLabel ("ct")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { oscMixId, 1 }, TRANS ("Osc Mix"), juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { cutoffId, 1 },
                                                     TRANS ("Cutoff"),
                                                     juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f),
                                                     2000.0f,
                                                     Attributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { resonanceId, 1 }, TRANS ("Resonance"), juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { filterEnvAmountId, 1 },
                                                             TRANS ("Filter Env"),
                                                             juce::NormalisableRange<float> (-4.0f, 4.0f, 0.01f),
                                                             2.0f,
                                                             Attributes().withLabel ("oct")));
    layout.add (timeParameter (ampAttackId, TRANS ("Amp Attack"), 5.0f));
    layout.add (timeParameter (ampDecayId, TRANS ("Amp Decay"), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ampSustainId, 1 },
                                                             TRANS ("Amp Sustain"),
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                                                             0.8f));
    layout.add (timeParameter (ampReleaseId, TRANS ("Amp Release"), 200.0f));
    layout.add (timeParameter (filterAttackId, TRANS ("Filter Attack"), 5.0f));
    layout.add (timeParameter (filterDecayId, TRANS ("Filter Decay"), 300.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { filterSustainId, 1 },
                                                             TRANS ("Filter Sustain"),
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                                                             0.3f));
    layout.add (timeParameter (filterReleaseId, TRANS ("Filter Release"), 300.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { masterGainId, 1 },
                                                             TRANS ("Master"),
                                                             juce::NormalisableRange<float> (-36.0f, 6.0f, 0.1f),
                                                             -9.0f,
                                                             Attributes().withLabel ("dB")));
    return layout;
}

void SubtractiveSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synthesiser.setCurrentPlaybackSampleRate (sampleRate);
    masterGain.prepare ({ sampleRate, static_cast<juce::uint32> (samplesPerBlock), 2 });
    masterGain.setRampDurationSeconds (0.02);
}

void SubtractiveSynthProcessor::releaseResources()
{
    synthesiser.allNotesOff (0, false);
}

void SubtractiveSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synthesiser.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    masterGain.setGainDecibels (synthParameters.masterGainDb->load());
    juce::dsp::AudioBlock<float> block (buffer);
    masterGain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

//==============================================================================
SynthVoice::SynthVoice (const SubtractiveSynthProcessor::Parameters& synthParameters)
    : parameters (synthParameters)
{
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    noteFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    noteVelocity = velocity;
    pitchWheelMoved (currentPitchWheelPosition); // ein Bend, der schon anliegt, gilt ab der ersten Note

    applyParameters();
    oscillator1.reset();
    oscillator2.reset();
    filter.reset();
    ampEnvelope.noteOn();
    filterEnvelope.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        filterEnvelope.reset();
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved (int newValue)
{
    // JUCE liefert 0..16383, Mitte 8192 = kein Bend.
    bendSemitones = (newValue - 8192) / 8192.0 * bendRangeSemitones;
}

void SynthVoice::controllerMoved (int controllerNumber, int newValue)
{
    if (controllerNumber == 1) // Mod-Wheel
        vibratoDepth = newValue / 127.0;
}

void SynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate > 0.0)
    {
        oscillator1.prepare (newRate);
        oscillator2.prepare (newRate);
        filter.prepare (newRate);
        ampEnvelope.prepare (newRate);
        filterEnvelope.prepare (newRate);
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! ampEnvelope.isActive())
        return;

    applyParameters();

    const float mix = parameters.oscMix->load();
    const float baseCutoff = parameters.cutoff->load();
    const float envOctaves = parameters.filterEnvAmount->load();
    const float resonance = parameters.resonance->load();

    for (int i = 0; i < numSamples; ++i)
    {
        const float osc = (1.0f - mix) * oscillator1.nextSample() + mix * oscillator2.nextSample();

        // Filterhüllkurve verschiebt den Cutoff in Oktaven: 2 Oktaven = Faktor 4.
        const float envValue = filterEnvelope.nextValue();
        const float cutoff = baseCutoff * std::pow (2.0f, envOctaves * envValue);

        if (i % 32 == 0) // Cutoff nicht für jedes Sample neu rechnen (tan ist teuer), 32 reicht hörbar
        {
            filter.setCutoff (cutoff, resonance);

            // Tonhöhe aus Bend und Vibrato, ebenfalls blockweise: 32 Samples sind bei 48 kHz 0,7 ms,
            // feiner hört niemand. Vibrato = Sinus, Tiefe vom Mod-Wheel.
            vibratoPhase += vibratoRateHz * 32.0 / getSampleRate();
            vibratoPhase -= std::floor (vibratoPhase);
            const double vibratoSemitones =
                vibratoDepth * vibratoDepthCents / 100.0 * std::sin (vibratoPhase * juce::MathConstants<double>::twoPi);
            const double pitchFactor = std::pow (2.0, (bendSemitones + vibratoSemitones) / 12.0);
            oscillator1.setFrequency (noteFrequencyHz * pitchFactor);
            oscillator2.setFrequency (noteFrequencyHz * pitchFactor *
                                      std::pow (2.0, parameters.osc2Detune->load() / 1200.0));
        }

        const float sample = filter.processSample (osc) * ampEnvelope.nextValue() * noteVelocity;

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample (channel, startSample + i, sample);
    }

    if (! ampEnvelope.isActive())
        clearCurrentNote();
}

void SynthVoice::applyParameters()
{
    oscillator1.setWaveform (waveformForChoice (parameters.osc1Wave->load()));
    oscillator2.setWaveform (waveformForChoice (parameters.osc2Wave->load()));
    oscillator1.setFrequency (noteFrequencyHz);
    oscillator2.setFrequency (noteFrequencyHz * std::pow (2.0, parameters.osc2Detune->load() / 1200.0));

    ampEnvelope.setAttackMs (parameters.ampAttack->load());
    ampEnvelope.setDecayMs (parameters.ampDecay->load());
    ampEnvelope.setSustainLevel (parameters.ampSustain->load());
    ampEnvelope.setReleaseMs (parameters.ampRelease->load());

    filterEnvelope.setAttackMs (parameters.filterAttack->load());
    filterEnvelope.setDecayMs (parameters.filterDecay->load());
    filterEnvelope.setSustainLevel (parameters.filterSustain->load());
    filterEnvelope.setReleaseMs (parameters.filterRelease->load());
}

ui::PanelDescription SubtractiveSynthProcessor::describePanel()
{
    return { .title = info().name,
             .sections = {
                 { .title = TRANS ("Oscillators"),
                   .controls = { ui::knob (osc1WaveId),
                                 ui::knob (osc2WaveId),
                                 ui::knob (osc2DetuneId),
                                 ui::knob (oscMixId) } },
                 { .title = TRANS ("Filter"),
                   .controls = { ui::knob (cutoffId), ui::knob (resonanceId), ui::knob (filterEnvAmountId) } },
                 { .title = TRANS ("Filter envelope"),
                   .controls = { ui::knob (filterAttackId),
                                 ui::knob (filterDecayId),
                                 ui::knob (filterSustainId),
                                 ui::knob (filterReleaseId) } },
                 { .title = TRANS ("Amp envelope"),
                   .controls = { ui::knob (ampAttackId),
                                 ui::knob (ampDecayId),
                                 ui::knob (ampSustainId),
                                 ui::knob (ampReleaseId) } },
                 { .title = TRANS ("Output"), .controls = { ui::knob (masterGainId) } } } };
}

} // namespace fk::plugins
