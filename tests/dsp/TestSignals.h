#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace fk::tests
{

/// Sinus mit gegebener Frequenz und Amplitude.
inline std::vector<float>
makeSine (double frequencyHz, double sampleRate, std::size_t numSamples, float amplitude = 1.0f)
{
    std::vector<float> samples (numSamples);

    for (std::size_t i = 0; i < numSamples; ++i)
        samples[i] =
            amplitude *
            static_cast<float> (std::sin (2.0 * std::numbers::pi * frequencyHz * static_cast<double> (i) / sampleRate));

    return samples;
}

/// Effektivwert (RMS) eines Puffers.
inline float rms (const std::vector<float>& samples)
{
    double sum = 0.0;

    for (const float sample : samples)
        sum += static_cast<double> (sample) * sample;

    return samples.empty() ? 0.0f : static_cast<float> (std::sqrt (sum / static_cast<double> (samples.size())));
}

/// Amplitude einer Frequenz im Puffer (eine DFT-Linie, Goertzel-artig). Für Oberton- und
/// Aliasing-Messungen: Frequenz so wählen, dass ganze Perioden in den Puffer passen.
inline float magnitudeAt (const std::vector<float>& samples, double frequencyHz, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;

    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const double phase = 2.0 * std::numbers::pi * frequencyHz * static_cast<double> (i) / sampleRate;
        real += static_cast<double> (samples[i]) * std::cos (phase);
        imaginary -= static_cast<double> (samples[i]) * std::sin (phase);
    }

    return static_cast<float> (2.0 * std::sqrt (real * real + imaginary * imaginary) /
                               static_cast<double> (samples.size()));
}

/// Verhältnis zweier Amplituden in dB.
inline float ratioDb (float numerator, float denominator)
{
    return 20.0f * std::log10 (numerator / denominator);
}

/// Größter Betrag im Puffer.
inline float peak (const std::vector<float>& samples)
{
    float result = 0.0f;

    for (const float sample : samples)
        result = std::max (result, std::abs (sample));

    return result;
}

} // namespace fk::tests
