#pragma once

#include "fk/dsp/reverb/FdnReverb.h"
#include "fk/plugins/InternalPlugin.h"

#include <vector>

namespace fk::plugins
{

/// Hall-Plugin um dsp::reverb::FdnReverb. Als Insert mit Mix-Regler, auf einer FX-Spur mit Mix 100 %.
class ReverbProcessor final : public InternalPlugin
{
public:
    ReverbProcessor();

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /// So lange klingt der Hall nach dem letzten Ton (für Export und Bake).
    double getTailLengthSeconds() const override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>& preDelayMs;
    std::atomic<float>& decaySeconds;
    std::atomic<float>& sizePercent;
    std::atomic<float>& dampingHz;
    std::atomic<float>& lowCutHz;
    std::atomic<float>& diffusionPercent;
    std::atomic<float>& modulationPercent;
    std::atomic<float>& widthPercent;
    std::atomic<float>& mixPercent;

    dsp::reverb::FdnReverb reverb;
    std::vector<float> wetLeft;
    std::vector<float> wetRight;
};

} // namespace fk::plugins
