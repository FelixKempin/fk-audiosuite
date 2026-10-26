#pragma once

#include "fk/services/MixerService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SendService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/InsertSlot.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/SendRow.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace fk::ui
{

/// Ein Kanalzug der MixConsole: Name, Inserts, Pan, Fader mit Meter, Mute/Solo.
class ChannelStrip final : public juce::Component, private juce::Timer
{
public:
    static constexpr int numInsertSlots = 4;

    /// Wird gerufen, wenn der Nutzer die Oberfläche eines Inserts öffnen will.
    std::function<void (const juce::String& insertId)> onOpenInsertEditor;

    /// Eine Aktion wurde abgelehnt (zum Beispiel gebackener Insert); Text für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    ChannelStrip (services::TrackService& trackService,
                  services::MixerService& mixerService,
                  services::PluginService& pluginService,
                  services::SendService& sendService,
                  services::TrackHandle& track,
                  juce::Colour trackColour);
    ~ChannelStrip() override;

    /// Liest Name, Mute/Solo, Lautstärke, Pan und Inserts neu aus den Services.
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void choosePluginForSlot();
    void refreshSends();
    void reportError (const core::Error& error) const;

    services::TrackService& tracks;
    services::MixerService& mixer;
    services::PluginService& plugins;
    services::SendService& sends;
    services::TrackHandle& track;
    juce::Colour colour;

    juce::Label nameLabel;
    std::array<InsertSlot, numInsertSlots> insertSlots;
    juce::Slider panSlider;
    juce::Slider trimKnob;        ///< Eingangs-Trim vor den Inserts
    juce::TextButton phaseButton; ///< Phasenumkehr
    juce::Slider volumeSlider;
    LevelMeter meter;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::ComboBox outputBox;
    std::vector<std::unique_ptr<SendRow>> sendRows;
    std::vector<services::OutputInfo> outputs;
    bool isGroup = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace fk::ui
