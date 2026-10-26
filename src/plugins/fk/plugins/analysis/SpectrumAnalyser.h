#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <span>
#include <vector>

namespace fk::plugins::analysis
{

/// Spektrum eines Signals für Anzeigen (SpectrumView). Zwei Seiten, ein Puffer:
///
/// - Audio-Thread: push() summiert den Block auf mono und schreibt ihn in eine FIFO
///   (juce::AbstractFifo, lock-frei, keine Allokation).
/// - Message-Thread: update() holt neue Samples aus der FIFO und rechnet, sobald fftSize Samples
///   da sind, eine FFT mit Hann-Fenster; getMagnitudesDb() ist das geglättete Ergebnis.
///
/// Beide Seiten teilen nur die FIFO und ihre Indizes; der Audio-Thread wartet nie (§10).
class SpectrumAnalyser final
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; ///< 2048 Samples, bei 48 kHz 23 Hz pro Bin
    static constexpr int numBins = fftSize / 2;

    SpectrumAnalyser();

    /// Message-Thread, vor dem ersten Block (prepareToPlay).
    void prepare (double sampleRate);

    /// Audio-Thread: summiert die Kanäle auf mono und legt sie ab. Ist die FIFO voll, gehen
    /// Samples verloren; für eine Anzeige ist das richtig, warten wäre falsch.
    void push (const juce::AudioBuffer<float>& buffer) noexcept;

    /// Message-Thread: verarbeitet neue Samples. Gibt true zurück, wenn sich das Spektrum geändert hat.
    bool update();

    /// Betrag je Bin in dB (0 = Vollaussteuerung), geglättet; Index i liegt bei getFrequencyForBin (i).
    [[nodiscard]] std::span<const float> getMagnitudesDb() const noexcept;

    [[nodiscard]] float getFrequencyForBin (int bin) const noexcept;

    [[nodiscard]] double getSampleRate() const noexcept;

private:
    static constexpr int fifoCapacity = fftSize * 4;
    static constexpr float minimumDb = -100.0f;
    static constexpr float decayDbPerUpdate = 2.0f;

    juce::AbstractFifo fifo { fifoCapacity };
    std::vector<float> fifoBuffer;   ///< fifoCapacity, nur über die FIFO-Indizes benutzt
    std::vector<float> pending;      ///< Message-Thread: Samples, die noch keine volle FFT ergeben
    std::vector<float> fftData;      ///< 2 * fftSize, Arbeitsspeicher der FFT
    std::vector<float> magnitudesDb; ///< numBins
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { static_cast<std::size_t> (fftSize),
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::atomic<double> sampleRate { 44100.0 };
};

} // namespace fk::plugins::analysis
