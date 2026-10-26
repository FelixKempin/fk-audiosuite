#include "fk/ui/AudioSettingsComponent.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int maxChannels = 64;
constexpr double minFrequencyHz = 50.0;
constexpr double maxFrequencyHz = 2000.0;
constexpr double defaultFrequencyHz = 440.0;
} // namespace

AudioSettingsComponent::AudioSettingsComponent (engine::EngineContext& context, services::MidiService& midiService)
    : engineContext (context)
    , midi (midiService)
    , deviceSelector (context.getAudioDeviceManager(), 0, maxChannels, 1, maxChannels, false, false, false, false)
    , toneButton (TRANS ("Test tone"))
    , rescanButton (TRANS ("Rescan"))
{
    // MIDI-Geräte verwaltet Tracktion selbst (JUCE-Auswahl zeigt sie deshalb nicht): eigene Liste.
    midiTitle.setText (TRANS ("MIDI inputs"), juce::dontSendNotification);
    midiTitle.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    rescanButton.setTooltip (
        TRANS ("Rescan hardware right away (the DAW also detects plugging in and out by itself)"));
    rescanButton.onClick = [this] { midi.rescanDevices(); };
    midiEmptyLabel.setText (
        TRANS ("No MIDI device found. Connect a keyboard or interface, then click \"Rescan\"."),
        juce::dontSendNotification);
    midiEmptyLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (midiTitle);
    addAndMakeVisible (rescanButton);
    addChildComponent (midiEmptyLabel);
    midi.addListener (*this);
    rebuildMidiList();
    // Der Testton hängt sich zusätzlich an die Geräteverwaltung; Tracktion läuft parallel weiter.
    sourcePlayer.setSource (&toneSource);
    engineContext.getAudioDeviceManager().addAudioCallback (&sourcePlayer);

    toneButton.setClickingTogglesState (true);
    toneButton.onClick = [this] { toneSource.setEnabled (toneButton.getToggleState()); };

    frequencySlider.setRange (minFrequencyHz, maxFrequencyHz);
    frequencySlider.setSkewFactorFromMidPoint (defaultFrequencyHz);
    frequencySlider.setValue (defaultFrequencyHz, juce::dontSendNotification);
    frequencySlider.setTextValueSuffix (" Hz");
    frequencySlider.onValueChange = [this] { toneSource.setFrequency (frequencySlider.getValue()); };

    frequencyLabel.setText (TRANS ("Frequency"), juce::dontSendNotification);
    frequencyLabel.attachToComponent (&frequencySlider, true);

    addAndMakeVisible (deviceSelector);
    addAndMakeVisible (toneButton);
    addAndMakeVisible (frequencySlider);
    addAndMakeVisible (frequencyLabel);

    setSize (560, 760);
}

AudioSettingsComponent::~AudioSettingsComponent()
{
    midi.removeListener (*this);
    engineContext.getAudioDeviceManager().removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
}

void AudioSettingsComponent::midiDevicesChanged()
{
    rebuildMidiList();
}

void AudioSettingsComponent::rebuildMidiList()
{
    midiToggles.clear();
    midiDeviceIds.clear();

    for (const auto& device : midi.getInputDevices())
    {
        auto toggle = std::make_unique<juce::ToggleButton> (device.name);
        toggle->setToggleState (device.enabled, juce::dontSendNotification);
        toggle->setTooltip (device.isVirtual
                                ? TRANS ("Virtual device of the DAW")
                                : TRANS ("Hardware input: on = tracks can choose it"));
        toggle->onClick = [this, id = device.id, raw = toggle.get()]
        { midi.setInputDeviceEnabled (id, raw->getToggleState()); };
        addAndMakeVisible (*toggle);
        midiDeviceIds.push_back (device.id);
        midiToggles.push_back (std::move (toggle));
    }

    midiEmptyLabel.setVisible (midiToggles.empty());
    resized();
}

void AudioSettingsComponent::resized()
{
    constexpr int rowHeight = 28;
    constexpr int labelWidth = 80;

    auto area = getLocalBounds().reduced (theme::spacingLarge);

    auto bottomRow = area.removeFromBottom (rowHeight);
    toneButton.setBounds (bottomRow.removeFromLeft (120));
    bottomRow.removeFromLeft (labelWidth + theme::spacingLarge);
    frequencySlider.setBounds (bottomRow);

    area.removeFromBottom (theme::spacingLarge);

    // MIDI-Liste über dem Testton: Titel mit Button, dann eine Zeile je Gerät (mindestens eine für den Hinweis).
    const int midiRows = juce::jmax (1, static_cast<int> (midiToggles.size()));
    auto midiArea = area.removeFromBottom (rowHeight + midiRows * 24 + theme::spacingMedium);
    auto titleRow = midiArea.removeFromTop (rowHeight);
    rescanButton.setBounds (titleRow.removeFromRight (110));
    midiTitle.setBounds (titleRow);
    midiEmptyLabel.setBounds (midiArea.withHeight (24));

    for (auto& toggle : midiToggles)
        toggle->setBounds (midiArea.removeFromTop (24));

    area.removeFromBottom (theme::spacingMedium);
    deviceSelector.setBounds (area);
}

} // namespace fk::ui
