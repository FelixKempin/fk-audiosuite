#include "fk/ui/ClipStretchPanel.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
} // namespace

ClipStretchPanel::ClipStretchPanel (services::ClipService& clipService,
                                    services::TrackService& trackService,
                                    services::TrackHandle& audioTrack,
                                    juce::String id)
    : clips (clipService)
    , tracks (trackService)
    , track (audioTrack)
    , clipId (std::move (id))
    , detectButton (TRANS ("Detect"))
{
    followToggle.setButtonText (TRANS ("Follow tempo"));
    followToggle.setTooltip (TRANS ("Music mode: tempo changes in the project stretch the material along. Needs the tempo of the material."));
    followToggle.onClick = [this] { applyFollow(); };

    bpmLabel.setText (TRANS ("Tempo of the material"), juce::dontSendNotification);
    bpmField.setEditable (true);
    bpmField.setColour (juce::Label::backgroundColourId, theme::backgroundInput);
    bpmField.setTooltip (TRANS ("Tempo in BPM; leave empty and click \"Detect\""));
    bpmField.onTextChange = [this]
    {
        if (followToggle.getToggleState())
            applyFollow();
    };

    detectButton.setTooltip (TRANS ("Estimate the tempo from the rhythm (needs at least 4 s with clear beats)"));
    detectButton.onClick = [this] { detect(); };

    pitchLabel.setText (TRANS ("Pitch"), juce::dontSendNotification);
    pitchSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    pitchSlider.setRange (-24.0, 24.0, 0.1);
    pitchSlider.setTextValueSuffix (" " + TRANS ("st"));
    pitchSlider.setDoubleClickReturnValue (true, 0.0);
    pitchSlider.setTooltip (TRANS ("Pitch in semitones, length stays. Double-click: 0"));
    pitchSlider.onDragEnd = [this]
    {
        if (const auto result = clips.setPitch ({ clipId }, static_cast<float> (pitchSlider.getValue())); ! result)
            report (result.error());

        refresh();
    };
    pitchSlider.onValueChange = [this]
    {
        // Tastatur und Doppelklick lösen kein onDragEnd aus: dann sofort übernehmen.
        if (! pitchSlider.isMouseButtonDown())
            pitchSlider.onDragEnd();
    };

    speedInfo.setFont (juce::FontOptions (12.0f));
    speedInfo.setColour (juce::Label::textColourId, theme::textSecondary);

    for (auto* component : std::initializer_list<juce::Component*> {
             &followToggle, &bpmLabel, &bpmField, &detectButton, &pitchLabel, &pitchSlider, &speedInfo })
        addAndMakeVisible (component);

    refresh();
    setSize (340, 150);
}

services::ClipInfo ClipStretchPanel::currentClip() const
{
    for (const auto& clip : tracks.getClips (track))
        if (clip.id == clipId)
            return clip;

    return {};
}

void ClipStretchPanel::refresh()
{
    const auto clip = currentClip();
    followToggle.setToggleState (clip.followsTempo, juce::dontSendNotification);

    if (clip.sourceBpm > 0.0)
        bpmField.setText (juce::String (clip.sourceBpm, 2), juce::dontSendNotification);

    pitchSlider.setValue (clip.pitchSemitones, juce::dontSendNotification);
    speedInfo.setText (clip.followsTempo
                           ? TRANS ("Length follows the project tempo.")
                           : plugins::i18n::format (TRANS ("Speed: {percent} %  (Alt+drag on the right clip edge stretches)"),
                                                    { { "percent", juce::String (juce::roundToInt (clip.speedRatio * 100.0)) } }),
                       juce::dontSendNotification);
}

void ClipStretchPanel::applyFollow()
{
    const auto bpm = bpmField.getText().replaceCharacter (',', '.').getDoubleValue();

    if (const auto result = clips.setFollowsTempo ({ clipId }, followToggle.getToggleState(), bpm); ! result)
        report (result.error());

    refresh();
}

void ClipStretchPanel::detect()
{
    const auto bpm = clips.detectTempo (clipId);

    if (! bpm)
    {
        report (bpm.error());
        return;
    }

    bpmField.setText (juce::String (*bpm, 2), juce::dontSendNotification);

    if (followToggle.getToggleState())
        applyFollow();
}

void ClipStretchPanel::report (const core::Error& error) const
{
    if (onError)
        onError (services::errorText (error));
}

void ClipStretchPanel::resized()
{
    constexpr int rowHeight = 26;
    auto area = getLocalBounds().reduced (theme::spacingMedium);
    followToggle.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingSmall);

    auto bpmRow = area.removeFromTop (rowHeight);
    bpmLabel.setBounds (bpmRow.removeFromLeft (130));
    detectButton.setBounds (bpmRow.removeFromRight (80));
    bpmRow.removeFromRight (theme::spacingSmall);
    bpmField.setBounds (bpmRow);
    area.removeFromTop (theme::spacingSmall);

    auto pitchRow = area.removeFromTop (rowHeight);
    pitchLabel.setBounds (pitchRow.removeFromLeft (70));
    pitchSlider.setBounds (pitchRow);
    area.removeFromTop (theme::spacingSmall);
    speedInfo.setBounds (area.removeFromTop (rowHeight));
}

} // namespace fk::ui
