#pragma once

namespace fk::dsp::dynamics
{

/// Hüllkurvenfolger: glättet den Betrag eines Signals mit getrennten Zeiten für
/// Anstieg (Attack) und Abfall (Release). Herzstück von Kompressor und Limiter.
///
/// Echtzeit: processSample() allokiert nicht. setAttackMs/setReleaseMs rechnen eine
/// Exponentialfunktion und gehören in Parameter-Updates, nicht in jeden Sample.
class EnvelopeFollower final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setAttackMs (double attackMs) noexcept;
    void setReleaseMs (double releaseMs) noexcept;

    /// Erwartet einen nicht-negativen Wert (Betrag oder Pegel), liefert die geglättete Hüllkurve.
    [[nodiscard]] float processSample (float input) noexcept;

    [[nodiscard]] float getCurrentValue() const noexcept;

private:
    /// Koeffizient eines Einpol-Tiefpasses, der nach timeMs etwa 63 % des Zielwerts erreicht.
    [[nodiscard]] float coefficientForTime (double timeMs) const noexcept;

    double sampleRate = 44100.0;
    double attackMs = 10.0;
    double releaseMs = 100.0;
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float envelope = 0.0f;
};

} // namespace fk::dsp::dynamics
