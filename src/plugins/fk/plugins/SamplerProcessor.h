#pragma once

#include "fk/core/Result.h"
#include "fk/plugins/InternalPlugin.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>

namespace sfz
{
class Sfizz;
}

namespace fk::plugins
{

/// SFZ-Sampler um sfizz (BSD-2, ADR 0002): lädt eine .sfz-Datei mit ihren Samples und spielt
/// sie polyphon. Die Datei steht im Plugin-Zustand (Projektdatei), nicht als Parameter.
///
/// Threads: loadSfz() läuft im Message-Thread und hält dabei renderLock; processBlock() versucht
/// den Lock nur (ScopedTryLock) und schweigt, solange geladen wird. So wartet der Audio-Thread nie.
class SamplerProcessor final : public InternalPlugin
{
public:
    SamplerProcessor();
    ~SamplerProcessor() override;

    [[nodiscard]] static Info info();
    [[nodiscard]] ui::PanelDescription describePanel() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /// Lädt eine SFZ-Datei (Message-Thread). Fehler: Datei fehlt oder enthält keine Regionen.
    [[nodiscard]] core::Result<void> loadSfz (const juce::File& file);

    /// Entlädt das Instrument; die Spur bleibt stumm, bis eine neue Datei geladen wird.
    void unload();

    [[nodiscard]] juce::File getSfzFile() const;
    [[nodiscard]] int getNumRegions() const noexcept;
    [[nodiscard]] int getNumActiveVoices() const noexcept;

    /// Eine Zeile für die Oberfläche: Dateiname, Regionen, Stimmen oder ein Hinweis.
    [[nodiscard]] juce::String getStatusText() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void chooseFile();
    void sendMidi (const juce::MidiBuffer& midi);

    std::unique_ptr<sfz::Sfizz> synth;
    juce::CriticalSection renderLock;
    std::atomic<float>& volumeDb;
    std::atomic<int> numRegions { 0 };
    juce::dsp::Gain<float> outputGain;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace fk::plugins
