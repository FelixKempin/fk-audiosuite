#include "fk/ui/Inspector.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/PluginMenu.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int meterRefreshHz = 30;
constexpr int noInputItemId = 1;
constexpr double minVolumeDb = -60.0;
constexpr double maxVolumeDb = 6.0;

juce::String kindText (const services::TrackInfo& info)
{
    switch (info.kind)
    {
        case services::TrackKind::audio:
            return info.numChannels == 1 ? TRANS ("Audio track, mono") : TRANS ("Audio track, stereo");
        case services::TrackKind::instrument:
            return TRANS ("Instrument track");
        case services::TrackKind::group:
            return TRANS ("Group track");
        case services::TrackKind::fx:
            return TRANS ("FX track (aux return)");
        case services::TrackKind::rackOutput:
            return TRANS ("Output of a rack instrument");
        case services::TrackKind::midi:
            return TRANS ("MIDI track (external device)");
    }

    return {};
}
} // namespace

Inspector::Inspector (services::TrackService& trackService,
                      services::MixerService& mixerService,
                      services::PluginService& pluginService,
                      services::SendService& sendService,
                      services::RackService& rackService,
                      services::ExternalMidiService& externalMidiService)
    : tracks (trackService)
    , mixer (mixerService)
    , plugins (pluginService)
    , sends (sendService)
    , racks (rackService)
    , externalMidi (externalMidiService)
    , openInstrumentButton (Icon::openPlugin,
                            TRANS ("Open instrument: plugin window of the track instrument"))
    , replaceInstrumentButton (Icon::addInstrumentTrack, TRANS ("Replace instrument: another instrument on this track"))
    , addRackMidiButton (Icon::plus,
                         TRANS ("Another MIDI track for this rack instrument (next free channel)"),
                         TRANS ("MIDI Track"))
    , rackOutputsButton (Icon::connections,
                         TRANS ("Enable outputs: each active output is its own channel in the mixer"),
                         TRANS ("Outputs"))
    , bakeTrackButton (
          Icon::bake, TRANS ("Bake Track: render the instrument to audio, inserts stay live"), TRANS ("Track"))
    , bakeInsertsButton (Icon::bake,
                         TRANS ("Bake Inserts: render the instrument and the marked inserts (box left of the slot); all without a mark. Always from slot 1 on"),
                         TRANS ("Inserts"))
    , unbakeButton (Icon::locked,
                    TRANS ("Unbake: everything live and editable again"),
                    TRANS ("Unbake"))
    , armButton ("R")
    , muteButton ("M")
    , soloButton ("S")
{
    bakeLabel.setText (TRANS ("Bake"), juce::dontSendNotification);
    bakeLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    bakeLabel.setFont (juce::FontOptions (11.0f));
    const auto requestBake = [this] (BakeAction action)
    {
        if (track != nullptr && onBakeRequested)
            onBakeRequested (track, action, bakeMarkCount > 0 ? bakeMarkCount : -1);
    };
    bakeTrackButton.onClick = [requestBake] { requestBake (BakeAction::bakeTrack); };
    bakeInsertsButton.onClick = [requestBake] { requestBake (BakeAction::bakeInserts); };
    unbakeButton.onClick = [requestBake] { requestBake (BakeAction::unbake); };

    for (auto* component :
         std::initializer_list<juce::Component*> { &bakeLabel, &bakeTrackButton, &bakeInsertsButton, &unbakeButton })
        addAndMakeVisible (component);

    // Instrument-Zeile: Name (Doppelklick öffnet), Öffnen-Button, Ersetzen-Button.
    instrumentLabel.setText (TRANS ("Instrument"), juce::dontSendNotification);
    instrumentLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    instrumentLabel.setFont (juce::FontOptions (11.0f));
    instrumentNameLabel.setFont (juce::FontOptions (12.0f));
    instrumentNameLabel.setTooltip (TRANS ("Double-click opens the plugin window"));
    openInstrumentButton.onClick = [this]
    {
        if (track != nullptr && onOpenInstrument)
            if (const auto instrument = plugins.getInstrument (*track))
                onOpenInstrument (instrument->id);
    };
    replaceInstrumentButton.onClick = [this] { chooseInstrument(); };
    addAndMakeVisible (instrumentLabel);
    addAndMakeVisible (instrumentNameLabel);
    instrumentNameLabel.addMouseListener (this, false);
    addAndMakeVisible (openInstrumentButton);
    addAndMakeVisible (replaceInstrumentButton);

    // Rack-Instrument: MIDI-Kanal der Spur, weitere MIDI-Spur, Ausgänge aktivieren.
    rackLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    rackLabel.setFont (juce::FontOptions (11.0f));

    for (int channel = 1; channel <= 16; ++channel)
        midiChannelBox.addItem (plugins::i18n::format (TRANS ("Channel {number}"), { { "number", juce::String (channel) } }), channel);

    midiChannelBox.setTooltip (TRANS ("MIDI channel of this track: clips and live playing go to the rack instrument on this channel"));
    midiChannelBox.onChange = [this]
    {
        if (track == nullptr)
            return;

        const auto channel = midiChannelBox.getSelectedId();
        const auto result =
            racks.getRole (*track) ? racks.setMidiChannel (*track, channel) : externalMidi.setChannel (*track, channel);

        if (! result)
            reportError (result.error());
    };

    // MIDI-Spur an einem Gerät: Programm und Bank, gesendet beim Ändern und bei jedem Wiedergabestart.
    programBox.addItem (TRANS ("No program change"), 1);

    for (int program = 0; program < 128; ++program)
        programBox.addItem (plugins::i18n::format (TRANS ("Program {number}"), { { "number", juce::String (program + 1) } }), program + 2);

    programBox.setTooltip (TRANS ("Program change to the device: when changed and at every playback start"));
    programBox.onChange = [this] { applyProgram(); };
    bankSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bankSlider.setRange (-1.0, 16383.0, 1.0);
    bankSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    bankSlider.textFromValueFunction = [] (double value)
    { return value < 0.0 ? TRANS ("no bank")
                         : plugins::i18n::format (TRANS ("Bank {number}"), { { "number", juce::String (juce::roundToInt (value)) } }); };
    bankSlider.valueFromTextFunction = [] (const juce::String& text)
    { return text.retainCharacters ("0123456789").isEmpty() ? -1.0 : text.retainCharacters ("0123456789").getDoubleValue(); };
    bankSlider.setTooltip (TRANS ("Bank (CC 0 and CC 32), 0-16383; \"no bank\" sends only the program"));
    bankSlider.onValueChange = [this] { applyProgram(); };
    addAndMakeVisible (programBox);
    addAndMakeVisible (bankSlider);
    addRackMidiButton.onClick = [this]
    {
        if (track == nullptr)
            return;

        if (const auto role = racks.getRole (*track))
            if (const auto result = racks.addMidiTrack (role->rackId); ! result)
                reportError (result.error());
    };
    rackOutputsButton.onClick = [this] { showOutputsMenu(); };

    for (auto* component : std::initializer_list<juce::Component*> {
             &rackLabel, &midiChannelBox, &addRackMidiButton, &rackOutputsButton })
        addAndMakeVisible (component);

    nameLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    nameLabel.setEditable (false, true, false);
    nameLabel.onTextChange = [this]
    {
        if (track != nullptr)
            tracks.setTrackName (*track, nameLabel.getText());
    };

    kindLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    kindLabel.setFont (juce::FontOptions (11.0f));

    sendsLabel.setText (TRANS ("Sends"), juce::dontSendNotification);

    for (auto* label : { &inputLabel, &outputLabel, &insertsLabel, &sendsLabel })
    {
        label->setColour (juce::Label::textColourId, theme::textSecondary);
        label->setFont (juce::FontOptions (11.0f));
    }

    inputLabel.setText (TRANS ("Input"), juce::dontSendNotification);
    outputLabel.setText (TRANS ("Output"), juce::dontSendNotification);
    insertsLabel.setText (TRANS ("Inserts"), juce::dontSendNotification);

    inputBox.onChange = [this]
    {
        if (track == nullptr)
            return;

        const auto selectedId = inputBox.getSelectedId();
        const auto inputId = selectedId > noInputItemId
                                 ? inputs[static_cast<std::size_t> (selectedId - noInputItemId - 1)].id
                                 : juce::String();

        if (const auto result = tracks.setAudioInput (*track, inputId); ! result)
            reportError (result.error());
    };

    outputBox.onChange = [this]
    {
        if (track == nullptr || outputBox.getSelectedId() <= 0)
            return;

        if (const auto result =
                tracks.setOutput (*track, outputs[static_cast<std::size_t> (outputBox.getSelectedId() - 1)].id);
            ! result)
            reportError (result.error());
    };

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

    // Klein und rund in der Pan-Zeile: Der Inspector hat keinen Platz für eine eigene Zeile.
    trimSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trimSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trimSlider.setPopupDisplayEnabled (true, true, nullptr);
    trimSlider.setRange (-24.0, 24.0, 0.1);
    trimSlider.setTextValueSuffix (" dB");
    trimSlider.setDoubleClickReturnValue (true, 0.0);
    trimSlider.setTooltip (
        TRANS ("Trim: adjust the level before the inserts (gain staging). Double-click: 0 dB"));
    trimSlider.onDragEnd = [this]
    {
        if (track != nullptr)
            if (const auto result = mixer.setTrimDb (*track, static_cast<float> (trimSlider.getValue())); ! result)
                reportError (result.error());
    };
    trimSlider.onValueChange = [this]
    {
        if (! trimSlider.isMouseButtonDown())
            trimSlider.onDragEnd();
    };
    phaseButton.setButtonText (juce::String::fromUTF8 ("Ø"));
    phaseButton.setClickingTogglesState (true);
    phaseButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    phaseButton.setTooltip (TRANS ("Invert phase (polarity), e.g. snare top against snare bottom"));
    phaseButton.onClick = [this]
    {
        if (track != nullptr)
            if (const auto result = mixer.setPhaseInverted (*track, phaseButton.getToggleState()); ! result)
                reportError (result.error());
    };

    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setDoubleClickReturnValue (true, 0.0);
    panSlider.onValueChange = [this]
    {
        if (track != nullptr)
            mixer.setPan (*track, static_cast<float> (panSlider.getValue()));
    };

    // Geste melden: Tracktion schreibt Automation (Touch/Latch) nur zwischen Anfang und Ende.
    panSlider.onDragStart = [this]
    {
        if (track == nullptr)
            return;

        mixer.beginGesture (*track, services::MixerService::MixParameter::pan);
    };
    panSlider.onDragEnd = [this]
    {
        if (track == nullptr)
            return;

        mixer.endGesture (*track, services::MixerService::MixParameter::pan);
    };

    volumeSlider.setSliderStyle (juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    volumeSlider.setRange (minVolumeDb, maxVolumeDb, 0.1);
    volumeSlider.setSkewFactorFromMidPoint (-12.0);
    volumeSlider.setDoubleClickReturnValue (true, 0.0);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.onValueChange = [this]
    {
        if (track == nullptr)
            return;

        const auto value = volumeSlider.getValue();
        mixer.setVolumeDb (*track, value <= minVolumeDb ? -100.0f : static_cast<float> (value));
    };

    armButton.setClickingTogglesState (true);
    armButton.setColour (juce::TextButton::buttonOnColourId, theme::record);
    armButton.onClick = [this]
    {
        if (track != nullptr)
            tracks.setArmed (*track, armButton.getToggleState());
    };
    volumeSlider.onDragStart = [this]
    {
        if (track == nullptr)
            return;

        mixer.beginGesture (*track, services::MixerService::MixParameter::volume);
    };
    volumeSlider.onDragEnd = [this]
    {
        if (track == nullptr)
            return;

        mixer.endGesture (*track, services::MixerService::MixParameter::volume);
    };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, theme::mute);
    muteButton.onClick = [this]
    {
        if (track != nullptr)
            tracks.setMuted (*track, muteButton.getToggleState());
    };
    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    soloButton.onClick = [this]
    {
        if (track != nullptr)
            tracks.setSolo (*track, soloButton.getToggleState());
    };

    for (auto* component : std::initializer_list<juce::Component*> { &nameLabel,
                                                                     &kindLabel,
                                                                     &inputLabel,
                                                                     &inputBox,
                                                                     &outputLabel,
                                                                     &outputBox,
                                                                     &insertsLabel,
                                                                     &panSlider,
                                                                     &trimSlider,
                                                                     &phaseButton,
                                                                     &volumeSlider,
                                                                     &meter,
                                                                     &armButton,
                                                                     &muteButton,
                                                                     &soloButton })
        addAndMakeVisible (component);

    plugins.addListener (*this);
    sends.addListener (*this);
    mixer.addListener (*this);
    setTrack (nullptr);
    startTimerHz (meterRefreshHz);
}

Inspector::~Inspector()
{
    mixer.removeListener (*this);
    sends.removeListener (*this);
    plugins.removeListener (*this);
}

void Inspector::insertsChanged()
{
    refresh(); // Inserts und Instrumentname (Instrument ersetzen, Undo)
}

void Inspector::sendsChanged()
{
    if (track == nullptr)
        return;

    refreshSends();
    resized();
}

void Inspector::mixerChanged()
{
    if (track == nullptr)
        return;

    const auto info = tracks.getTrackInfo (*track);
    muteButton.setToggleState (info.muted, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);
    followAutomation();
}

void Inspector::setTrack (services::TrackHandle* newTrack)
{
    if (newTrack != track)
        bakeMarkCount = 0; // Markierung gehört zur Spur

    track = newTrack;
    refresh();
}

void Inspector::refresh()
{
    const bool hasTrack = track != nullptr;

    // Ohne Spur alle Bedienelemente ausblenden; paint() zeigt dann den Hinweis, was zu tun ist.
    for (auto* child : getChildren())
        child->setVisible (hasTrack);

    repaint();

    if (! hasTrack)
        return;

    const auto info = tracks.getTrackInfo (*track);
    nameLabel.setText (info.name, juce::dontSendNotification);
    kindLabel.setText (kindText (info), juce::dontSendNotification);
    armButton.setToggleState (info.armed, juce::dontSendNotification);
    armButton.setVisible (info.kind == services::TrackKind::audio || services::playsMidi (info.kind));
    muteButton.setToggleState (info.muted, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);

    instrumentNameLabel.setText (info.instrumentName, juce::dontSendNotification);

    for (auto* component : std::initializer_list<juce::Component*> {
             &instrumentLabel, &instrumentNameLabel, &openInstrumentButton, &replaceInstrumentButton })
        component->setVisible (info.isInstrument);

    refreshRack();

    const auto volumeDb = mixer.getVolumeDb (*track);
    volumeSlider.setValue (juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (volumeDb)),
                           juce::dontSendNotification);
    panSlider.setValue (mixer.getPan (*track), juce::dontSendNotification);
    trimSlider.setValue (mixer.getTrimDb (*track), juce::dontSendNotification);
    phaseButton.setToggleState (mixer.isPhaseInverted (*track), juce::dontSendNotification);

    rebuildInputChoices (info);
    rebuildOutputChoices (info);
    refreshBake (info);
    refreshInserts();
    refreshSends();
    resized();
}

void Inspector::applyProgram()
{
    if (track == nullptr)
        return;

    const auto program = programBox.getSelectedId() - 2; // ID 1 = keins (-1)
    const auto bank = juce::roundToInt (bankSlider.getValue());

    if (const auto result = externalMidi.setProgram (*track, program, bank); ! result)
        reportError (result.error());
}

void Inspector::refreshRack()
{
    const auto role = track != nullptr ? racks.getRole (*track) : std::nullopt;
    const auto rack = role ? racks.getRack (role->rackId) : std::nullopt;
    const auto external = track != nullptr ? externalMidi.getSettings (*track) : std::nullopt;

    programBox.setVisible (external.has_value());
    bankSlider.setVisible (external.has_value());

    if (external)
    {
        rackLabel.setVisible (true);
        rackLabel.setText (plugins::i18n::format (TRANS ("MIDI to: {name}"), { { "name", tracks.getTrackInfo (*track).outputName } }),
                           juce::dontSendNotification);
        midiChannelBox.setVisible (true);
        midiChannelBox.setSelectedId (external->channel, juce::dontSendNotification);
        programBox.setSelectedId (external->program + 2, juce::dontSendNotification);
        bankSlider.setValue (external->bank, juce::dontSendNotification);
        addRackMidiButton.setVisible (false);
        rackOutputsButton.setVisible (false);
        return;
    }

    for (auto* component : std::initializer_list<juce::Component*> { &rackLabel, &rackOutputsButton })
        component->setVisible (rack.has_value());

    midiChannelBox.setVisible (rack && role->isMidi);
    addRackMidiButton.setVisible (rack && role->isMidi);

    // Das Instrument gehört dem Rack: Tauschen geht hier nicht, Öffnen schon.
    if (rack)
        replaceInstrumentButton.setVisible (false);

    if (! rack)
        return;

    if (role->isMidi)
    {
        rackLabel.setText (plugins::i18n::format (TRANS ("Rack instrument: {name}"), { { "name", rack->name } }),
                           juce::dontSendNotification);
        midiChannelBox.setSelectedId (role->midiChannel, juce::dontSendNotification);
    }
    else
    {
        const auto pair = static_cast<std::size_t> (role->outputPair);
        rackLabel.setText (plugins::i18n::format (
                               TRANS ("Output {output} of {name}"),
                               { { "output", pair < rack->outputNames.size() ? rack->outputNames[pair] : juce::String() },
                                 { "name", rack->name } }),
                           juce::dontSendNotification);
    }
}

void Inspector::showOutputsMenu()
{
    const auto role = track != nullptr ? racks.getRole (*track) : std::nullopt;
    const auto rack = role ? racks.getRack (role->rackId) : std::nullopt;

    if (! rack)
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader (plugins::i18n::format (TRANS ("Outputs of {name}"), { { "name", rack->name } }));

    for (std::size_t pair = 0; pair < rack->outputNames.size(); ++pair)
        menu.addItem (static_cast<int> (pair) + 1,
                      plugins::i18n::format (TRANS ("Output {output}"), { { "output", rack->outputNames[pair] } }),
                      true,
                      rack->activeOutputs[pair]);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rackOutputsButton),
                        [safeThis = juce::Component::SafePointer<Inspector> (this), rack] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            const auto pair = chosen - 1;
                            const bool active = rack->activeOutputs[static_cast<std::size_t> (pair)];

                            if (const auto result = safeThis->racks.setOutputActive (rack->id, pair, ! active); ! result)
                                safeThis->reportError (result.error());
                        });
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    g.setColour (theme::borderStrong);
    g.drawVerticalLine (getWidth() - 1, 0.0f, static_cast<float> (getHeight()));

    if (track == nullptr)
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (
            TRANS ("Inspector\n\nClick a track in the arrangement to edit its input, output, inserts, volume and level here.\n\nNo track yet? Create an audio, instrument or group track above."),
            getLocalBounds().reduced (theme::spacingLarge),
            juce::Justification::centredTop,
            12);
    }
}

void Inspector::resized()
{
    constexpr int rowHeight = 22;
    constexpr int slotHeight = 20;

    auto area = getLocalBounds().reduced (theme::spacingMedium);

    nameLabel.setBounds (area.removeFromTop (rowHeight));
    kindLabel.setBounds (area.removeFromTop (16));
    area.removeFromTop (theme::spacingMedium);

    if (instrumentLabel.isVisible())
    {
        instrumentLabel.setBounds (area.removeFromTop (14));
        auto instrumentRow = area.removeFromTop (rowHeight);
        replaceInstrumentButton.setBounds (instrumentRow.removeFromRight (rowHeight));
        instrumentRow.removeFromRight (theme::spacingSmall);
        openInstrumentButton.setBounds (instrumentRow.removeFromRight (rowHeight));
        instrumentNameLabel.setBounds (instrumentRow);
        area.removeFromTop (theme::spacingMedium);
    }

    if (rackLabel.isVisible())
    {
        rackLabel.setBounds (area.removeFromTop (14));
        auto rackRow = area.removeFromTop (rowHeight);
        const int third = rackRow.getWidth() / 3;

        if (programBox.isVisible())
        {
            // MIDI-Spur an einem Gerät: Kanal und Programm in einer Zeile, Bank darunter.
            midiChannelBox.setBounds (rackRow.removeFromLeft (third).reduced (1, 0));
            programBox.setBounds (rackRow.reduced (1, 0));
            bankSlider.setBounds (area.removeFromTop (rowHeight));
        }
        else
        {
            if (midiChannelBox.isVisible())
            {
                midiChannelBox.setBounds (rackRow.removeFromLeft (third).reduced (1, 0));
                addRackMidiButton.setBounds (rackRow.removeFromLeft (third).reduced (1, 0));
            }

            rackOutputsButton.setBounds (rackRow.reduced (1, 0));
        }

        area.removeFromTop (theme::spacingMedium);
    }

    inputLabel.setBounds (area.removeFromTop (14));
    inputBox.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingSmall);
    outputLabel.setBounds (area.removeFromTop (14));
    outputBox.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingMedium);

    insertsLabel.setBounds (area.removeFromTop (14));

    for (auto& slot : insertSlots)
    {
        slot.setBounds (area.removeFromTop (slotHeight));
        area.removeFromTop (2);
    }

    area.removeFromTop (theme::spacingSmall);
    bakeLabel.setBounds (area.removeFromTop (14));
    auto bakeRow = area.removeFromTop (rowHeight);
    const int bakeButtonWidth = (bakeRow.getWidth() - 2 * theme::spacingSmall) / 3;

    for (auto* button : { &bakeTrackButton, &bakeInsertsButton, &unbakeButton })
    {
        button->setBounds (bakeRow.removeFromLeft (bakeButtonWidth));
        bakeRow.removeFromLeft (theme::spacingSmall);
    }

    area.removeFromTop (theme::spacingMedium);

    sendsLabel.setBounds (area.removeFromTop (14));
    sendsLabel.setVisible (track != nullptr && ! sendRows.empty());

    for (auto& row : sendRows)
    {
        row->setBounds (area.removeFromTop (slotHeight));
        area.removeFromTop (2);
    }

    if (! sendRows.empty())
        area.removeFromTop (theme::spacingMedium);

    auto buttonRow = area.removeFromBottom (rowHeight);
    const int buttonWidth = buttonRow.getWidth() / 3;
    armButton.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (1, 0));
    muteButton.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (1, 0));
    soloButton.setBounds (buttonRow.reduced (1, 0));
    area.removeFromBottom (theme::spacingSmall);

    // Eine Zeile: Phase, Trim (klein), Panorama.
    auto panRow = area.removeFromTop (rowHeight);
    phaseButton.setBounds (panRow.removeFromLeft (26));
    panRow.removeFromLeft (theme::spacingSmall);
    trimSlider.setBounds (panRow.removeFromLeft (rowHeight));
    panRow.removeFromLeft (theme::spacingSmall);
    panSlider.setBounds (panRow);
    area.removeFromTop (theme::spacingSmall);

    meter.setBounds (area.removeFromRight (16).withTrimmedBottom (22));
    area.removeFromRight (theme::spacingSmall);
    volumeSlider.setBounds (area);
}

void Inspector::timerCallback()
{
    if (! isShowing())
        return;

    if (track == nullptr)
    {
        meter.setLevels (-100.0f, -100.0f);
        return;
    }

    const auto levels = mixer.getLevels (*track);
    meter.setLevels (levels.leftDb, levels.rightDb);
    updateInsertMeters();
    followAutomation();
}

void Inspector::followAutomation()
{
    // Beim Abspielen bewegt die Automation die Parameter, ohne das Projekt zu ändern (kein
    // mixerChanged). Deshalb lesen wir die Werte im Timer nach, solange niemand den Regler zieht.
    if (! volumeSlider.isMouseButtonDown())
        volumeSlider.setValue (
            juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (mixer.getVolumeDb (*track))),
            juce::dontSendNotification);

    if (! panSlider.isMouseButtonDown())
        panSlider.setValue (mixer.getPan (*track), juce::dontSendNotification);
}

void Inspector::updateInsertMeters()
{
    const auto reductions = plugins.getInsertGainReductionsDb (*track);

    for (std::size_t i = 0; i < insertSlots.size(); ++i)
        insertSlots[i].setMeterDb (i < reductions.size() ? reductions[i] : std::nullopt);
}

void Inspector::refreshSends()
{
    sendRows.clear();

    for (const auto& send : sends.getSends (*track))
    {
        auto row = std::make_unique<SendRow>();
        row->setSend (send);
        const auto fxTrackId = send.fxTrackId;
        row->onLevelChanged = [this, fxTrackId] (float gainDb)
        {
            if (track != nullptr)
                if (const auto result = sends.setSendLevel (*track, fxTrackId, gainDb); ! result)
                    reportError (result.error());
        };
        row->onPreFaderChanged = [this, fxTrackId] (bool preFader)
        {
            if (track != nullptr)
                if (const auto result = sends.setSendPreFader (*track, fxTrackId, preFader); ! result)
                    reportError (result.error());
        };
        addAndMakeVisible (*row);
        sendRows.push_back (std::move (row));
    }
}

void Inspector::rebuildInputChoices (const services::TrackInfo& info)
{
    inputBox.clear (juce::dontSendNotification);
    inputs.clear();

    if (info.kind == services::TrackKind::group || info.kind == services::TrackKind::fx ||
        info.kind == services::TrackKind::rackOutput)
    {
        inputBox.addItem (info.kind == services::TrackKind::fx           ? TRANS ("FX track: signal comes from the sends")
                          : info.kind == services::TrackKind::rackOutput ? TRANS ("Signal comes from the rack instrument")
                                                                         : TRANS ("Groups have no input"),
                          noInputItemId);
        inputBox.setSelectedId (noInputItemId, juce::dontSendNotification);
        inputBox.setEnabled (false);
        return;
    }

    inputBox.setEnabled (true);
    inputs = services::playsMidi (info.kind) ? tracks.getAvailableMidiInputs()
                                                          : tracks.getAvailableAudioInputs (*track);
    inputBox.addItem (TRANS ("No input"), noInputItemId);

    int itemId = noInputItemId + 1;
    int selectedItemId = noInputItemId;

    for (const auto& input : inputs)
    {
        inputBox.addItem (input.name, itemId);

        if (input.id == info.inputId)
            selectedItemId = itemId;

        ++itemId;
    }

    inputBox.setSelectedId (selectedItemId, juce::dontSendNotification);
}

void Inspector::rebuildOutputChoices (const services::TrackInfo& info)
{
    outputBox.clear (juce::dontSendNotification);
    outputs = tracks.getAvailableOutputs (*track);

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
}

void Inspector::refreshInserts()
{
    const auto inserts = plugins.getInserts (*track);

    for (std::size_t i = 0; i < insertSlots.size(); ++i)
    {
        auto& slot = insertSlots[i];

        if (i < inserts.size())
        {
            const auto insertId = inserts[i].id;
            const bool baked = inserts[i].baked;
            const int slotNumber = static_cast<int> (i) + 1;
            slot.setInsert (inserts[i]);
            slot.setBakeMarked (! baked && slotNumber <= bakeMarkCount);
            slot.onBakeMarkClicked = [this, slotNumber] { toggleBakeMark (slotNumber); };
            slot.onOpenRequested = [this, insertId, baked]
            {
                if (baked)
                {
                    if (onError)
                        onError (TRANS ("This insert is baked. To edit it, first use \"Unbake\" (Inspector or track header)."));
                    return;
                }

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
            slot.setBakeMarked (false);
            slot.onBakeMarkClicked = nullptr;
            slot.onOpenRequested = nullptr;
            slot.onEnabledChanged = nullptr;
            slot.onRemoveRequested = nullptr;
            slot.getSidechainSources = nullptr;
            slot.onSidechainChosen = nullptr;
        }
    }
}

void Inspector::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.eventComponent == &instrumentNameLabel)
        openInstrumentButton.triggerClick();
}

void Inspector::chooseInstrument()
{
    if (track == nullptr)
        return;

    const auto instruments = plugins.getAvailableInstruments();
    auto menu = buildPluginMenu (instruments);

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&replaceInstrumentButton),
        [this, instruments] (int choice)
        {
            if (choice <= 0 || track == nullptr || static_cast<std::size_t> (choice) > instruments.size())
                return;

            if (const auto result =
                    plugins.setInstrument (*track, instruments[static_cast<std::size_t> (choice - 1)].id);
                ! result)
                reportError (result.error());
            else
                refresh();
        });
}

void Inspector::choosePluginForSlot()
{
    if (track == nullptr)
        return;

    const auto effects = plugins.getAvailableEffects();
    auto menu = buildPluginMenu (effects);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<Inspector> (this), effects] (int result)
                        {
                            if (safeThis == nullptr || result <= 0 || safeThis->track == nullptr)
                                return;

                            const auto& chosen = effects[static_cast<std::size_t> (result - 1)];

                            if (const auto inserted = safeThis->plugins.insertPlugin (*safeThis->track, chosen.id);
                                ! inserted)
                                safeThis->reportError (inserted.error());
                        });
}

void Inspector::reportError (const core::Error& error) const
{
    juce::Logger::writeToLog ("Inspector: " + services::errorText (error));

    if (onError)
        onError (services::errorText (error));
}

void Inspector::toggleBakeMark (int slotNumber)
{
    // Linear wie die Kette: Slot n markieren markiert 1..n; einen markierten Slot anklicken
    // nimmt ihn und alle dahinter heraus.
    bakeMarkCount = slotNumber <= bakeMarkCount ? slotNumber - 1 : slotNumber;
    refreshInserts();

    if (track != nullptr)
        refreshBake (tracks.getTrackInfo (*track));
}

void Inspector::refreshBake (const services::TrackInfo& info)
{
    const bool lockedByGroup = info.bakedIntoGroup.isNotEmpty();
    const bool canBake = info.kind != services::TrackKind::fx && ! lockedByGroup && ! info.inRack;

    for (auto* component :
         std::initializer_list<juce::Component*> { &bakeLabel, &bakeTrackButton, &bakeInsertsButton, &unbakeButton })
        component->setVisible (info.kind != services::TrackKind::fx && info.kind != services::TrackKind::midi && ! info.inRack);

    bakeTrackButton.setVisible (info.kind == services::TrackKind::instrument && ! info.inRack);
    bakeTrackButton.setEnabled (canBake);
    bakeInsertsButton.setEnabled (canBake);
    unbakeButton.setEnabled (info.baked);
    replaceInstrumentButton.setEnabled (! info.baked && ! lockedByGroup);

    bakeLabel.setText (lockedByGroup ? plugins::i18n::format (TRANS ("Bake: baked into group \"{group}\""),
                                                               { { "group", info.bakedIntoGroup } })
                       : info.baked  ? TRANS ("Bake: baked")
                       : bakeMarkCount > 0
                           ? plugins::i18n::format (TRANS ("Bake: inserts 1 to {count} marked"),
                                                    { { "count", juce::String (bakeMarkCount) } })
                           : TRANS ("Bake"),
                       juce::dontSendNotification);
    bakeInsertsButton.setLabel (info.kind == services::TrackKind::group ? TRANS ("Group") : TRANS ("Inserts"));
}

} // namespace fk::ui
