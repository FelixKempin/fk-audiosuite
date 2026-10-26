#pragma once

#include <cstddef>
#include <vector>

namespace fk::dsp::delay
{

/// Verzögerungsleitung mit gebrochener Verzögerung, ein Kanal. Gelesen wird mit
/// Lagrange-Interpolation dritter Ordnung über vier Nachbarsamples: genau für Polynome bis
/// Grad 3, bei 1 kHz und 48 kHz Fehler unter 0,1 %, und anders als ein Allpass ohne
/// Einschwingen, wenn sich die Verzögerung ändert. Das braucht der Wellenleiter: Vibrato,
/// Legato und Posaunenzug verschieben die Länge laufend.
///
/// Die Puffergröße ist eine Zweierpotenz, damit der Ringindex eine Maske statt einer Division ist.
///
/// Echtzeit: prepare() allokiert. push(), read() und reset() nicht.
class FractionalDelayLine final
{
public:
    /// Kleinste lesbare Verzögerung: Die Interpolation braucht ein Sample vor dem Lesepunkt.
    static constexpr float minimumDelay = 1.0f;

    /// Reserviert Platz für Verzögerungen bis maximumDelaySamples. Nicht im Audio-Thread aufrufen.
    void prepare (std::size_t maximumDelaySamples);
    void reset() noexcept;

    /// Schreibt das nächste Sample. Danach liefert read (0) es zurück (ohne Interpolation).
    void push (float input) noexcept;

    /// Das Sample von vor delaySamples Takten, zwischen den Takten interpoliert. delaySamples
    /// wird auf [minimumDelay, maximumDelaySamples] begrenzt.
    [[nodiscard]] float read (float delaySamples) const noexcept;

    [[nodiscard]] float getMaximumDelay() const noexcept;

private:
    [[nodiscard]] float at (std::size_t delay) const noexcept;

    std::vector<float> buffer;
    std::size_t mask = 0;
    std::size_t writeIndex = 0;
    float maximumDelay = minimumDelay;
};

} // namespace fk::dsp::delay
