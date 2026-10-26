#pragma once

#include "fk/dsp/dynamics/EnvelopeFollower.h"
#include "fk/dsp/dynamics/GainComputer.h"

#include <span>

namespace fk::dsp::dynamics
{

/// Stereo-Kompressor (Feed-Forward, Spitzenwert-Detektor, stereo gekoppelt):
/// Beide Kanäle werden mit derselben Absenkung bearbeitet, damit das Stereobild stabil bleibt.
///
/// Aufbau: Pegel des lauteren Kanals in dB → GainComputer (Kennlinie) → EnvelopeFollower
/// (Attack/Release auf der Absenkung) → Verstärkung anwenden, Makeup dazu.
///
/// Echtzeit: process() allokiert nicht (CODING_GUIDELINES.md §8).
class Compressor final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setThresholdDb (float thresholdDb) noexcept;
    void setRatio (float ratio) noexcept;
    void setKneeDb (float kneeDb) noexcept;
    void setAttackMs (double attackMs) noexcept;
    void setReleaseMs (double releaseMs) noexcept;
    void setMakeupGainDb (float makeupDb) noexcept;

    /// Bearbeitet beide Kanäle an Ort und Stelle. Beide Puffer müssen gleich lang sein.
    void process (std::span<float> left, std::span<float> right) noexcept;

    /// Wie oben, aber der Pegel wird an detectLeft/detectRight gemessen (Sidechain): Eine Bassdrum
    /// auf der Sidechain duckt den Bass, ohne dass die Bassdrum selbst im Signal ist.
    void process (std::span<float> left,
                  std::span<float> right,
                  std::span<const float> detectLeft,
                  std::span<const float> detectRight) noexcept;

    /// Aktuelle Absenkung in dB (≤ 0), z. B. für eine Anzeige. Nach process() lesen.
    [[nodiscard]] float getCurrentGainReductionDb() const noexcept;

private:
    GainComputer gainComputer;
    EnvelopeFollower envelope;
    float makeupGainLinear = 1.0f;
    float currentReductionDb = 0.0f;
};

} // namespace fk::dsp::dynamics
