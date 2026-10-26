#include "fk/ui/SampleEditor.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <cmath>

namespace fk::ui
{

namespace
{
constexpr int toolbarHeight = 34;
constexpr int infoHeight = 22;

juce::String formatSeconds (double seconds)
{
    return juce::String (seconds, 3) + " s";
}
} // namespace

SampleEditor::SampleEditor (services::ClipService& clipService,
                            services::TrackService& trackService,
                            services::WaveformService& waveformService)
    : clips (clipService)
    , tracks (trackService)
    , waveforms (waveformService)
    , silenceButton (Icon::silence, TRANS ("Silence: mute a section (or the whole clip)"))
    , gainButton (Icon::gain, TRANS ("Amplify or attenuate (selection in dB)"))
    , normalizeButton (Icon::normalize, TRANS ("Normalize: set the peak of the section to 0 dBFS"))
    , fadeInButton (Icon::fadeIn, TRANS ("Fade in over the section"))
    , fadeOutButton (Icon::fadeOut, TRANS ("Fade out over the section"))
    , reverseButton (Icon::reverse, TRANS ("Reverse the section"))
    , phaseButton (Icon::phase, TRANS ("Invert phase"))
    , zoomInButton (Icon::plus, TRANS ("Zoom in (Ctrl+wheel)"))
    , zoomOutButton (Icon::minus, TRANS ("Zoom out (Ctrl+wheel)"))
    , zoomFitButton (Icon::zoomFit, TRANS ("Show the whole clip"))
{
    using P = services::ClipService::AudioProcess;
    silenceButton.onClick = [this] { apply (P::silence); };
    gainButton.onClick = [this] { showGainMenu(); };
    normalizeButton.onClick = [this] { apply (P::normalize); };
    fadeInButton.onClick = [this] { apply (P::fadeIn); };
    fadeOutButton.onClick = [this] { apply (P::fadeOut); };
    reverseButton.onClick = [this] { apply (P::reverse); };
    phaseButton.onClick = [this] { apply (P::invertPhase); };
    zoomInButton.onClick = [this] { zoom (0.5, visibleStart + visibleLength / 2.0); };
    zoomOutButton.onClick = [this] { zoom (2.0, visibleStart + visibleLength / 2.0); };
    zoomFitButton.onClick = [this] { zoomToFit(); };

    infoLabel.setFont (juce::FontOptions (12.0f));
    infoLabel.setColour (juce::Label::textColourId, theme::textSecondary);

    for (auto* component : std::initializer_list<juce::Component*> { &silenceButton,
                                                                     &gainButton,
                                                                     &normalizeButton,
                                                                     &fadeInButton,
                                                                     &fadeOutButton,
                                                                     &reverseButton,
                                                                     &phaseButton,
                                                                     &zoomInButton,
                                                                     &zoomOutButton,
                                                                     &zoomFitButton,
                                                                     &infoLabel })
        addAndMakeVisible (component);

    tracks.addListener (*this);
    refresh();
}

SampleEditor::~SampleEditor()
{
    tracks.removeListener (*this);
}

void SampleEditor::setClip (const juce::String& newTrackId, const juce::String& newClipId)
{
    trackId = newTrackId;
    clipId = newClipId;
    selection.reset();
    shownFile = juce::File(); // Vorschau neu anlegen
    refresh();
    zoomToFit();
}

std::optional<services::ClipInfo> SampleEditor::currentClip() const
{
    if (auto* track = tracks.findTrack (trackId))
        for (const auto& clip : tracks.getClips (*track))
            if (clip.id == clipId && ! clip.isMidi)
                return clip;

    return std::nullopt;
}

void SampleEditor::tracksChanged()
{
    refresh();
}

void SampleEditor::contentChanged()
{
    refresh();
}

void SampleEditor::refresh()
{
    info = currentClip();

    // Neue Datei (nach einer Bearbeitung oder anderem Clip): Vorschau neu aufbauen.
    if (! info)
    {
        thumbnail.reset();
        shownFile = juce::File();
    }
    else if (info->audioFile != shownFile)
    {
        shownFile = info->audioFile;
        thumbnail = waveforms.createThumbnail (shownFile, *this);
    }

    const bool enabled = info.has_value() && ! info->followsTempo && ! info->reversed;

    for (auto* button :
         { &silenceButton, &gainButton, &normalizeButton, &fadeInButton, &fadeOutButton, &reverseButton, &phaseButton })
        button->setEnabled (enabled);

    if (! info)
        infoLabel.setText ({}, juce::dontSendNotification);
    else
    {
        auto text = info->name + "  |  " + info->audioFile.getFileName() + "  |  " +
                    plugins::i18n::format (TRANS ("Length {value}"), { { "value", formatSeconds (info->lengthSeconds) } });

        if (selection)
            text << "  |  "
                 << plugins::i18n::format (TRANS ("Selection {from} to {to}"),
                                           { { "from", formatSeconds (selection->first) },
                                             { "to", formatSeconds (selection->second) } });
        else
            text << "  |  " << TRANS ("Drag to select a section; without a selection everything applies to the whole clip");

        if (info->followsTempo || info->reversed)
            text << "  |  " << TRANS ("Editing only works when the clip does not follow the tempo and plays forwards");

        infoLabel.setText (text, juce::dontSendNotification);
    }

    repaint();
}

void SampleEditor::apply (services::ClipService::AudioProcess process, float gainDb)
{
    if (! info)
        return;

    std::optional<services::ClipService::ClipRange> range;

    if (selection)
        range = services::ClipService::ClipRange { selection->first, selection->second };

    if (const auto result = clips.processAudio ({ clipId }, process, range, gainDb); ! result && onError)
        onError (services::errorText (result.error()));
}

void SampleEditor::showGainMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (selection ? TRANS ("Amplify section") : TRANS ("Amplify whole clip"));
    const float choices[] = { 6.0f, 3.0f, 1.0f, -1.0f, -3.0f, -6.0f, -12.0f };

    for (int i = 0; i < 7; ++i)
        menu.addItem (i + 1, (choices[i] > 0.0f ? "+" : "") + juce::String (choices[i], 0) + " dB");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (gainButton),
                        [safeThis = juce::Component::SafePointer (this), choices] (int chosen)
                        {
                            if (safeThis != nullptr && chosen > 0)
                                safeThis->apply (services::ClipService::AudioProcess::gain, choices[chosen - 1]);
                        });
}

juce::Rectangle<int> SampleEditor::waveformArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight).withTrimmedBottom (infoHeight).reduced (theme::spacingSmall);
}

double SampleEditor::secondsAtX (int x) const
{
    const auto area = waveformArea();
    return visibleStart + visibleLength * static_cast<double> (x - area.getX()) / juce::jmax (1, area.getWidth());
}

int SampleEditor::xForSeconds (double seconds) const
{
    const auto area = waveformArea();
    return area.getX() + static_cast<int> ((seconds - visibleStart) / visibleLength * area.getWidth());
}

void SampleEditor::zoom (double factor, double anchorSeconds)
{
    if (! info)
        return;

    const auto newLength = juce::jlimit (0.005, juce::jmax (0.01, info->lengthSeconds), visibleLength * factor);
    const auto anchorRatio = (anchorSeconds - visibleStart) / visibleLength;
    visibleLength = newLength;
    visibleStart = juce::jlimit (
        0.0, juce::jmax (0.0, info->lengthSeconds - visibleLength), anchorSeconds - anchorRatio * newLength);
    repaint();
}

void SampleEditor::zoomToFit()
{
    visibleStart = 0.0;
    visibleLength = info ? juce::jmax (0.01, info->lengthSeconds) : 1.0;
    repaint();
}

void SampleEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    const auto area = waveformArea();
    g.setColour (theme::backgroundInput);
    g.fillRect (area);

    if (! info)
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (TRANS ("Double-click an audio clip in the arrangement to edit it here."),
                    area,
                    juce::Justification::centred);
        return;
    }

    // Auswahl unter der Wellenform, damit die Kurve lesbar bleibt.
    if (selection)
    {
        const auto x1 = xForSeconds (selection->first);
        const auto x2 = xForSeconds (selection->second);
        g.setColour (theme::accent.withAlpha (0.25f));
        g.fillRect (
            juce::Rectangle<int> (x1, area.getY(), juce::jmax (1, x2 - x1), area.getHeight()).getIntersection (area));
    }

    if (thumbnail != nullptr && thumbnail->getTotalLength() > 0.0)
    {
        // Clip-Zeit in Material-Zeit: ab Versatz, mit der Geschwindigkeit des Clips.
        const auto from = info->offsetSeconds + visibleStart * info->speedRatio;
        const auto to = info->offsetSeconds + (visibleStart + visibleLength) * info->speedRatio;
        g.setColour (theme::accent);
        thumbnail->drawChannels (g, area, from, to, 1.0f);
    }

    // Zeitachse: grobe Sekundenmarken.
    g.setColour (theme::textSecondary);
    g.setFont (juce::FontOptions (10.0f));
    // Schritt 1, 2 oder 5 mal Zehnerpotenz, mindestens 60 Pixel zwischen zwei Marken.
    auto step = std::pow (10.0, std::floor (std::log10 (juce::jmax (0.001, visibleLength / 20.0))));

    for (const auto factor : { 2.0, 2.5, 2.0, 2.0, 2.5, 2.0, 2.0, 2.5, 2.0 })
        if (visibleLength / step * 60.0 > area.getWidth())
            step *= factor;

    for (auto t = std::ceil (visibleStart / step) * step; t < visibleStart + visibleLength; t += step)
    {
        const auto x = xForSeconds (t);
        g.drawVerticalLine (x, static_cast<float> (area.getBottom() - 6), static_cast<float> (area.getBottom()));
        g.drawText (
            juce::String (t, step < 1.0 ? 2 : 0), x + 2, area.getBottom() - 16, 60, 12, juce::Justification::left);
    }
}

void SampleEditor::resized()
{
    auto bar = getLocalBounds().removeFromTop (toolbarHeight).reduced (theme::spacingSmall);

    for (auto* button :
         { &silenceButton, &gainButton, &normalizeButton, &fadeInButton, &fadeOutButton, &reverseButton, &phaseButton })
    {
        button->setBounds (bar.removeFromLeft (30));
        bar.removeFromLeft (theme::spacingSmall);
    }

    bar.removeFromLeft (theme::spacingMedium);

    for (auto* button : { &zoomInButton, &zoomOutButton, &zoomFitButton })
    {
        button->setBounds (bar.removeFromLeft (30));
        bar.removeFromLeft (theme::spacingSmall);
    }

    infoLabel.setBounds (getLocalBounds().removeFromBottom (infoHeight).reduced (theme::spacingSmall, 0));
}

void SampleEditor::mouseDown (const juce::MouseEvent& event)
{
    if (! info || ! waveformArea().contains (event.getPosition()))
        return;

    dragAnchor = juce::jlimit (0.0, info->lengthSeconds, secondsAtX (event.x));
    selection.reset();
    refresh();
}

void SampleEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (! info || event.getDistanceFromDragStart() < 3)
        return;

    const auto current = juce::jlimit (0.0, info->lengthSeconds, secondsAtX (event.x));
    selection = std::pair { juce::jmin (dragAnchor, current), juce::jmax (dragAnchor, current) };
    refresh();
}

void SampleEditor::mouseUp (const juce::MouseEvent&)
{
    if (selection && selection->second - selection->first < 0.001)
        selection.reset();

    refresh();
}

void SampleEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! info)
        return;

    const auto delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

    if (event.mods.isCtrlDown())
    {
        zoom (delta > 0.0f ? 1.0 / 1.25 : 1.25, secondsAtX (event.x));
        return;
    }

    visibleStart = juce::jlimit (
        0.0, juce::jmax (0.0, info->lengthSeconds - visibleLength), visibleStart - delta * visibleLength * 0.3);
    repaint();
}

} // namespace fk::ui
