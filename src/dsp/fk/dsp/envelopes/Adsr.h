#pragma once

namespace fk::dsp::envelopes
{

/// Hüllkurve mit Attack, Decay, Sustain, Release. Werte 0..1. Linear in der Zeit;
/// gut genug für Lautstärke und Filter, später exponentiell.
///
/// Echtzeit: nextValue() allokiert nicht.
class Adsr final
{
public:
    /// Zustand der Hüllkurve. Öffentlich, damit Stimmen wissen, ob sie frei sind.
    enum class Stage
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    void prepare (double sampleRate) noexcept;

    void setAttackMs (double attackMs) noexcept;
    void setDecayMs (double decayMs) noexcept;
    void setSustainLevel (float level) noexcept;
    void setReleaseMs (double releaseMs) noexcept;

    void noteOn() noexcept;
    void noteOff() noexcept;
    void reset() noexcept;

    [[nodiscard]] float nextValue() noexcept;
    [[nodiscard]] Stage getStage() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;

private:
    [[nodiscard]] float rateForMs (double milliseconds) const noexcept;

    double sampleRate = 44100.0;
    double attackMs = 5.0;
    double decayMs = 50.0;
    float sustainLevel = 0.8f;
    double releaseMs = 100.0;

    Stage stage = Stage::idle;
    float value = 0.0f;
};

} // namespace fk::dsp::envelopes
