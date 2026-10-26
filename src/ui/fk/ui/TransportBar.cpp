#include "fk/ui/TransportBar.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <array>

namespace fk::ui
{

namespace
{
constexpr int positionRefreshHz = 30;
constexpr int buttonWidth = 36;
constexpr int rowHeight = theme::transportBarHeight;
constexpr std::array<double, 5> rollChoices { 0.0, 0.5, 1.0, 2.0, 4.0 }; // Sekunden, 0 = aus

juce::String rollText (double seconds)
{
    return seconds <= 0.0 ? TRANS ("off") : juce::String (seconds, seconds < 1.0 ? 1 : 0) + " s";
}

/// Locator-Anzeige: Takt.Schlag ohne Ticks, z. B. "L  3.1".
juce::String locatorText (const juce::String& prefix, const juce::String& barsBeats)
{
    const auto bar = barsBeats.upToFirstOccurrenceOf (".", false, false).getIntValue();
    const auto beat =
        barsBeats.fromFirstOccurrenceOf (".", false, false).upToFirstOccurrenceOf (".", false, false).getIntValue();
    return prefix + "  " + juce::String (bar) + "." + juce::String (beat);
}

/// Label, das bei Klick eine Aktion auslöst (Locatoren, Zeitformat).
void makeClickable (juce::Label& label)
{
    label.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    label.setInterceptsMouseClicks (true, false);
}
} // namespace

TransportBar::TransportBar (Services services)
    : transport (services.transport)
    , timeline (services.timeline)
    , automation (services.automation)
    , markers (services.markers)
    , projectSettings (services.projectSettings)
    , returnToStartButton (Icon::returnToStart, TRANS ("Go to start (Home)"))
    , rewindButton (Icon::rewind, TRANS ("Rewind: one bar back, hold to repeat (Num -)"))
    , forwardButton (Icon::fastForward, TRANS ("Forward: one bar ahead, hold to repeat (Num +)"))
    , previousMarkerButton (Icon::markerPrevious, TRANS ("Go to previous marker (Shift+B)"))
    , nextMarkerButton (Icon::markerNext, TRANS ("Go to next marker (Shift+N)"))
    , addMarkerButton (Icon::markerAdd, TRANS ("Add marker at the cursor (Insert)"))
    , playButton (Icon::play, TRANS ("Play (Space)"))
    , stopButton (Icon::stop, TRANS ("Stop (Space)"))
    , recordButton (Icon::record, TRANS ("Record on armed tracks (*)"))
    , retrospectiveButton (Icon::retrospective,
                           TRANS ("Retrospective record: bring MIDI played without recording in the last 30 seconds onto the armed tracks (Shift+Num *)"))
    , loopButton (Icon::loop, TRANS ("Loop on/off; drag a range in the ruler (/)"))
    , punchButton (Icon::punch,
                   TRANS ("Punch in/out: record only between locators L and R (I). Turns loop off."))
    , preRollButton (Icon::preRoll, {}, TRANS ("Pre"))
    , postRollButton (Icon::postRoll, {}, TRANS ("Post"))
    , metronomeButton (Icon::metronome, TRANS ("Metronome on/off (C)"))
    , automationReadButton (Icon::automationRead, TRANS ("Automation read: curves control the faders"))
    , automationWriteButton (Icon::automationWrite,
                             TRANS ("Automation write: record fader movements during playback (mode per track)"))
{
    returnToStartButton.onClick = [this] { transport.returnToStart(); };

    // Gedrückt halten wiederholt: nach 400 ms alle 120 ms einen Takt.
    rewindButton.setRepeatSpeed (400, 120);
    rewindButton.onClick = [this] { stepBars (-1); };
    forwardButton.setRepeatSpeed (400, 120);
    forwardButton.onClick = [this] { stepBars (1); };

    previousMarkerButton.onClick = [this]
    {
        if (const auto seconds = markers.getPreviousMarkerSeconds (transport.getPositionSeconds()))
            transport.setPositionSeconds (*seconds);
    };
    nextMarkerButton.onClick = [this]
    {
        if (const auto seconds = markers.getNextMarkerSeconds (transport.getPositionSeconds()))
            transport.setPositionSeconds (*seconds);
    };
    addMarkerButton.onClick = [this] { (void)markers.addMarker (transport.getPositionSeconds()); };

    playButton.onClick = [this] { transport.play(); };
    stopButton.onClick = [this] { transport.stop(); };
    recordButton.onClick = [this] { transport.record(); };
    retrospectiveButton.onClick = [this] { captureRetrospective(); };

    playButton.setOnColour (theme::meterLow);
    recordButton.setOnColour (theme::record);

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this]
    {
        // Loop und Punch schließen sich aus (Tracktion nimmt nicht mit beidem auf).
        if (loopButton.getToggleState() && transport.isPunchEnabled())
            transport.setPunchEnabled (false);

        timeline.setLooping (loopButton.getToggleState());
    };

    punchButton.setOnColour (theme::record);
    punchButton.onClick = [this] { togglePunch(); };
    preRollButton.setOnColour (theme::accent);
    preRollButton.onClick = [this] { showRollMenu (true); };
    postRollButton.setOnColour (theme::accent);
    postRollButton.onClick = [this] { showRollMenu (false); };

    metronomeButton.setClickingTogglesState (true);
    metronomeButton.onClick = [this] { timeline.setMetronomeEnabled (metronomeButton.getToggleState()); };

    automationReadButton.setClickingTogglesState (true);
    automationReadButton.setOnColour (theme::meterLow);
    automationReadButton.onClick = [this] { automation.setReadEnabled (automationReadButton.getToggleState()); };

    automationWriteButton.setClickingTogglesState (true);
    automationWriteButton.setOnColour (theme::record);
    automationWriteButton.onClick = [this] { automation.setWriteEnabled (automationWriteButton.getToggleState()); };

    using CountIn = services::TransportService::CountIn;
    countInBox.setTooltip (
        TRANS ("Count-in before recording: the metronome counts in, recording starts at the cursor"));
    countInBox.addItem (services::TransportService::getCountInName (CountIn::none), 1);
    countInBox.addItem (services::TransportService::getCountInName (CountIn::oneBar), 2);
    countInBox.addItem (services::TransportService::getCountInName (CountIn::twoBars), 3);
    countInBox.onChange = [this]
    {
        const auto id = countInBox.getSelectedId();
        transport.setCountIn (id == 2 ? CountIn::oneBar : id == 3 ? CountIn::twoBars : CountIn::none);
    };

    styleDisplayLabel (barsLabel, 18.0f);
    styleDisplayLabel (timeLabel, 15.0f);
    styleDisplayLabel (leftLocatorLabel, 13.0f);
    styleDisplayLabel (rightLocatorLabel, 13.0f);
    styleDisplayLabel (tempoLabel, 15.0f);
    styleDisplayLabel (timeSignatureLabel, 15.0f);

    barsLabel.setTooltip (TRANS ("Position: bar.beat.ticks"));
    timeLabel.setTooltip (TRANS ("Position in time. Click: choose format (time, timecode, samples)"));
    makeClickable (timeLabel);
    timeLabel.addMouseListener (this, false);
    leftLocatorLabel.setTooltip (
        TRANS ("Left locator (start of loop and punch). Click: jump there (Num 1). Right-click: set to the cursor (Ctrl+Num 1)"));
    rightLocatorLabel.setTooltip (
        TRANS ("Right locator (end of loop and punch). Click: jump there (Num 2). Right-click: set to the cursor (Ctrl+Num 2)"));
    makeClickable (leftLocatorLabel);
    makeClickable (rightLocatorLabel);
    leftLocatorLabel.addMouseListener (this, false);
    rightLocatorLabel.addMouseListener (this, false);

    tempoLabel.setEditable (false, true, false);
    tempoLabel.onTextChange = [this] { applyTempoFromLabel(); };
    tempoLabel.setTooltip (TRANS ("Tempo in BPM, double-click to edit"));

    timeSignatureLabel.setEditable (false, true, false);
    timeSignatureLabel.onTextChange = [this] { applyTimeSignatureFromLabel(); };
    timeSignatureLabel.setTooltip (TRANS ("Time signature, e.g. 4/4 or 6/8, double-click to edit"));

    dspLoadLabel.setJustificationType (juce::Justification::centredRight);
    dspLoadLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    dspLoadLabel.setTooltip (
        TRANS ("Audio load: share of the time the audio callback computes. From 98 % the engine mutes so nothing hangs. Remedies: bake tracks, a larger buffer in the audio settings, a release build instead of a debug build."));
    addAndMakeVisible (dspLoadLabel);

    const auto button = [] (juce::Component& component, bool group = false)
    { return Item { &component, buttonWidth, group }; };
    items = { button (returnToStartButton),
              button (rewindButton),
              button (forwardButton),
              button (previousMarkerButton),
              button (nextMarkerButton),
              button (addMarkerButton),
              button (playButton, true),
              button (stopButton),
              button (recordButton),
              button (retrospectiveButton),
              button (loopButton),
              button (punchButton),
              Item { &preRollButton, 64, false },
              Item { &postRollButton, 70, false },
              button (metronomeButton),
              Item { &countInBox, 112, false },
              button (automationReadButton, true),
              button (automationWriteButton),
              Item { &barsLabel, 128, true },
              Item { &timeLabel, 124, false },
              Item { &leftLocatorLabel, 78, true },
              Item { &rightLocatorLabel, 78, false },
              Item { &tempoLabel, 100, true },
              Item { &timeSignatureLabel, 56, false } };

    for (const auto& item : items)
        addAndMakeVisible (item.component);

    transport.addListener (*this);
    timeline.addListener (*this);
    automation.addListener (*this);
    automationChanged();
    updateButtons();
    updateTimelineFields();
    startTimerHz (positionRefreshHz);
}

TransportBar::~TransportBar()
{
    automation.removeListener (*this);
    timeline.removeListener (*this);
    transport.removeListener (*this);
}

int TransportBar::layoutItems (int width, std::vector<juce::Rectangle<int>>* cells) const
{
    constexpr int dspWidth = 110;
    const int left = theme::spacingMedium;
    const int right = width - theme::spacingMedium;
    int row = 0;
    int x = left;

    // Gruppen bleiben zusammen: Passt eine Gruppe nicht mehr in die Reihe, wandert sie ganz in die nächste.
    const auto groupWidth = [this] (std::size_t first)
    {
        int total = 0;

        for (auto i = first; i < items.size() && (i == first || ! items[i].startsGroup); ++i)
            total += items[i].width + theme::spacingSmall;

        return total;
    };

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const int gap = item.startsGroup && x > left ? theme::spacingMedium : 0;
        const int needed = item.startsGroup ? groupWidth (i) - theme::spacingSmall : item.width;

        if (x > left && x + gap + needed > right)
        {
            ++row;
            x = left;
        }
        else
        {
            x += gap;
        }

        if (cells != nullptr)
            cells->push_back (
                juce::Rectangle<int> (x, row * rowHeight, item.width, rowHeight).reduced (0, theme::spacingMedium));

        x += item.width + theme::spacingSmall;
    }

    // DSP-Last rechts in der letzten Reihe; reicht der Platz nicht, eine eigene Reihe.
    if (right - x < dspWidth)
    {
        ++row;
        x = left;
    }

    if (cells != nullptr)
        cells->push_back (
            juce::Rectangle<int> (x, row * rowHeight, right - x, rowHeight).reduced (0, theme::spacingMedium));

    return (row + 1) * rowHeight;
}

int TransportBar::getRequiredHeight (int width) const
{
    return layoutItems (width, nullptr);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));
}

void TransportBar::resized()
{
    std::vector<juce::Rectangle<int>> cells;
    layoutItems (getWidth(), &cells);

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        // Auswahllisten etwas flacher als Buttons, sonst wirken sie aufgeblasen.
        const bool isComboBox = dynamic_cast<juce::ComboBox*> (items[i].component) != nullptr;
        items[i].component->setBounds (isComboBox ? cells[i].reduced (0, 4) : cells[i]);
    }

    dspLoadLabel.setBounds (cells.back());
}

void TransportBar::mouseUp (const juce::MouseEvent& event)
{
    if (event.eventComponent == &timeLabel)
    {
        showTimeDisplayMenu();
        return;
    }

    for (const bool left : { true, false })
    {
        if (event.eventComponent != (left ? &leftLocatorLabel : &rightLocatorLabel))
            continue;

        if (event.mods.isPopupMenu())
            showLocatorMenu (left);
        else
            jumpToLocator (left);
    }
}

void TransportBar::stepBars (int direction)
{
    const auto position = transport.getPositionSeconds();
    transport.setPositionSeconds (direction < 0 ? timeline.getPreviousBarSeconds (position)
                                                : timeline.getNextBarSeconds (position));
}

void TransportBar::jumpToLocator (bool left)
{
    const auto range = timeline.getLoopRange();
    transport.setPositionSeconds (left ? range.startSeconds : range.endSeconds);
}

void TransportBar::setLocatorToPlayhead (bool left)
{
    auto range = timeline.getLoopRange();
    const auto position = transport.getPositionSeconds();

    if (left)
        range.startSeconds = position;
    else
        range.endSeconds = position;

    // Vertauschte Locatoren gerade rücken, wie Cubase es auch tut.
    if (range.endSeconds < range.startSeconds)
        std::swap (range.startSeconds, range.endSeconds);

    timeline.setLoopRange (range);
}

void TransportBar::captureRetrospective()
{
    const auto captured = transport.captureRetrospective();

    if (! onMessage)
        return;

    if (! captured)
        onMessage (services::errorText (captured.error()));
    else if (*captured == 0)
        onMessage (TRANS ("Nothing to retrieve: no MIDI arrived on an armed track in the last 30 seconds (arm a track, then play)."));
}

void TransportBar::togglePunch()
{
    transport.setPunchEnabled (! transport.isPunchEnabled());
    updateTimelineFields();
}

void TransportBar::showRollMenu (bool preRoll)
{
    const auto current = preRoll ? transport.getPreRollSeconds() : transport.getPostRollSeconds();
    juce::PopupMenu menu;
    menu.addSectionHeader (preRoll ? TRANS ("Pre-roll: playback starts before the left locator (with punch)")
                                   : TRANS ("Post-roll: keep playing after the right locator, then stop (with punch)"));

    for (std::size_t i = 0; i < rollChoices.size(); ++i)
        menu.addItem (
            static_cast<int> (i) + 1, rollText (rollChoices[i]), true, std::abs (current - rollChoices[i]) < 0.01);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (preRoll ? &preRollButton : &postRollButton),
                        [safeThis = juce::Component::SafePointer (this), preRoll] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            const auto seconds = rollChoices[static_cast<std::size_t> (chosen - 1)];

                            if (preRoll)
                                safeThis->transport.setPreRollSeconds (seconds);
                            else
                                safeThis->transport.setPostRollSeconds (seconds);

                            safeThis->updateButtons();
                        });
}

void TransportBar::showTimeDisplayMenu()
{
    using services::TimeDisplay;
    const auto settings = projectSettings.getSettings();
    juce::PopupMenu menu;
    menu.addSectionHeader (TRANS ("Time display"));

    for (const auto display : { TimeDisplay::seconds, TimeDisplay::timecode, TimeDisplay::samples })
        menu.addItem (static_cast<int> (display) + 1,
                      display == TimeDisplay::timecode
                          ? plugins::i18n::format (TRANS ("{name} ({fps} fps)"),
                                                   { { "name", services::getTimeDisplayName (display) },
                                                     { "fps", juce::String (settings.timecodeFps) } })
                          : services::getTimeDisplayName (display),
                      true,
                      settings.timeDisplay == display);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (timeLabel),
                        [safeThis = juce::Component::SafePointer (this)] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            auto newSettings = safeThis->projectSettings.getSettings();
                            newSettings.timeDisplay = static_cast<services::TimeDisplay> (chosen - 1);
                            (void)safeThis->projectSettings.setSettings (newSettings);
                        });
}

void TransportBar::showLocatorMenu (bool left)
{
    juce::PopupMenu menu;
    menu.addItem (1, TRANS ("Jump there"));
    menu.addItem (2, TRANS ("Set to cursor"));
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (left ? leftLocatorLabel : rightLocatorLabel),
                        [safeThis = juce::Component::SafePointer (this), left] (int chosen)
                        {
                            if (safeThis == nullptr)
                                return;

                            if (chosen == 1)
                                safeThis->jumpToLocator (left);
                            else if (chosen == 2)
                                safeThis->setLocatorToPlayhead (left);
                        });
}

void TransportBar::transportStateChanged()
{
    updateButtons();
}

void TransportBar::timelineChanged()
{
    updateTimelineFields();
}

void TransportBar::automationChanged()
{
    automationReadButton.setToggleState (automation.isReadEnabled(), juce::dontSendNotification);
    automationWriteButton.setToggleState (automation.isWriteEnabled(), juce::dontSendNotification);
}

void TransportBar::timerCallback()
{
    const auto seconds = transport.getPositionSeconds();
    const auto settings = projectSettings.getSettings();
    barsLabel.setText (timeline.formatBarsBeats (seconds), juce::dontSendNotification);
    timeLabel.setText (services::formatTime (seconds,
                                             settings.timeDisplay,
                                             juce::jmax (1.0, projectSettings.getDeviceSampleRate()),
                                             settings.timecodeFps),
                       juce::dontSendNotification);
    updateDspLoad();
}

void TransportBar::updateDspLoad()
{
    constexpr float warnLoad = 0.7f;
    constexpr float overloadLoad = 0.9f;
    const float load = transport.getDspLoad();

    // Die Engine senkt ihren Messwert nach einem stummen Block sofort ab; deshalb den Hinweis
    // zwei Sekunden halten, sonst blitzt er nur auf.
    if (load >= overloadLoad)
        overloadHoldTicks = 2 * positionRefreshHz;
    else if (overloadHoldTicks > 0)
        --overloadHoldTicks;

    const auto percent = "DSP " + juce::String (juce::roundToInt (load * 100.0f)) + " %";
    dspLoadLabel.setText (overloadHoldTicks > 0 ? percent + TRANS ("  Overload: sound muted") : percent,
                          juce::dontSendNotification);
    dspLoadLabel.setColour (juce::Label::textColourId,
                            overloadHoldTicks > 0 ? theme::record
                            : load >= warnLoad    ? theme::solo
                                                  : theme::textSecondary);
}

void TransportBar::updateButtons()
{
    playButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
    recordButton.setToggleState (transport.isRecording(), juce::dontSendNotification);
    punchButton.setToggleState (transport.isPunchEnabled(), juce::dontSendNotification);

    const auto preRoll = transport.getPreRollSeconds();
    const auto postRoll = transport.getPostRollSeconds();
    preRollButton.setToggleState (preRoll > 0.0, juce::dontSendNotification);
    postRollButton.setToggleState (postRoll > 0.0, juce::dontSendNotification);
    preRollButton.setTooltip (plugins::i18n::format (
        TRANS ("Pre-roll: {value}. With punch, playback starts this long before the left locator. Click: change"),
        { { "value", rollText (preRoll) } }));
    postRollButton.setTooltip (plugins::i18n::format (
        TRANS ("Post-roll: {value}. With punch, playback keeps running this long after the right locator, then stops. Click: change"),
        { { "value", rollText (postRoll) } }));

    using CountIn = services::TransportService::CountIn;
    const auto countIn = transport.getCountIn();
    countInBox.setSelectedId (countIn == CountIn::oneBar    ? 2
                              : countIn == CountIn::twoBars ? 3
                                                            : 1,
                              juce::dontSendNotification);
}

void TransportBar::updateTimelineFields()
{
    loopButton.setToggleState (timeline.isLooping(), juce::dontSendNotification);
    punchButton.setToggleState (transport.isPunchEnabled(), juce::dontSendNotification);
    metronomeButton.setToggleState (timeline.isMetronomeEnabled(), juce::dontSendNotification);

    const auto range = timeline.getLoopRange();
    leftLocatorLabel.setText (locatorText ("L", timeline.formatBarsBeats (range.startSeconds)),
                              juce::dontSendNotification);
    rightLocatorLabel.setText (locatorText ("R", timeline.formatBarsBeats (range.endSeconds)),
                               juce::dontSendNotification);

    const auto signature = timeline.getTimeSignature();
    tempoLabel.setText (juce::String (timeline.getTempoBpm(), 2) + " BPM", juce::dontSendNotification);
    timeSignatureLabel.setText (juce::String (signature.numerator) + "/" + juce::String (signature.denominator),
                                juce::dontSendNotification);
}

void TransportBar::applyTempoFromLabel()
{
    const auto bpm =
        tempoLabel.getText().retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();

    if (bpm > 0.0)
        timeline.setTempoBpm (bpm);

    updateTimelineFields();
}

void TransportBar::applyTimeSignatureFromLabel()
{
    const auto text = timeSignatureLabel.getText().trim();
    const auto numerator = text.upToFirstOccurrenceOf ("/", false, false).getIntValue();
    const auto denominator = text.fromFirstOccurrenceOf ("/", false, false).getIntValue();

    if (numerator > 0 && denominator > 0)
        timeline.setTimeSignature ({ numerator, denominator });

    updateTimelineFields();
}

void TransportBar::styleDisplayLabel (juce::Label& label, float fontSize)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (fontSize));
    label.setColour (juce::Label::backgroundColourId, theme::backgroundInput);
}

} // namespace fk::ui
