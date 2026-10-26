#pragma once

#include <span>

namespace fk::dsp::filters
{

/// Zustandsvariablen-Filter (TPT/Zavalishin), 12 dB/Oktave, mit Resonanz.
/// Anders als der Biquad verträgt er schnelle Cutoff-Änderungen ohne Knackser,
/// deshalb der Filter der Wahl für Synthesizer mit Hüllkurven-Modulation.
///
/// Echtzeit: processSample() allokiert nicht. setCutoff() rechnet einen Tangens;
/// pro Block aufrufen, nicht pro Sample.
class StateVariableFilter final
{
public:
    enum class Mode
    {
        lowPass,
        bandPass,
        highPass
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setMode (Mode mode) noexcept;

    /// cutoffHz wird auf [20 Hz, 0,45 * Samplerate] begrenzt; resonance 0..1 (1 = kurz vor Selbstoszillation).
    void setCutoff (double cutoffHz, float resonance) noexcept;

    [[nodiscard]] float processSample (float input) noexcept;
    void process (std::span<float> buffer) noexcept;

private:
    double sampleRate = 44100.0;
    Mode mode = Mode::lowPass;
    float g = 0.0f; // vorgewärmte Frequenz
    float k = 1.0f; // Dämpfung = 2 - 2 * Resonanz
    float a1 = 0.0f;
    float a2 = 0.0f;
    float ic1eq = 0.0f; // Integratorzustände
    float ic2eq = 0.0f;
};

} // namespace fk::dsp::filters
