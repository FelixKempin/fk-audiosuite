#pragma once

#include "fk/dsp/delay/FractionalDelayLine.h"
#include "fk/dsp/filters/OnePole.h"

#include <span>

namespace fk::dsp::delay
{

/// Stereo-Echo mit Rückkopplung, Filtern im Rückkopplungsweg und Ping-Pong.
///
/// Jede Wiederholung läuft durch Low-Cut und High-Cut: Wie bei einem Bandecho werden spätere
/// Echos dumpfer und dünner und vernebeln den Mix nicht. Ping-Pong führt die Rückkopplung über
/// Kreuz (links → rechts → links), das Echo springt zwischen den Seiten.
/// Die Verzögerung gleitet weich (Glättung ~50 ms) zum neuen Wert: Ändern während des Spielens
/// erzeugt einen kurzen Tonhöhen-Schleifer statt eines Knacks, wie bei analogen Geräten.
///
/// Gibt nur den Echo-Anteil aus. Echtzeit: prepare() allokiert, process() nicht.
class StereoDelay final
{
public:
    struct Settings
    {
        float delaySeconds = 0.375f;
        float feedback = 0.35f; ///< 0 bis 0,95
        bool pingPong = false;
        float lowCutHz = 120.0f;
        float highCutHz = 6000.0f;
    };

    static constexpr double maxDelaySeconds = 4.0;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const Settings& settings) noexcept;

    /// inLeft/inRight lesen, Echo nach outLeft/outRight schreiben (keine Überlappung).
    void process (std::span<const float> inLeft,
                  std::span<const float> inRight,
                  std::span<float> outLeft,
                  std::span<float> outRight) noexcept;

private:
    double sampleRate = 48000.0;
    Settings current;
    float targetDelaySamples = 1.0f;
    float smoothedDelaySamples = 1.0f;
    float smoothing = 0.0f; ///< Glättungsfaktor je Sample
    bool snapToTarget = true; ///< nach prepare()/reset(): ohne Gleiten auf die eingestellte Zeit
    FractionalDelayLine left;
    FractionalDelayLine right;
    filters::OnePole lowCutLeft, lowCutRight, highCutLeft, highCutRight;
};

} // namespace fk::dsp::delay
