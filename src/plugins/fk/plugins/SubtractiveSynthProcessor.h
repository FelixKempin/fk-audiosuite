#pragma once

#include "fk/dsp/envelopes/Adsr.h"
#include "fk/dsp/filters/StateVariableFilter.h"
#include "fk/dsp/generators/Oscillator.h"
#include "fk/plugins/InternalPlugin.h"

#include <juce_dsp/juce_dsp.h>

#include <array>

namespace fk::plugins
{

/// Lehrbuch-Synthesizer: zwei Oszillatoren, Tiefpass mit Resonanz, je eine Hüllkurve
/// für Lautstärke und Filter, 16 Stimmen. Stimmenverwaltung (welche Taste → welche Stimme)
/// übernimmt juce::Synthesiser; der Klang pro Stimme steckt in SynthVoice.
class SubtractiveSynthProcessor final : public InternalPlugin
{
public:
    SubtractiveSynthProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /// Die Parameter, die jede Stimme pro Block liest. Ein Bündel von Atomics statt vieler Einzelzeiger.
    struct Parameters
    {
        std::atomic<float>* osc1Wave = nullptr;
        std::atomic<float>* osc2Wave = nullptr;
        std::atomic<float>* osc2Detune = nullptr;      ///< Cent
        std::atomic<float>* oscMix = nullptr;          ///< 0 = nur Osc 1, 1 = nur Osc 2
        std::atomic<float>* cutoff = nullptr;          ///< Hz
        std::atomic<float>* resonance = nullptr;       ///< 0..1
        std::atomic<float>* filterEnvAmount = nullptr; ///< Oktaven
        std::atomic<float>* ampAttack = nullptr;
        std::atomic<float>* ampDecay = nullptr;
        std::atomic<float>* ampSustain = nullptr;
        std::atomic<float>* ampRelease = nullptr;
        std::atomic<float>* filterAttack = nullptr;
        std::atomic<float>* filterDecay = nullptr;
        std::atomic<float>* filterSustain = nullptr;
        std::atomic<float>* filterRelease = nullptr;
        std::atomic<float>* masterGainDb = nullptr;
    };

private:
    static constexpr int numVoices = 16;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    Parameters synthParameters;
    juce::Synthesiser synthesiser;
    juce::dsp::Gain<float> masterGain;
};

/// Eine Stimme: erzeugt den Klang für genau eine gedrückte Taste.
/// Echtzeit: renderNextBlock() läuft im Audio-Thread.
class SynthVoice final : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const SubtractiveSynthProcessor::Parameters& parameters);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber,
                    float velocity,
                    juce::SynthesiserSound* sound,
                    int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void setCurrentPlaybackSampleRate (double newRate) override;
    using juce::SynthesiserVoice::renderNextBlock; // double-Variante nicht verdecken
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    void applyParameters();

    const SubtractiveSynthProcessor::Parameters& parameters;
    dsp::generators::Oscillator oscillator1;
    dsp::generators::Oscillator oscillator2;
    dsp::filters::StateVariableFilter filter;
    dsp::envelopes::Adsr ampEnvelope;
    dsp::envelopes::Adsr filterEnvelope;
    double noteFrequencyHz = 440.0;
    float noteVelocity = 1.0f;

    // Spielhilfen: Pitch-Bend verschiebt die Tonhöhe um bis zu ±2 Halbtöne, das Mod-Wheel (CC1)
    // steuert die Tiefe eines Vibratos (bis ±50 Cent bei 5,5 Hz). Beide kommen vom Message-Thread
    // (Synthesiser ruft sie innerhalb von renderNextBlock) und werden nur hier gelesen.
    static constexpr double bendRangeSemitones = 2.0;
    static constexpr double vibratoRateHz = 5.5;
    static constexpr double vibratoDepthCents = 50.0;
    double bendSemitones = 0.0;
    double vibratoDepth = 0.0; // 0..1 vom Mod-Wheel
    double vibratoPhase = 0.0; // 0..1
};

/// JUCE verlangt ein "Sound"-Objekt; wir haben nur einen Klang, der auf allen Tasten spielt.
class SynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

} // namespace fk::plugins
