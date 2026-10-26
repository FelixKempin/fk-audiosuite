#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <span>

namespace fk::dsp::filters
{

/// Filter erster Ordnung (6 dB/Oktave), ein Kanal. Das kleinste Filter, das es gibt: ein
/// Koppelkondensator (Hochpass) oder eine Miller-Kapazität (Tiefpass) in einer Röhrenstufe
/// ist genau das. Für steilere Flanken den Biquad nehmen.
///
/// Echtzeit: processSample() allokiert nicht. setCutoff() rechnet einen Tangens; pro Block
/// oder in prepare() aufrufen, nicht pro Sample.
class OnePole final
{
public:
    enum class Mode
    {
        lowPass,
        highPass
    };

    void prepare (double sampleRate, Mode newMode, double cutoffHz) noexcept
    {
        assert (sampleRate > 0.0);
        currentSampleRate = sampleRate;
        mode = newMode;
        setCutoff (cutoffHz);
        reset();
    }

    void reset() noexcept { state = 0.0f; }

    /// cutoffHz wird auf [1 Hz, 0,49 * Samplerate] begrenzt.
    void setCutoff (double cutoffHz) noexcept
    {
        const double limited = std::fmin (std::fmax (cutoffHz, 1.0), currentSampleRate * 0.49);
        // Bilinear-Transformation mit Vorverzerrung: Die Grenzfrequenz stimmt exakt.
        const double warped = std::tan (std::numbers::pi * limited / currentSampleRate);
        g = static_cast<float> (warped / (1.0 + warped));
    }

    [[nodiscard]] float processSample (float input) noexcept
    {
        // Topologie-erhaltender Integrator (Zavalishin): Tiefpass = Integratorausgang,
        // Hochpass = Eingang minus Tiefpass. Verträgt Frequenzwechsel ohne Knackser.
        const float v = (input - state) * g;
        const float lowPass = v + state;
        state = lowPass + v;
        return mode == Mode::lowPass ? lowPass : input - lowPass;
    }

    void process (std::span<float> buffer) noexcept
    {
        for (float& sample : buffer)
            sample = processSample (sample);
    }

private:
    double currentSampleRate = 44100.0;
    Mode mode = Mode::lowPass;
    float g = 0.0f;
    float state = 0.0f;
};

} // namespace fk::dsp::filters
