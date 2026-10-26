#pragma once

#include "fk/dsp/generators/SineOscillator.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace fk::engine
{

/// Testton für den Audio-Einstellungsdialog: prüft, ob die gewählten Ausgänge spielen. Verbindet den reinen
/// DSP-Baustein (dsp::generators::SineOscillator) mit der JUCE-Audio-Welt (juce::AudioSource).
///
/// Echtzeit: getNextAudioBlock() läuft im Audio-Thread. Die Einstellungen kommen vom
/// Message-Thread über std::atomic, ohne Lock (CODING_GUIDELINES.md §8).
/// → LEARNING.md §10
class TestToneSource final : public juce::AudioSource
{
public:
    TestToneSource() = default;

    /// Message-Thread: Ton ein- oder ausschalten.
    void setEnabled (bool shouldBeEnabled) noexcept;

    /// Message-Thread: Frequenz in Hz.
    void setFrequency (double frequencyHz) noexcept;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    static constexpr float outputGain = 0.2f; // Leise genug, dass niemand erschrickt.

    dsp::generators::SineOscillator oscillator;
    std::atomic<bool> enabled = false;
    std::atomic<double> frequencyHz = 440.0;
};

} // namespace fk::engine
