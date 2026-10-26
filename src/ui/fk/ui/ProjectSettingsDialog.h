#pragma once

#include "fk/services/AutosaveService.h"
#include "fk/services/ProjectSettingsService.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Projekteinstellungen: Samplerate, Aufnahmeformat, Bittiefe, Aufnahmemodi, Timecode, dazu das Intervall der
/// automatischen Sicherung (gilt für alle Projekte). Jede Änderung wirkt sofort und ist ein Undo-Schritt.
class ProjectSettingsDialog final : public juce::Component
{
public:
    ProjectSettingsDialog (services::ProjectSettingsService& settingsService, services::AutosaveService& autosave);

    void resized() override;

    static void show (services::ProjectSettingsService& settingsService, services::AutosaveService& autosave);

private:
    void applySettings();
    void fillBitDepths (services::RecordFormat format, int selectedBits);
    void switchDeviceRate();
    void updateDeviceInfo();

    services::ProjectSettingsService& settings;
    services::AutosaveService& autosave;

    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateBox;
    juce::Label deviceInfo;
    juce::TextButton switchDeviceButton;
    juce::Label formatLabel;
    juce::ComboBox formatBox;
    juce::Label bitDepthLabel;
    juce::ComboBox bitDepthBox;
    juce::Label audioModeLabel;
    juce::ComboBox audioModeBox;
    juce::Label midiModeLabel;
    juce::ComboBox midiModeBox;
    juce::Label fpsLabel;
    juce::ComboBox fpsBox;
    juce::ToggleButton crossfadeToggle;
    juce::Label autosaveLabel;
    juce::ComboBox autosaveBox;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSettingsDialog)
};

} // namespace fk::ui
