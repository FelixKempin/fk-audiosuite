#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Zweikanalige Pegelanzeige mit kurzem Nachlauf (Peak Hold). Bekommt Werte in dB
/// von außen gesetzt; sie fragt selbst nichts ab, damit sie überall einsetzbar bleibt.
class LevelMeter final : public juce::Component
{
public:
    void setLevels (float leftDb, float rightDb);
    void paint (juce::Graphics& g) override;

private:
    static constexpr float floorDb = -60.0f;
    static constexpr float holdDecayDbPerFrame = 0.4f;

    /// Zustand eines Kanals: aktueller Pegel und gehaltene Spitze.
    struct Channel
    {
        float levelDb = -100.0f;
        float peakDb = -100.0f;
    };

    void drawChannel (juce::Graphics& g, juce::Rectangle<float> area, const Channel& channel) const;
    [[nodiscard]] static float proportionForDb (float db);

    Channel left;
    Channel right;
};

} // namespace fk::ui
