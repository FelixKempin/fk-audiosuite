#pragma once

#include "fk/dsp/delay/DelayLine.h"
#include "fk/dsp/dynamics/EnvelopeFollower.h"

#include <span>
#include <vector>

namespace fk::dsp::dynamics
{

/// Brickwall-Limiter mit Vorausschau (Lookahead), stereo gekoppelt.
///
/// Das Signal wird um die Lookahead-Zeit verzögert; die Verstärkung wird aus dem größten
/// Spitzenwert im Vorausschau-Fenster berechnet. So kann der Limiter die Verstärkung
/// absenken, BEVOR die Spitze ankommt, und garantiert den Ceiling. Release glättet
/// das Zurückfahren der Absenkung.
///
/// Meldet seine Latenz (getLatencySamples), damit der Host sie ausgleicht (PDC).
/// Echtzeit: process() allokiert nicht; prepare() reserviert alles.
class Limiter final
{
public:
    void prepare (double sampleRate, double lookaheadMs);
    void reset() noexcept;

    void setCeilingDb (float ceilingDb) noexcept;
    void setReleaseMs (double releaseMs) noexcept;

    void process (std::span<float> left, std::span<float> right) noexcept;

    [[nodiscard]] std::size_t getLatencySamples() const noexcept;
    [[nodiscard]] float getCurrentGainReductionDb() const noexcept;

private:
    /// Größter Betrag im Vorausschau-Fenster. Einfache Suche; reicht für 5 ms bei 48 kHz.
    /// TODO(felix): Durch monotone Deque ersetzen, wenn das Profil es verlangt.
    [[nodiscard]] float peakInWindow() const noexcept;

    delay::DelayLine leftDelay;
    delay::DelayLine rightDelay;
    std::vector<float> window; // Beträge der letzten lookaheadSamples Eingänge
    std::size_t windowIndex = 0;
    std::size_t lookaheadSamples = 0;
    EnvelopeFollower releaseEnvelope;
    float ceilingLinear = 1.0f;
    float currentReductionDb = 0.0f;
};

} // namespace fk::dsp::dynamics
