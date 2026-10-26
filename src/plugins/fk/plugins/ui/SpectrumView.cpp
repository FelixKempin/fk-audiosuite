#include "fk/plugins/ui/SpectrumView.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

namespace
{
constexpr float gridLabelWidth = 34.0f;
constexpr float gridLabelHeight = 14.0f;
// Das Spektrum liegt unten im Bild: 0 dB des Signals bei maxDb der Skala, -spectrumSpanDb am Boden.
constexpr float spectrumSpanDb = 90.0f;
} // namespace

SpectrumView::SpectrumView (analysis::SpectrumAnalyser& spectrumAnalyser)
    : analyser (spectrumAnalyser)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void SpectrumView::setDecibelRange (float minDb, float maxDb)
{
    scale.minDb = minDb;
    scale.maxDb = maxDb;
    repaint();
}

FrequencyScale SpectrumView::getScale() const noexcept
{
    return scale;
}

void SpectrumView::resized()
{
    // Links Platz für dB-Beschriftung, unten für Hz-Beschriftung; derselbe Rahmen für alle Lagen.
    scale.bounds = getLocalBounds().toFloat().withTrimmedLeft (gridLabelWidth).withTrimmedBottom (gridLabelHeight);
    updateSpectrumPath();
}

void SpectrumView::timerCallback()
{
    if (analyser.update())
    {
        updateSpectrumPath();
        repaint();
    }
}

void SpectrumView::updateSpectrumPath()
{
    spectrumPath.clear();
    const auto magnitudes = analyser.getMagnitudesDb();

    if (magnitudes.empty() || scale.bounds.isEmpty())
        return;

    // Pro Pixelspalte den lautesten Bin nehmen: Bei 20 Hz liegen viele Pixel auf einem Bin,
    // bei 20 kHz viele Bins auf einem Pixel. So geht keine Spitze verloren.
    const auto bottom = scale.bounds.getBottom();
    spectrumPath.startNewSubPath (scale.bounds.getX(), bottom);

    const int numColumns = static_cast<int> (scale.bounds.getWidth());

    for (int column = 0; column <= numColumns; ++column)
    {
        const float x = scale.bounds.getX() + static_cast<float> (column);
        const float hzStart = scale.hzForX (x);
        const float hzEnd = scale.hzForX (x + 1.0f);
        const float binWidth = analyser.getFrequencyForBin (1);

        if (binWidth <= 0.0f)
            break;

        const int firstBin =
            juce::jlimit (1, analysis::SpectrumAnalyser::numBins - 1, static_cast<int> (hzStart / binWidth));
        const int lastBin =
            juce::jlimit (firstBin, analysis::SpectrumAnalyser::numBins - 1, static_cast<int> (hzEnd / binWidth));

        float loudest = -200.0f;
        for (int bin = firstBin; bin <= lastBin; ++bin)
            loudest = juce::jmax (loudest, magnitudes[static_cast<std::size_t> (bin)]);

        // Signalpegel (0 dB oben) auf die Skala legen: Ganz oben = Vollaussteuerung.
        const float proportion = juce::jlimit (0.0f, 1.0f, (loudest + spectrumSpanDb) / spectrumSpanDb);
        const float y = bottom - proportion * scale.bounds.getHeight();
        spectrumPath.lineTo (x, y);
    }

    spectrumPath.lineTo (scale.bounds.getRight(), bottom);
    spectrumPath.closeSubPath();
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.setColour (theme::backgroundInput);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::cornerRadius);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    // Frequenzraster: Dekaden kräftig, Zwischenwerte zart, Beschriftung unten.
    for (const float hz : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = scale.xForHz (hz);
        const bool major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
        g.setColour (major ? theme::gridBar : theme::gridBeat);
        g.drawVerticalLine (static_cast<int> (x), scale.bounds.getY(), scale.bounds.getBottom());

        g.setColour (theme::textSecondary);
        const auto label = hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String (hz, 0);
        // Beschriftung am Rand nach innen schieben, damit "20" und "20k" nicht abgeschnitten werden.
        const float labelX = juce::jlimit (0.0f, static_cast<float> (getWidth()) - 32.0f, x - 16.0f);
        g.drawText (label,
                    juce::Rectangle<float> (labelX, scale.bounds.getBottom(), 32.0f, gridLabelHeight),
                    juce::Justification::centred);
    }

    // dB-Raster: Null kräftig, Rest zart, Beschriftung links.
    const float step = (scale.maxDb - scale.minDb) > 60.0f ? 20.0f : 6.0f;
    for (float db = scale.minDb; db <= scale.maxDb + 0.01f; db += step)
    {
        const float y = scale.yForDb (db);
        g.setColour (std::abs (db) < 0.01f ? theme::gridBar : theme::gridBeat);
        g.drawHorizontalLine (static_cast<int> (y), scale.bounds.getX(), scale.bounds.getRight());
        g.setColour (theme::textSecondary);
        g.drawText (juce::String (db, 0),
                    juce::Rectangle<float> (0.0f, y - 7.0f, gridLabelWidth - 4.0f, gridLabelHeight),
                    juce::Justification::centredRight);
    }

    g.setColour (theme::accentSoft);
    g.fillPath (spectrumPath);
    g.setColour (theme::accent.withAlpha (0.5f));
    g.strokePath (spectrumPath, juce::PathStrokeType (1.0f));
}

} // namespace fk::plugins::ui
