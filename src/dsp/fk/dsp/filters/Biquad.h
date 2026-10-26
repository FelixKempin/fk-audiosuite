#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <span>

namespace fk::dsp::filters
{

/// Filtercharakteristik eines Biquads.
enum class FilterType
{
    lowPass,
    highPass,
    bandPass,
    notch,
    peak,
    lowShelf,
    highShelf
};

/// Biquad-Filter zweiter Ordnung (Direct Form II transposed), ein Kanal.
/// Für Stereo zwei Instanzen mit denselben Koeffizienten.
///
/// Template über den Sample-Typ: Dieselbe Mathematik für float (Audio-Pfad) und double
/// (Tests, Offline-Berechnung). Alles steht im Header, weil der Compiler den Code pro
/// Typ neu erzeugt (→ LEARNING.md §7). Koeffizienten nach der "Audio EQ Cookbook" von
/// Robert Bristow-Johnson.
///
/// Echtzeit: processSample() und process() allokieren nicht (CODING_GUIDELINES.md §8).
/// makeCoefficients() rechnet mit pow/sin/cos und gehört deshalb in prepare/Parameter-Updates,
/// nicht in jeden Sample.
template <typename SampleType>
class Biquad final
{
public:
    /// Normalisierte Koeffizienten (a0 = 1). Reiner Datenträger.
    struct Coefficients
    {
        SampleType b0 = 1;
        SampleType b1 = 0;
        SampleType b2 = 0;
        SampleType a1 = 0;
        SampleType a2 = 0;
    };

    /// Berechnet Koeffizienten. gainDb wirkt nur bei peak, lowShelf, highShelf.
    [[nodiscard]] static Coefficients
    makeCoefficients (FilterType type, double sampleRate, double frequencyHz, double q, double gainDb = 0.0) noexcept
    {
        assert (sampleRate > 0.0 && frequencyHz > 0.0 && frequencyHz < sampleRate * 0.5 && q > 0.0);

        const double omega = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
        const double sinOmega = std::sin (omega);
        const double cosOmega = std::cos (omega);
        const double alpha = sinOmega / (2.0 * q);
        const double amplitude = std::pow (10.0, gainDb / 40.0); // Wurzel aus dem linearen Gain, wie im Cookbook

        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case FilterType::lowPass:
                b0 = (1.0 - cosOmega) * 0.5;
                b1 = 1.0 - cosOmega;
                b2 = b0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosOmega;
                a2 = 1.0 - alpha;
                break;

            case FilterType::highPass:
                b0 = (1.0 + cosOmega) * 0.5;
                b1 = -(1.0 + cosOmega);
                b2 = b0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosOmega;
                a2 = 1.0 - alpha;
                break;

            case FilterType::bandPass:
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosOmega;
                a2 = 1.0 - alpha;
                break;

            case FilterType::notch:
                b0 = 1.0;
                b1 = -2.0 * cosOmega;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosOmega;
                a2 = 1.0 - alpha;
                break;

            case FilterType::peak:
                b0 = 1.0 + alpha * amplitude;
                b1 = -2.0 * cosOmega;
                b2 = 1.0 - alpha * amplitude;
                a0 = 1.0 + alpha / amplitude;
                a1 = -2.0 * cosOmega;
                a2 = 1.0 - alpha / amplitude;
                break;

            case FilterType::lowShelf:
            {
                const double sqrtA2Alpha = 2.0 * std::sqrt (amplitude) * alpha;
                b0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosOmega + sqrtA2Alpha);
                b1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosOmega);
                b2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosOmega - sqrtA2Alpha);
                a0 = (amplitude + 1.0) + (amplitude - 1.0) * cosOmega + sqrtA2Alpha;
                a1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosOmega);
                a2 = (amplitude + 1.0) + (amplitude - 1.0) * cosOmega - sqrtA2Alpha;
                break;
            }

            case FilterType::highShelf:
            {
                const double sqrtA2Alpha = 2.0 * std::sqrt (amplitude) * alpha;
                b0 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosOmega + sqrtA2Alpha);
                b1 = -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosOmega);
                b2 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosOmega - sqrtA2Alpha);
                a0 = (amplitude + 1.0) - (amplitude - 1.0) * cosOmega + sqrtA2Alpha;
                a1 = 2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosOmega);
                a2 = (amplitude + 1.0) - (amplitude - 1.0) * cosOmega - sqrtA2Alpha;
                break;
            }
        }

        return { static_cast<SampleType> (b0 / a0),
                 static_cast<SampleType> (b1 / a0),
                 static_cast<SampleType> (b2 / a0),
                 static_cast<SampleType> (a1 / a0),
                 static_cast<SampleType> (a2 / a0) };
    }

    /// Betrag des Frequenzgangs in dB bei frequencyHz: |H(z)| auf dem Einheitskreis, z = e^(jω).
    /// Für Anzeigen (Kurve im EQ), nicht für den Audio-Pfad: rechnet mit sin/cos pro Aufruf.
    [[nodiscard]] static double
    magnitudeDb (const Coefficients& coefficients, double frequencyHz, double sampleRate) noexcept
    {
        assert (sampleRate > 0.0 && frequencyHz >= 0.0);

        const double omega = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
        const double cos1 = std::cos (omega);
        const double sin1 = std::sin (omega);
        const double cos2 = std::cos (2.0 * omega);
        const double sin2 = std::sin (2.0 * omega);

        // Zähler b0 + b1·z⁻¹ + b2·z⁻², Nenner 1 + a1·z⁻¹ + a2·z⁻², jeweils als Real- und Imaginärteil.
        const double b0 = static_cast<double> (coefficients.b0);
        const double b1 = static_cast<double> (coefficients.b1);
        const double b2 = static_cast<double> (coefficients.b2);
        const double a1 = static_cast<double> (coefficients.a1);
        const double a2 = static_cast<double> (coefficients.a2);

        const double numeratorReal = b0 + b1 * cos1 + b2 * cos2;
        const double numeratorImag = -(b1 * sin1 + b2 * sin2);
        const double denominatorReal = 1.0 + a1 * cos1 + a2 * cos2;
        const double denominatorImag = -(a1 * sin1 + a2 * sin2);

        const double numerator = numeratorReal * numeratorReal + numeratorImag * numeratorImag;
        const double denominator = denominatorReal * denominatorReal + denominatorImag * denominatorImag;

        if (denominator <= 0.0 || numerator <= 0.0)
            return -120.0;

        return 10.0 * std::log10 (numerator / denominator);
    }

    void setCoefficients (const Coefficients& newCoefficients) noexcept { coefficients = newCoefficients; }

    /// Löscht den Filterzustand. Nach Samplerate-Wechsel oder bei Stille aufrufen.
    void reset() noexcept
    {
        state1 = 0;
        state2 = 0;
    }

    [[nodiscard]] SampleType processSample (SampleType input) noexcept
    {
        const SampleType output = coefficients.b0 * input + state1;
        state1 = coefficients.b1 * input - coefficients.a1 * output + state2;
        state2 = coefficients.b2 * input - coefficients.a2 * output;
        return output;
    }

    /// Filtert den Puffer an Ort und Stelle.
    void process (std::span<SampleType> buffer) noexcept
    {
        for (SampleType& sample : buffer)
            sample = processSample (sample);
    }

private:
    Coefficients coefficients;
    SampleType state1 = 0;
    SampleType state2 = 0;
};

} // namespace fk::dsp::filters
