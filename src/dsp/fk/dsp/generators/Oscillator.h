#pragma once

namespace fk::dsp::generators
{

/// Wellenform eines Oszillators.
enum class Waveform
{
    sine,
    saw,
    square,
    triangle
};

/// Bandbegrenzter Oszillator mit PolyBLEP: Sägezahn und Rechteck werden an ihren Sprüngen
/// geglättet, damit keine Aliasing-Obertöne über der halben Abtastrate entstehen.
///
/// Echtzeit: nextSample() allokiert nicht.
class Oscillator final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setWaveform (Waveform waveform) noexcept;
    void setFrequency (double frequencyHz) noexcept;

    [[nodiscard]] float nextSample() noexcept;

private:
    /// Korrektur um einen Sprung herum (Polynomial Band-Limited Step), in Phasen-Einheiten [0, 1).
    [[nodiscard]] float polyBlep (double t) const noexcept;

    double sampleRate = 44100.0;
    double phase = 0.0;          // in Umdrehungen [0, 1)
    double phaseIncrement = 0.0; // Umdrehungen pro Sample
    float triangleState = 0.0f;  // Dreieck = integriertes Rechteck
    Waveform waveform = Waveform::saw;
};

} // namespace fk::dsp::generators
