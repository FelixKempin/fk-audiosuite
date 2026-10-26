#pragma once

#include <span>

namespace fk::dsp::generators
{

/// Sinus-Oszillator mit Phasenakkumulator. Ein Kanal; für Stereo zwei Instanzen oder Kopie.
///
/// Echtzeit: nextSample() und process() laufen im Audio-Thread und allokieren nicht,
/// siehe CODING_GUIDELINES.md §8. prepare() gehört in prepareToPlay() des Aufrufers.
/// → LEARNING.md §6 (Klassen) und §10 (Echtzeit-Audio)
class SineOscillator final
{
public:
    /// Muss vor der ersten Verwendung und nach jeder Änderung der Samplerate aufgerufen werden.
    void prepare (double sampleRate) noexcept;

    /// Setzt die Frequenz in Hz. Die Phase bleibt stetig, deshalb klickfrei.
    void setFrequency (double frequencyHz) noexcept;
    [[nodiscard]] double getFrequency() const noexcept;

    /// Setzt die Phase auf 0. Nur bei Stille aufrufen, sonst knackt es.
    void reset() noexcept;

    /// Liefert das nächste Sample im Bereich [-1, 1] und rückt die Phase weiter.
    [[nodiscard]] float nextSample() noexcept;

    /// Füllt den Puffer vollständig mit Samples (überschreibt, mischt nicht).
    void process (std::span<float> output) noexcept;

private:
    void updatePhaseIncrement() noexcept;

    double sampleRate = 44100.0;
    double frequencyHz = 440.0;

    // Phase in Umdrehungen [0, 1) statt Radiant: Der Überlauf ist dann ein einfaches "minus 1".
    double phase = 0.0;
    double phaseIncrement = 0.0;
};

} // namespace fk::dsp::generators
