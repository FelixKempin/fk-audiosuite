#pragma once

#include "fk/dsp/dynamics/EnvelopeFollower.h"

#include <span>

namespace fk::dsp::dynamics
{

/// Noise Gate mit Hysterese: öffnet über der Schwelle, schließt erst unter Schwelle minus
/// Hysterese, damit es an der Grenze nicht flattert. Hold hält es nach dem Unterschreiten noch
/// offen, Attack/Release glätten die Verstärkung. Vor einer Verzerrerstufe gesetzt, bekommt es
/// das saubere Signal (Sidechain vom Eingang) und schneidet Brummen in Spielpausen weg.
///
/// Echtzeit: process() allokiert nicht.
class NoiseGate final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setThresholdDb (float thresholdDb) noexcept;
    void setHysteresisDb (float hysteresisDb) noexcept;
    void setAttackMs (double attackMs) noexcept;
    void setHoldMs (double holdMs) noexcept;
    void setReleaseMs (double releaseMs) noexcept;

    /// Bearbeitet buffer; der Pegel wird aus sidechain gemessen (darf derselbe Puffer sein).
    void process (std::span<float> buffer, std::span<const float> sidechain) noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] float getCurrentGain() const noexcept;

private:
    double sampleRate = 44100.0;
    float openThreshold = 0.01f;   ///< linear
    float closeThreshold = 0.005f; ///< linear
    float thresholdDb = -40.0f;
    float hysteresisDb = 6.0f;
    double holdMs = 50.0;
    std::size_t holdSamples = 0;
    std::size_t holdCounter = 0;
    bool open = false;
    EnvelopeFollower detector;
    EnvelopeFollower gainSmoother;
};

} // namespace fk::dsp::dynamics
