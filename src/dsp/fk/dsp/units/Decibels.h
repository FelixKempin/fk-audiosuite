#pragma once

#include <cmath>

namespace fk::dsp::units
{

/// Unterhalb dieses Werts gilt ein Pegel als Stille (Verstärkung 0).
constexpr float minusInfinityDb = -100.0f;

/// Dezibel → linearer Faktor. 0 dB = 1, -6,02 dB = 0,5, +20 dB = 10.
[[nodiscard]] inline float decibelsToGain (float decibels) noexcept
{
    return decibels > minusInfinityDb ? std::pow (10.0f, decibels * 0.05f) : 0.0f;
}

/// Linearer Faktor → Dezibel. 0 und negative Werte ergeben minusInfinityDb.
[[nodiscard]] inline float gainToDecibels (float gain) noexcept
{
    return gain > 0.0f ? std::fmax (minusInfinityDb, 20.0f * std::log10 (gain)) : minusInfinityDb;
}

} // namespace fk::dsp::units
