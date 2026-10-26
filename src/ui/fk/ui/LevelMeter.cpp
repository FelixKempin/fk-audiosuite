#include "fk/ui/LevelMeter.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

void LevelMeter::setLevels (float leftDb, float rightDb)
{
    auto update = [] (Channel& channel, float db)
    {
        channel.levelDb = db;
        channel.peakDb = db > channel.peakDb ? db : channel.peakDb - holdDecayDbPerFrame;
    };

    update (left, leftDb);
    update (right, rightDb);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (theme::backgroundInput);
    g.fillRoundedRectangle (area, 2.0f);

    const float gap = 2.0f;
    const float channelWidth = (area.getWidth() - gap) * 0.5f;
    drawChannel (g, area.removeFromLeft (channelWidth).reduced (0.0f, 1.0f), left);
    area.removeFromLeft (gap);
    drawChannel (g, area.reduced (0.0f, 1.0f), right);
}

void LevelMeter::drawChannel (juce::Graphics& g, juce::Rectangle<float> area, const Channel& channel) const
{
    const float height = area.getHeight();
    const float levelHeight = height * proportionForDb (channel.levelDb);

    // Drei Zonen von unten nach oben: grün bis -18 dB, gelb bis -6 dB, orange bis 0, darüber rot.
    const float greenTop = height * proportionForDb (-18.0f);
    const float yellowTop = height * proportionForDb (-6.0f);

    auto fillZone = [&] (float from, float to, juce::Colour colour)
    {
        const float zoneTop = juce::jmin (to, levelHeight);

        if (zoneTop > from)
        {
            g.setColour (colour);
            g.fillRect (area.getX(), area.getBottom() - zoneTop, area.getWidth(), zoneTop - from);
        }
    };

    fillZone (0.0f, greenTop, theme::meterLow);
    fillZone (greenTop, yellowTop, theme::meterMid);
    fillZone (yellowTop, height, theme::meterHigh);

    if (channel.peakDb > floorDb)
    {
        const float peakY = area.getBottom() - height * proportionForDb (channel.peakDb);
        g.setColour (channel.peakDb >= 0.0f ? theme::meterClip : theme::textPrimary);
        g.fillRect (area.getX(), peakY - 1.0f, area.getWidth(), 2.0f);
    }
}

float LevelMeter::proportionForDb (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
}

} // namespace fk::ui
