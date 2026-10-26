#include "fk/ui/ClipView.h"

#include "fk/ui/Theme.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::ui
{

ClipView::ClipView (services::WaveformService& waveformService, services::ClipInfo clipInfo, juce::Colour trackColour)
    : waveforms (waveformService)
    , info (std::move (clipInfo))
    , colour (trackColour)
    , thumbnail (info.isMidi ? nullptr : waveformService.createThumbnail (info.audioFile, *this))
{
    setInterceptsMouseClicks (false, false);
}

const services::ClipInfo& ClipView::getClipInfo() const noexcept
{
    return info;
}

void ClipView::setClipInfo (services::ClipInfo newInfo)
{
    const bool sourceChanged = newInfo.isMidi != info.isMidi || newInfo.audioFile != info.audioFile;
    info = std::move (newInfo);

    if (sourceChanged)
        thumbnail = info.isMidi ? nullptr : waveforms.createThumbnail (info.audioFile, *this);

    repaint();
}

int ClipView::xForSeconds (double secondsFromClipStart) const
{
    if (info.lengthSeconds <= 0.0)
        return 0;

    return static_cast<int> (secondsFromClipStart / info.lengthSeconds * getWidth());
}

juce::Rectangle<int> ClipView::getFadeInHandle() const
{
    if (info.isMidi)
        return {};

    return { xForSeconds (info.fadeInSeconds) - handleSize / 2, 0, handleSize, handleSize };
}

juce::Rectangle<int> ClipView::getFadeOutHandle() const
{
    if (info.isMidi)
        return {};

    return { xForSeconds (info.lengthSeconds - info.fadeOutSeconds) - handleSize / 2, 0, handleSize, handleSize };
}

void ClipView::setPreviewFades (double fadeInSeconds, double fadeOutSeconds)
{
    info.fadeInSeconds = fadeInSeconds;
    info.fadeOutSeconds = fadeOutSeconds;
    repaint();
}

void ClipView::setPreviewPosition (double startSeconds, double lengthSeconds, double offsetSeconds)
{
    info.startSeconds = startSeconds;
    info.lengthSeconds = lengthSeconds;
    info.offsetSeconds = offsetSeconds;
    repaint();
}

void ClipView::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

bool ClipView::isSelected() const noexcept
{
    return selected;
}

void ClipView::paint (juce::Graphics& g)
{
    constexpr float cornerRadius = 6.0f;
    constexpr int titleHeight = 16;

    const auto bounds = getLocalBounds().toFloat();

    // Stumme Clips grau: sichtbar, aber eindeutig "spielt nicht".
    const auto base = info.muted ? theme::textDisabled : colour;
    g.setColour (base.withAlpha (info.muted ? 0.35f : 0.7f));
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (base.darker (0.4f));
    g.fillRect (bounds.withHeight (static_cast<float> (titleHeight)));

    g.setColour (theme::textPrimary);
    g.setFont (juce::FontOptions (11.0f));
    // Mit Takes steht der aktuelle im Titel ("Gesang · Take 2/4"), damit man die Wahl sieht.
    auto title = (info.muted ? TRANS ("[muted] ") : juce::String()) +
                 (info.numTakes > 1 ? plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{name} · Take {take}/{takes}")),
                                                             { { "name", info.name },
                                                               { "take", juce::String (info.currentTake + 1) },
                                                               { "takes", juce::String (info.numTakes) } })
                                    : info.name);

    // Time-Stretch auf einen Blick: folgt dem Tempo, Geschwindigkeit, Tonhöhe.
    if (info.followsTempo)
        title << juce::String::fromUTF8 (" · ")
              << plugins::i18n::format (TRANS ("Tempo {bpm} BPM"), { { "bpm", juce::String (juce::roundToInt (info.sourceBpm)) } });
    else if (std::abs (info.speedRatio - 1.0) > 0.001)
        title << juce::String::fromUTF8 (" · ")
              << plugins::i18n::format (TRANS ("{percent} % length"),
                                        { { "percent", juce::String (juce::roundToInt (100.0 / info.speedRatio)) } });

    if (std::abs (info.pitchSemitones) > 0.01f)
        title << juce::String::fromUTF8 (" · ")
              << plugins::i18n::format (TRANS ("{value} st"),
                                        { { "value", (info.pitchSemitones > 0.0f ? "+" : "") + juce::String (info.pitchSemitones, 1) } });
    g.drawText (title,
                getLocalBounds().withHeight (titleHeight).reduced (theme::spacingSmall, 0),
                juce::Justification::centredLeft,
                true);

    if (info.isMidi && ! info.notes.empty())
    {
        // Mini-Pianorolle: Tonhöhe von unten nach oben, Zeit von links nach rechts.
        const auto area = getLocalBounds().withTrimmedTop (titleHeight).reduced (1).toFloat();
        int lowest = 127, highest = 0;

        for (const auto& note : info.notes)
        {
            lowest = juce::jmin (lowest, note.noteNumber);
            highest = juce::jmax (highest, note.noteNumber);
        }

        const float noteHeight = area.getHeight() / static_cast<float> (juce::jmax (12, highest - lowest + 1));
        g.setColour (theme::textPrimary.withAlpha (0.85f));

        for (const auto& note : info.notes)
        {
            const float x =
                area.getX() +
                static_cast<float> ((note.startSeconds - info.startSeconds) / info.lengthSeconds) * area.getWidth();
            const float width =
                juce::jmax (2.0f, static_cast<float> (note.lengthSeconds / info.lengthSeconds) * area.getWidth());
            const float y = area.getBottom() - static_cast<float> (note.noteNumber - lowest + 1) * noteHeight;
            g.fillRect (x, y, width, juce::jmax (1.0f, noteHeight - 1.0f));
        }
    }
    else if (thumbnail != nullptr && thumbnail->getTotalLength() > 0.0)
    {
        const auto waveformArea = getLocalBounds().withTrimmedTop (titleHeight).reduced (1);
        g.setColour (theme::backgroundBase.withAlpha (0.85f));
        thumbnail->drawChannels (g, waveformArea, info.offsetSeconds, info.offsetSeconds + info.lengthSeconds, 1.0f);
    }

    if (! info.isMidi)
    {
        // Fades als Rampen von der oberen Ecke zum Fade-Ende unten, dazu Griffe oben zum Ziehen.
        const auto fadeInX = static_cast<float> (xForSeconds (info.fadeInSeconds));
        const auto fadeOutX = static_cast<float> (xForSeconds (info.lengthSeconds - info.fadeOutSeconds));
        const auto bottom = bounds.getBottom();
        g.setColour (theme::backgroundBase.withAlpha (0.45f));

        if (info.fadeInSeconds > 0.0)
        {
            juce::Path ramp;
            ramp.addTriangle (0.0f, 0.0f, fadeInX, 0.0f, 0.0f, bottom);
            g.fillPath (ramp);
        }

        if (info.fadeOutSeconds > 0.0)
        {
            juce::Path ramp;
            ramp.addTriangle (fadeOutX, 0.0f, bounds.getRight(), 0.0f, bounds.getRight(), bottom);
            g.fillPath (ramp);
        }

        g.setColour (theme::textPrimary);
        g.fillRect (getFadeInHandle().toFloat());
        g.fillRect (getFadeOutHandle().toFloat());

        if (std::abs (info.gainDb) > 0.05f)
        {
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ((info.gainDb > 0.0f ? "+" : "") + juce::String (info.gainDb, 1) + " dB",
                        getLocalBounds().withHeight (titleHeight).reduced (theme::spacingSmall, 0),
                        juce::Justification::centredRight,
                        false);
        }
    }

    if (selected)
    {
        g.setColour (theme::textPrimary);
        g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius, 2.0f);
    }
    else
    {
        g.setColour (colour.brighter (0.2f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);
    }
}

} // namespace fk::ui
