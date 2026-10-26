#include "fk/ui/ChannelStrip.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/PluginMenu.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int meterRefreshHz = 30;
constexpr double minVolumeDb = -60.0; // untere Fader-Stellung = Stille
constexpr double maxVolumeDb = 6.0;
} // namespace

ChannelStrip::ChannelStrip (services::TrackService& trackService,
                            services::MixerService& mixerService,
                            services::PluginService& pluginService,
                            services::SendService& sendService,
                            services::TrackHandle& audioTrack,
                            juce::Colour trackColour)
    : tracks (trackService)
    , mixer (mixerService)
    , plugins (pluginService)
    , sends (sendService)
    , track (audioTrack)
    , colour (trackColour)
    , muteButton ("M")
    , soloButton ("S")
{
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    nameLabel.setMinimumHorizontalScale (0.7f);

    for (auto& slot : insertSlots)
    {
        slot.onAddRequested = [this] { choosePluginForSlot(); };
        slot.setPresetSource (&plugins);
        slot.onError = [this] (const juce::String& message)
        {
            if (onError)
                onError (message);
        };
        addAndMakeVisible (slot);
    }

    trimKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trimKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trimKnob.setRange (-24.0, 24.0, 0.1);
    trimKnob.setDoubleClickReturnValue (true, 0.0);
    trimKnob.setPopupDisplayEnabled (true, true, nullptr);
    trimKnob.setTextValueSuffix (" dB");
    trimKnob.setTooltip (TRANS ("Trim: level before the inserts. Double-click: 0 dB"));
    trimKnob.onDragEnd = [this]
    {
        if (const auto result = mixer.setTrimDb (track, static_cast<float> (trimKnob.getValue())); ! result)
            reportError (result.error());
    };
    trimKnob.onValueChange = [this]
    {
        if (! trimKnob.isMouseButtonDown())
            trimKnob.onDragEnd();
    };
    phaseButton.setButtonText (juce::String::fromUTF8 ("Ø"));
    phaseButton.setClickingTogglesState (true);
    phaseButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    phaseButton.setTooltip (TRANS ("Invert phase"));
    phaseButton.onClick = [this]
    {
        if (const auto result = mixer.setPhaseInverted (track, phaseButton.getToggleState()); ! result)
            reportError (result.error());
    };
    addAndMakeVisible (trimKnob);
    addAndMakeVisible (phaseButton);

    panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setDoubleClickReturnValue (true, 0.0);
    panSlider.onValueChange = [this] { mixer.setPan (track, static_cast<float> (panSlider.getValue())); };

    // Geste melden: Tracktion schreibt Automation (Touch/Latch) nur zwischen Anfang und Ende.
    panSlider.onDragStart = [this] { mixer.beginGesture (track, services::MixerService::MixParameter::pan); };
    panSlider.onDragEnd = [this] { mixer.endGesture (track, services::MixerService::MixParameter::pan); };

    volumeSlider.setSliderStyle (juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    volumeSlider.setRange (minVolumeDb, maxVolumeDb, 0.1);
    volumeSlider.setSkewFactorFromMidPoint (-12.0); // mehr Weg im nützlichen Bereich um 0 dB
    volumeSlider.setDoubleClickReturnValue (true, 0.0);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.onValueChange = [this]
    {
        const auto value = volumeSlider.getValue();
        mixer.setVolumeDb (track, value <= minVolumeDb ? -100.0f : static_cast<float> (value));
    };

    volumeSlider.onDragStart = [this] { mixer.beginGesture (track, services::MixerService::MixParameter::volume); };
    volumeSlider.onDragEnd = [this] { mixer.endGesture (track, services::MixerService::MixParameter::volume); };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, theme::mute);
    muteButton.onClick = [this] { tracks.setMuted (track, muteButton.getToggleState()); };

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    soloButton.onClick = [this] { tracks.setSolo (track, soloButton.getToggleState()); };

    outputBox.onChange = [this]
    {
        if (outputBox.getSelectedId() <= 0)
            return;

        if (const auto result =
                tracks.setOutput (track, outputs[static_cast<std::size_t> (outputBox.getSelectedId() - 1)].id);
            ! result)
            reportError (result.error());
    };

    addAndMakeVisible (outputBox);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (panSlider);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (meter);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

    refresh();
    startTimerHz (meterRefreshHz);
}

ChannelStrip::~ChannelStrip() = default;

void ChannelStrip::refresh()
{
    const auto info = tracks.getTrackInfo (track);
    nameLabel.setText (info.name, juce::dontSendNotification);
    muteButton.setToggleState (info.muted, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);
    isGroup = info.kind == services::TrackKind::group || info.kind == services::TrackKind::fx;

    outputs = tracks.getAvailableOutputs (track);
    outputBox.clear (juce::dontSendNotification);
    int itemId = 1;
    int selectedItemId = 1;

    for (const auto& output : outputs)
    {
        outputBox.addItem (output.name, itemId);

        if (output.id == info.outputId)
            selectedItemId = itemId;

        ++itemId;
    }

    outputBox.setSelectedId (selectedItemId, juce::dontSendNotification);
    refreshSends();
    resized();

    const auto volumeDb = mixer.getVolumeDb (track);
    volumeSlider.setValue (juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (volumeDb)),
                           juce::dontSendNotification);
    panSlider.setValue (mixer.getPan (track), juce::dontSendNotification);
    trimKnob.setValue (mixer.getTrimDb (track), juce::dontSendNotification);
    phaseButton.setToggleState (mixer.isPhaseInverted (track), juce::dontSendNotification);

    const auto inserts = plugins.getInserts (track);

    for (std::size_t i = 0; i < insertSlots.size(); ++i)
    {
        auto& slot = insertSlots[i];

        if (i < inserts.size())
        {
            const auto insertId = inserts[i].id;
            slot.setInsert (inserts[i]);
            slot.onOpenRequested = [this, insertId]
            {
                if (onOpenInsertEditor)
                    onOpenInsertEditor (insertId);
            };
            slot.onEnabledChanged = [this, insertId] (bool enabled)
            {
                if (const auto result = plugins.setInsertEnabled (insertId, enabled); ! result)
                    reportError (result.error());
            };
            slot.onRemoveRequested = [this, insertId]
            {
                if (const auto result = plugins.removeInsert (insertId); ! result)
                    reportError (result.error());
            };
            slot.getSidechainSources = [this, insertId] { return plugins.getSidechainSources (insertId); };
            slot.onSidechainChosen = [this, insertId] (const juce::String& sourceTrackId)
            {
                if (const auto result = plugins.setSidechainSource (insertId, sourceTrackId); ! result)
                    reportError (result.error());
            };
        }
        else
        {
            slot.setInsert (std::nullopt);
            slot.onOpenRequested = nullptr;
            slot.onEnabledChanged = nullptr;
            slot.onRemoveRequested = nullptr;
            slot.getSidechainSources = nullptr;
            slot.onSidechainChosen = nullptr;
        }
    }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    g.fillAll (isGroup ? theme::backgroundElevated : theme::backgroundPanel);
    g.setColour (colour);
    g.fillRect (0, 0, getWidth(), theme::spacingSmall);
    g.setColour (theme::borderSubtle);
    g.drawVerticalLine (getWidth() - 1, 0.0f, static_cast<float> (getHeight()));
}

void ChannelStrip::resized()
{
    constexpr int slotHeight = 20;
    constexpr int panSize = 40;
    constexpr int buttonHeight = 22;
    constexpr int meterWidth = 14;

    auto area = getLocalBounds().reduced (theme::spacingSmall);
    area.removeFromTop (theme::spacingSmall);

    nameLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (theme::spacingSmall);

    for (auto& slot : insertSlots)
    {
        slot.setBounds (area.removeFromTop (slotHeight));
        area.removeFromTop (2);
    }

    for (auto& row : sendRows)
    {
        row->setBounds (area.removeFromTop (slotHeight));
        area.removeFromTop (2);
    }

    area.removeFromTop (theme::spacingSmall);
    {
        auto trimRow = area.removeFromTop (24);
        const auto rowWidth = 24 + theme::spacingSmall + 24;
        trimRow = trimRow.withSizeKeepingCentre (rowWidth, 24);
        trimKnob.setBounds (trimRow.removeFromLeft (24));
        trimRow.removeFromLeft (theme::spacingSmall);
        phaseButton.setBounds (trimRow);
    }
    area.removeFromTop (theme::spacingSmall);
    panSlider.setBounds (area.removeFromTop (panSize).withSizeKeepingCentre (panSize, panSize));
    area.removeFromTop (theme::spacingSmall);

    outputBox.setBounds (area.removeFromBottom (slotHeight));
    area.removeFromBottom (theme::spacingSmall);

    auto buttonRow = area.removeFromBottom (buttonHeight);
    muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (1, 0));
    soloButton.setBounds (buttonRow.reduced (1, 0));
    area.removeFromBottom (theme::spacingSmall);

    meter.setBounds (area.removeFromRight (meterWidth).withTrimmedBottom (22));
    area.removeFromRight (theme::spacingSmall);
    volumeSlider.setBounds (area);
}

void ChannelStrip::timerCallback()
{
    if (! isShowing())
        return; // MixConsole verdeckt (anderer Reiter): nichts lesen, nichts zeichnen

    const auto levels = mixer.getLevels (track);
    meter.setLevels (levels.leftDb, levels.rightDb);

    // Automation bewegt Parameter ohne Projektänderung: Regler nachführen, solange niemand zieht.
    if (! volumeSlider.isMouseButtonDown())
        volumeSlider.setValue (juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (mixer.getVolumeDb (track))),
                               juce::dontSendNotification);

    if (! panSlider.isMouseButtonDown())
        panSlider.setValue (mixer.getPan (track), juce::dontSendNotification);

    const auto reductions = plugins.getInsertGainReductionsDb (track);

    for (std::size_t i = 0; i < insertSlots.size(); ++i)
        insertSlots[i].setMeterDb (i < reductions.size() ? reductions[i] : std::nullopt);
}

void ChannelStrip::refreshSends()
{
    sendRows.clear();

    for (const auto& send : sends.getSends (track))
    {
        auto row = std::make_unique<SendRow>();
        row->setSend (send);
        const auto fxTrackId = send.fxTrackId;
        row->onLevelChanged = [this, fxTrackId] (float gainDb)
        {
            if (const auto result = sends.setSendLevel (track, fxTrackId, gainDb); ! result)
                reportError (result.error());
        };
        row->onPreFaderChanged = [this, fxTrackId] (bool preFader)
        {
            if (const auto result = sends.setSendPreFader (track, fxTrackId, preFader); ! result)
                reportError (result.error());
        };
        addAndMakeVisible (*row);
        sendRows.push_back (std::move (row));
    }
}

void ChannelStrip::choosePluginForSlot()
{
    const auto effects = plugins.getAvailableEffects();
    auto menu = buildPluginMenu (effects);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<ChannelStrip> (this), effects] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            const auto& chosen = effects[static_cast<std::size_t> (result - 1)];

                            if (const auto inserted = safeThis->plugins.insertPlugin (safeThis->track, chosen.id);
                                ! inserted)
                                safeThis->reportError (inserted.error());
                        });
}

void ChannelStrip::reportError (const core::Error& error) const
{
    juce::Logger::writeToLog ("Mixer-Aktion fehlgeschlagen: " + services::errorText (error));

    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
