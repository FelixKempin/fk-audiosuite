#pragma once

#include "fk/dsp/delay/StereoDelay.h"
#include "fk/plugins/InternalPlugin.h"

#include <vector>

namespace fk::plugins
{

/// Echo-Plugin um dsp::delay::StereoDelay, mit Tempo-Sync über die Wiedergabeposition des Hosts.
class DelayProcessor final : public InternalPlugin
{
public:
    DelayProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    double getTailLengthSeconds() const override;

    /// Notenwerte der Sync-Auswahl als Länge in Viertelnoten, gleiche Reihenfolge wie die Auswahl.
    [[nodiscard]] static double noteLengthInQuarters (int choiceIndex) noexcept;

    /// Aktuelle Verzögerung in Sekunden (für die Anzeige und Tests), nach Sync und Tempo.
    [[nodiscard]] float getDelaySeconds() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    [[nodiscard]] float computeDelaySeconds (double bpm) const noexcept;

    std::atomic<float>& sync;
    std::atomic<float>& timeMs;
    std::atomic<float>& note;
    std::atomic<float>& feedbackPercent;
    std::atomic<float>& pingPong;
    std::atomic<float>& lowCutHz;
    std::atomic<float>& highCutHz;
    std::atomic<float>& mixPercent;

    dsp::delay::StereoDelay delay;
    std::vector<float> wetLeft;
    std::vector<float> wetRight;
    std::atomic<float> currentDelaySeconds = 0.0f;
};

} // namespace fk::plugins
