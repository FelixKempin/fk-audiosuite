#pragma once

#include "fk/plugins/InternalPlugin.h"
#include "fk/plugins/midi/MidiEffects.h"

namespace fk::plugins
{

/// FK Transpose: MIDI-Insert vor dem Instrument, verschiebt Noten und Anschlag.
class TransposeProcessor final : public InternalPlugin
{
public:
    TransposeProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& semitones;
    std::atomic<float>& velocityOffset;
    midi::Transposer transposer;
};

/// FK Chord: eine Taste spielt einen Akkord (Dur, Moll, Sus4, Septakkorde, Power, Oktave).
class ChordProcessor final : public InternalPlugin
{
public:
    ChordProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& kind;
    midi::ChordGenerator generator;
};

/// FK Arpeggiator: gehaltene Noten nacheinander im Raster, synchron zum Projekt.
class ArpeggiatorProcessor final : public InternalPlugin
{
public:
    ArpeggiatorProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /// Notenwerte des Rate-Reglers in Schlägen (1/4 … 1/32, mit Triolen).
    [[nodiscard]] static double rateBeats (int choiceIndex);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& rate;
    std::atomic<float>& mode;
    std::atomic<float>& octaves;
    std::atomic<float>& gate;
    midi::Arpeggiator arpeggiator;
};

} // namespace fk::plugins
