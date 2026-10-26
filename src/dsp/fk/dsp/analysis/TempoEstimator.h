#pragma once

#include <optional>
#include <span>

namespace fk::dsp::analysis
{

/// Schätzt das Tempo von Audio mit Rhythmus (Drums, Gitarrenriffs, Loops).
///
/// Verfahren in drei Schritten:
/// 1. Einsatzstärke: Energie in kurzen Blöcken (Hop 512 Samples), davon nur der Anstieg
///    (logarithmisch, positiv). Ein Schlag ist ein plötzlicher Energieanstieg.
/// 2. Autokorrelation der Einsatzstärke über alle Verzögerungen zwischen 60 und 200 BPM:
///    Wiederholt sich das Muster nach einer Schlagdauer, ist die Korrelation dort hoch.
/// 3. Gewichtung um 120 BPM (log-normal, breit), damit nicht das halbe oder doppelte Tempo
///    gewinnt, dazu parabolische Verfeinerung der besten Verzögerung.
///
/// Nicht für den Audio-Thread (läuft einmal über eine ganze Datei).
class TempoEstimator final
{
public:
    /// Mono-Samples (vorher heruntermischen). Liefert nullopt bei zu kurzem oder rhythmuslosem Material
    /// (mindestens vier Sekunden; die beste Korrelation muss sich deutlich vom Mittel abheben).
    [[nodiscard]] static std::optional<double> estimate (std::span<const float> samples, double sampleRate);

    static constexpr double minBpm = 60.0;
    static constexpr double maxBpm = 200.0;
};

} // namespace fk::dsp::analysis
