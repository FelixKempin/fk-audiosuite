#include "fk/ui/ProjectSettingsDialog.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <array>
#include <cmath>

namespace fk::ui
{

namespace
{
constexpr std::array<int, 6> sampleRates { 44100, 48000, 88200, 96000, 176400, 192000 };
constexpr std::array<int, 7> autosaveMinutes { 0, 1, 2, 5, 10, 15, 30 }; // 0 = aus
constexpr int autosaveIdOffset = 1;                                      // ComboBox-IDs müssen > 0 sein

int formatId (services::RecordFormat format)
{
    return static_cast<int> (format) + 1;
}

services::RecordFormat formatForId (int id)
{
    return static_cast<services::RecordFormat> (id - 1);
}

} // namespace

ProjectSettingsDialog::ProjectSettingsDialog (services::ProjectSettingsService& settingsService,
                                              services::AutosaveService& autosaveService)
    : settings (settingsService)
    , autosave (autosaveService)
    , switchDeviceButton (TRANS ("Switch device"))
{
    const auto current = settings.getSettings();

    sampleRateLabel.setText (TRANS ("Sample rate"), juce::dontSendNotification);

    for (const auto rate : sampleRates)
        sampleRateBox.addItem (juce::String (rate) + " Hz", rate);

    // Ältere Projekte können eine Rate außerhalb der Liste haben; dann steht sie zusätzlich drin.
    if (const auto currentRate = juce::roundToInt (current.sampleRate); sampleRateBox.indexOfItemId (currentRate) < 0)
        sampleRateBox.addItem (juce::String (currentRate) + " Hz", currentRate);

    sampleRateBox.setSelectedId (juce::roundToInt (current.sampleRate), juce::dontSendNotification);
    sampleRateBox.setTooltip (TRANS ("Sample rate of the project. The audio device should run at the same rate."));
    sampleRateBox.onChange = [this]
    {
        applySettings();
        updateDeviceInfo();
    };

    switchDeviceButton.setTooltip (TRANS ("Switch the audio device to the project sample rate"));
    switchDeviceButton.onClick = [this] { switchDeviceRate(); };

    formatLabel.setText (TRANS ("Recording format"), juce::dontSendNotification);

    for (const auto format :
         { services::RecordFormat::wav, services::RecordFormat::flac, services::RecordFormat::aiff })
        formatBox.addItem (services::ProjectSettingsService::getFormatName (format), formatId (format));

    formatBox.setSelectedId (formatId (current.recordFormat), juce::dontSendNotification);
    formatBox.setTooltip (TRANS ("File format of new recordings. FLAC saves about half the space, lossless."));
    formatBox.onChange = [this]
    {
        fillBitDepths (formatForId (formatBox.getSelectedId()), bitDepthBox.getSelectedId());
        applySettings();
    };

    bitDepthLabel.setText (TRANS ("Bit depth"), juce::dontSendNotification);
    fillBitDepths (current.recordFormat, current.bitDepth);
    bitDepthBox.setTooltip (TRANS ("24 bit is enough for recordings; 32 bit float tolerates overs inside the computer."));
    bitDepthBox.onChange = [this] { applySettings(); };

    audioModeLabel.setText (TRANS ("Audio recording"), juce::dontSendNotification);
    audioModeBox.addItem (TRANS ("Keep takes"), 1);
    audioModeBox.addItem (TRANS ("Replace"), 2);
    audioModeBox.setSelectedId (current.audioRecordMode == services::AudioRecordMode::replace ? 2 : 1,
                                juce::dontSendNotification);
    audioModeBox.setTooltip (TRANS ("Keep takes: the new recording is on top, earlier ones stay selectable as takes. Replace: clips in the recorded range are replaced."));
    audioModeBox.onChange = [this] { applySettings(); };

    midiModeLabel.setText (TRANS ("MIDI recording"), juce::dontSendNotification);
    midiModeBox.addItem (TRANS ("New clip"), 1);
    midiModeBox.addItem (TRANS ("Merge"), 2);
    midiModeBox.addItem (TRANS ("Replace"), 3);
    midiModeBox.setSelectedId (static_cast<int> (current.midiRecordMode) + 1, juce::dontSendNotification);
    midiModeBox.setTooltip (TRANS ("New clip: each recording is its own clip. Merge: notes go into the existing clip (overdub). Replace: notes in the range are replaced."));
    midiModeBox.onChange = [this] { applySettings(); };

    fpsLabel.setText (TRANS ("Timecode"), juce::dontSendNotification);

    for (const auto fps : { 24, 25, 30 })
        fpsBox.addItem (juce::String (fps) + " fps", fps);

    fpsBox.setSelectedId (current.timecodeFps, juce::dontSendNotification);
    fpsBox.setTooltip (
        TRANS ("Frames per second for the timecode display in the transport bar (click the time)."));
    fpsBox.onChange = [this] { applySettings(); };

    crossfadeToggle.setButtonText (TRANS ("Automatic crossfades on overlapping audio clips"));
    crossfadeToggle.setToggleState (current.autoCrossfades, juce::dontSendNotification);
    crossfadeToggle.setTooltip (TRANS ("When two audio clips overlap, they fade into each other in the overlapping area instead of cutting hard."));
    crossfadeToggle.onClick = [this] { applySettings(); };

    autosaveLabel.setText (TRANS ("Autosave"), juce::dontSendNotification);

    for (std::size_t i = 0; i < autosaveMinutes.size(); ++i)
        autosaveBox.addItem (autosaveMinutes[i] == 0
                                 ? TRANS ("off")
                                 : plugins::i18n::format (TRANS ("every {minutes} min"),
                                                          { { "minutes", juce::String (autosaveMinutes[i]) } }),
                             static_cast<int> (i) + autosaveIdOffset);

    const auto minutes =
        static_cast<int> (std::chrono::duration_cast<std::chrono::minutes> (autosave.getInterval()).count());

    for (std::size_t i = 0; i < autosaveMinutes.size(); ++i)
        if (autosaveMinutes[i] == minutes)
            autosaveBox.setSelectedId (static_cast<int> (i) + autosaveIdOffset, juce::dontSendNotification);

    autosaveBox.setTooltip (
        TRANS ("Automatic backup to Backups/ when there are unsaved changes. Applies to all projects."));
    autosaveBox.onChange = [this]
    {
        const auto index = static_cast<std::size_t> (autosaveBox.getSelectedId() - autosaveIdOffset);

        if (index < autosaveMinutes.size())
            autosave.setInterval (std::chrono::minutes (autosaveMinutes[index]));
    };

    hint.setText (TRANS ("Format and bit depth apply to new recordings. Existing files with a different rate are converted by the engine during playback."),
                  juce::dontSendNotification);
    hint.setFont (juce::FontOptions (12.0f));
    deviceInfo.setFont (juce::FontOptions (12.0f));

    for (auto* label : { &sampleRateLabel,
                         &formatLabel,
                         &bitDepthLabel,
                         &audioModeLabel,
                         &midiModeLabel,
                         &fpsLabel,
                         &autosaveLabel,
                         &hint })
        label->setColour (juce::Label::textColourId, theme::textSecondary);

    for (auto* component : std::initializer_list<juce::Component*> { &sampleRateLabel,
                                                                     &sampleRateBox,
                                                                     &deviceInfo,
                                                                     &switchDeviceButton,
                                                                     &formatLabel,
                                                                     &formatBox,
                                                                     &bitDepthLabel,
                                                                     &bitDepthBox,
                                                                     &audioModeLabel,
                                                                     &audioModeBox,
                                                                     &midiModeLabel,
                                                                     &midiModeBox,
                                                                     &fpsLabel,
                                                                     &fpsBox,
                                                                     &crossfadeToggle,
                                                                     &autosaveLabel,
                                                                     &autosaveBox,
                                                                     &hint })
        addAndMakeVisible (component);

    updateDeviceInfo();
    setSize (480, 422);
}

void ProjectSettingsDialog::resized()
{
    constexpr int rowHeight = 26;
    constexpr int labelWidth = 120;

    auto area = getLocalBounds().reduced (theme::spacingLarge);

    auto row = [&area] (juce::Label& label, juce::Component& field)
    {
        auto r = area.removeFromTop (rowHeight);
        label.setBounds (r.removeFromLeft (labelWidth));
        field.setBounds (r.removeFromLeft (160));
        area.removeFromTop (theme::spacingSmall);
        return r;
    };

    row (sampleRateLabel, sampleRateBox);
    auto deviceRow = area.removeFromTop (rowHeight);
    deviceRow.removeFromLeft (labelWidth);
    switchDeviceButton.setBounds (deviceRow.removeFromRight (130));
    deviceInfo.setBounds (deviceRow);
    area.removeFromTop (theme::spacingMedium);
    row (formatLabel, formatBox);
    row (bitDepthLabel, bitDepthBox);
    row (audioModeLabel, audioModeBox);
    row (midiModeLabel, midiModeBox);
    row (fpsLabel, fpsBox);
    crossfadeToggle.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingSmall);
    row (autosaveLabel, autosaveBox);
    area.removeFromTop (theme::spacingSmall);
    hint.setBounds (area.removeFromTop (rowHeight * 2));
}

void ProjectSettingsDialog::show (services::ProjectSettingsService& settingsService,
                                  services::AutosaveService& autosave)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Project Settings");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (
        new ProjectSettingsDialog (settingsService, autosave)); // JUCE-API verlangt besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void ProjectSettingsDialog::applySettings()
{
    // Vom gespeicherten Stand ausgehen: Felder, die der Dialog nicht zeigt (z. B. die Zeitanzeige), bleiben.
    auto chosen = settings.getSettings();
    chosen.sampleRate = sampleRateBox.getSelectedId();
    chosen.recordFormat = formatForId (formatBox.getSelectedId());
    chosen.bitDepth = bitDepthBox.getSelectedId();
    chosen.audioRecordMode =
        audioModeBox.getSelectedId() == 2 ? services::AudioRecordMode::replace : services::AudioRecordMode::keepTakes;
    chosen.midiRecordMode =
        static_cast<services::MidiRecordMode> (juce::jlimit (0, 2, midiModeBox.getSelectedId() - 1));
    chosen.timecodeFps = fpsBox.getSelectedId();
    chosen.autoCrossfades = crossfadeToggle.getToggleState();

    if (const auto result = settings.setSettings (chosen); ! result)
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, TRANS ("Error"), services::errorText (result.error()));
}

void ProjectSettingsDialog::fillBitDepths (services::RecordFormat format, int selectedBits)
{
    bitDepthBox.clear (juce::dontSendNotification);
    const auto depths = services::ProjectSettingsService::getBitDepths (format);

    for (const auto bits : depths)
        bitDepthBox.addItem (bits == 32 ? TRANS ("32 bit float")
                                  : plugins::i18n::format (TRANS ("{bits} bit"), { { "bits", juce::String (bits) } }),
                           bits);

    // Gibt es die bisherige Tiefe im neuen Format nicht, die höchste mögliche nehmen.
    const bool available = std::ranges::find (depths, selectedBits) != depths.end();
    bitDepthBox.setSelectedId (available ? selectedBits : depths.back(), juce::dontSendNotification);
}

void ProjectSettingsDialog::switchDeviceRate()
{
    if (const auto result = settings.setDeviceSampleRate (sampleRateBox.getSelectedId()); ! result)
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, TRANS ("Sample rate"), services::errorText (result.error()));

    updateDeviceInfo();
}

void ProjectSettingsDialog::updateDeviceInfo()
{
    const auto deviceRate = settings.getDeviceSampleRate();

    if (deviceRate <= 0.0)
    {
        deviceInfo.setText (TRANS ("No audio device active"), juce::dontSendNotification);
        switchDeviceButton.setVisible (false);
        return;
    }

    const bool matches = std::abs (deviceRate - sampleRateBox.getSelectedId()) < 0.5;
    deviceInfo.setText (plugins::i18n::format (matches ? TRANS ("Device: {rate} Hz, matches") : TRANS ("Device: {rate} Hz, differs"),
                                               { { "rate", juce::String (juce::roundToInt (deviceRate)) } }),
                        juce::dontSendNotification);
    deviceInfo.setColour (juce::Label::textColourId, matches ? theme::textSecondary : theme::record);
    switchDeviceButton.setVisible (! matches);
}

} // namespace fk::ui
