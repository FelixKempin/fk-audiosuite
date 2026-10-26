#pragma once

#include "fk/services/MixerService.h"
#include "fk/services/PluginService.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/RackService.h"
#include "fk/services/SendService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/InsertSlot.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/SendRow.h"
#include "fk/ui/TrackHeader.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>

namespace fk::ui
{

/// Linke Zone (UI_CONCEPT.md §2): zeigt die ausgewählte Spur vollständig. Name, Art, Eingang,
/// Ausgang (Master oder Gruppe), Inserts, Pan, Fader mit Pegel, Rec/Mute/Solo.
/// Hält die Spur als Handle und ändert sie nur über die Services.
class Inspector final : public juce::Component,
                        private services::PluginService::Listener,
                        private services::SendService::Listener,
                        private services::MixerService::Listener,
                        private juce::Timer
{
public:
    static constexpr int numInsertSlots = 8;

    std::function<void (const juce::String& insertId)> onOpenInsertEditor;

    /// Plugin-Fenster des Spurinstruments öffnen (Button oder Doppelklick auf den Namen).
    std::function<void (const juce::String& instrumentId)> onOpenInstrument;

    /// Bake-Buttons: insertCount = markierte Slots von vorn (-1 = alle Inserts).
    std::function<void (services::TrackHandle* track, BakeAction action, int insertCount)> onBakeRequested;

    /// Eine Änderung wurde abgelehnt (zum Beispiel gebackener Slot); der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    Inspector (services::TrackService& trackService,
               services::MixerService& mixerService,
               services::PluginService& pluginService,
               services::SendService& sendService,
               services::RackService& rackService,
               services::ExternalMidiService& externalMidiService);
    ~Inspector() override;

    /// nullptr = keine Spur ausgewählt.
    void setTrack (services::TrackHandle* track);
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    // Änderungen aus anderen Ansichten (MixConsole, Undo, Automation) sofort zeigen.
    void insertsChanged() override;
    void sendsChanged() override;
    void mixerChanged() override;
    void timerCallback() override;
    void rebuildInputChoices (const services::TrackInfo& info);
    void rebuildOutputChoices (const services::TrackInfo& info);
    void refreshInserts();
    void refreshSends();
    void updateInsertMeters();
    void followAutomation();
    void choosePluginForSlot();
    void chooseInstrument();
    void reportError (const core::Error& error) const;
    void toggleBakeMark (int slotNumber);
    void refreshBake (const services::TrackInfo& info);
    void refreshRack();
    void applyProgram();
    void showOutputsMenu();

    services::TrackService& tracks;
    services::MixerService& mixer;
    services::PluginService& plugins;
    services::SendService& sends;
    services::RackService& racks;
    services::ExternalMidiService& externalMidi;
    services::TrackHandle* track = nullptr; // nur geliehen; ArrangeView meldet Wechsel und Löschung

    juce::Label nameLabel;
    juce::Label kindLabel;
    juce::Label instrumentLabel;     ///< "Instrument"
    juce::Label instrumentNameLabel; ///< Name, Doppelklick öffnet
    IconButton openInstrumentButton;
    IconButton replaceInstrumentButton;
    juce::Label rackLabel;           ///< "Rack-Instrument: Drums" bzw. "Ausgang 3+4 von Drums"
    juce::ComboBox midiChannelBox;   ///< MIDI-Kanal einer MIDI-Spur am Rack
    IconButton addRackMidiButton;    ///< weitere MIDI-Spur an dieses Rack
    IconButton rackOutputsButton;    ///< Ausgänge aktivieren
    juce::ComboBox programBox;       ///< Programmwechsel einer MIDI-Spur an einem Gerät
    juce::Slider bankSlider;         ///< Bank dazu (-1 = keine)
    juce::Label inputLabel;
    juce::ComboBox inputBox;
    juce::Label outputLabel;
    juce::ComboBox outputBox;
    juce::Label insertsLabel;
    std::array<InsertSlot, numInsertSlots> insertSlots;
    juce::Label bakeLabel;
    IconButton bakeTrackButton;
    IconButton bakeInsertsButton;
    IconButton unbakeButton;
    int bakeMarkCount = 0; ///< Slots 1..n für "Bake Inserts" markiert (linear), 0 = alle
    juce::Label sendsLabel;
    std::vector<std::unique_ptr<SendRow>> sendRows;
    juce::Slider panSlider;
    juce::Slider trimSlider;      ///< Eingangs-Trim vor den Inserts
    juce::TextButton phaseButton; ///< Phasenumkehr
    juce::Slider volumeSlider;
    LevelMeter meter;
    juce::TextButton armButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;

    std::vector<services::AudioInputInfo> inputs;
    std::vector<services::OutputInfo> outputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};

} // namespace fk::ui
