#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cmath>

namespace fk::plugins::ui
{

/// Abbildung zwischen Frequenz/Pegel und Pixeln für alle Frequenzansichten (Spektrum, Kurve,
/// Griffe): x logarithmisch über 20 Hz bis 20 kHz, y linear in dB. Reiner Wert ohne Zustand,
/// damit drei übereinanderliegende Komponenten exakt dieselbe Abbildung benutzen.
struct FrequencyScale
{
    juce::Rectangle<float> bounds;
    float minHz = 20.0f;
    float maxHz = 20000.0f;
    float minDb = -24.0f;
    float maxDb = 24.0f;

    [[nodiscard]] float xForHz (float hz) const noexcept
    {
        const auto proportion = std::log (hz / minHz) / std::log (maxHz / minHz);
        return bounds.getX() + proportion * bounds.getWidth();
    }

    [[nodiscard]] float hzForX (float x) const noexcept
    {
        const auto proportion = juce::jlimit (0.0f, 1.0f, (x - bounds.getX()) / bounds.getWidth());
        return minHz * std::pow (maxHz / minHz, proportion);
    }

    [[nodiscard]] float yForDb (float db) const noexcept
    {
        const auto proportion = (db - minDb) / (maxDb - minDb);
        return bounds.getBottom() - proportion * bounds.getHeight();
    }

    [[nodiscard]] float dbForY (float y) const noexcept
    {
        const auto proportion = juce::jlimit (0.0f, 1.0f, (bounds.getBottom() - y) / bounds.getHeight());
        return minDb + proportion * (maxDb - minDb);
    }
};

} // namespace fk::plugins::ui
