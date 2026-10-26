#pragma once

namespace fk::dsp::dynamics
{

/// Statische Kennlinie eines Kompressors: Wie viel dB werden bei welchem Eingangspegel
/// abgezogen? Rein zustandslos, deshalb leicht zu testen. Mit weichem Knie (Soft Knee).
class GainComputer final
{
public:
    void setThresholdDb (float thresholdDb) noexcept;
    void setRatio (float ratio) noexcept;
    void setKneeDb (float kneeWidthDb) noexcept;

    /// Liefert die Pegelabsenkung in dB als nicht-positiven Wert (0 = keine Kompression).
    [[nodiscard]] float computeGainReductionDb (float inputLevelDb) const noexcept;

private:
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float kneeDb = 6.0f;
};

} // namespace fk::dsp::dynamics
