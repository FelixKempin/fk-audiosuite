#pragma once

#include "fk/dsp/delay/DelayLine.h"
#include "fk/dsp/delay/FractionalDelayLine.h"
#include "fk/dsp/filters/OnePole.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace fk::dsp::reverb
{

/// Algorithmischer Hall nach dem Prinzip "Feedback Delay Network" (Jot 1991).
///
/// Signalweg: Eingang (Mono-Summe) → Vorverzögerung → Low-Cut → vier Allpässe (Diffusion,
/// verschmiert Einschwinger zu einer dichten Wolke) → acht Verzögerungsleitungen, deren Ausgänge
/// über eine Householder-Matrix in alle Eingänge zurückgeführt werden. Die Matrix ist verlustfrei
/// (orthogonal), also bestimmt allein die Dämpfung je Leitung, wie schnell der Hall abklingt:
/// g = 10^(−3 · Länge / (RT60 · Samplerate)) ergibt genau −60 dB nach RT60 Sekunden.
/// Ein Tiefpass je Leitung lässt Höhen schneller verklingen (Luft und Wände schlucken Höhen).
/// Zwei Leitungen werden langsam moduliert; das bricht stehende Resonanzen ("metallischer Klang").
/// Links und rechts sind unterschiedliche Vorzeichen-Summen der Leitungen: dekorreliert, also breit.
///
/// Gibt nur den Hallanteil aus; das Mischen mit dem trockenen Signal macht das Plugin.
/// Echtzeit: prepare() allokiert, process() nicht.
class FdnReverb final
{
public:
    struct Settings
    {
        float preDelayMs = 20.0f;
        float decaySeconds = 2.0f; ///< RT60, 0,2 bis 20 s
        float size = 1.0f;         ///< Raumgröße als Faktor auf die Leitungslängen, 0,5 bis 1,5
        float dampingHz = 8000.0f; ///< Grenzfrequenz des Tiefpasses in der Schleife
        float lowCutHz = 80.0f;    ///< Hochpass am Eingang, hält Bassdröhnen aus dem Hall
        float diffusion = 0.7f;    ///< 0 = einzelne Echos hörbar, 1 = dichte Wolke
        float modulation = 0.3f;   ///< 0 bis 1
        float width = 1.0f;        ///< 0 = mono, 1 = volle Breite
    };

    static constexpr std::size_t numLines = 8;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const Settings& settings) noexcept;

    /// Liest inLeft/inRight, schreibt den Hallanteil nach outLeft/outRight (gleiche Länge, dürfen
    /// nicht mit den Eingängen überlappen).
    void process (std::span<const float> inLeft,
                  std::span<const float> inRight,
                  std::span<float> outLeft,
                  std::span<float> outRight) noexcept;

private:
    /// Schroeder-Allpass: alle Frequenzen gleich laut, aber zeitlich verschmiert.
    /// Eigener Ringpuffer, weil der Zustand vor dem Schreiben gelesen werden muss.
    struct Allpass
    {
        std::vector<float> buffer;
        std::size_t index = 0;
        void prepare (std::size_t delaySamples);
        void reset() noexcept;
        [[nodiscard]] float processSample (float input, float gain) noexcept;
    };

    double sampleRate = 48000.0;
    Settings current;
    delay::DelayLine preDelay;
    std::size_t preDelaySamples = 0;
    filters::OnePole lowCut;
    std::array<Allpass, 4> diffusers;
    std::array<delay::FractionalDelayLine, numLines> lines;
    std::array<float, numLines> lineLengths {};
    std::array<float, numLines> lineGains {};
    std::array<filters::OnePole, numLines> damping;
    double modulationPhase = 0.0;
    float modulationDepthSamples = 0.0f;
};

} // namespace fk::dsp::reverb
