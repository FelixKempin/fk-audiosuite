#pragma once

#include "fk/services/PluginService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Ein Insert-Slot im Kanalzug: leer (Klick = Plugin wählen) oder belegt (Klick = Editor,
/// Rechtsklick = Umgehen/Entfernen). Tut selbst nichts am Modell; es ruft nur die Callbacks.
class InsertSlot final : public juce::Component
{
public:
    std::function<void()> onAddRequested;
    std::function<void()> onOpenRequested;
    std::function<void (bool shouldBeEnabled)> onEnabledChanged;
    std::function<void()> onRemoveRequested;

    /// Sidechain: mögliche Quellen (für das Menü) und die Wahl (leere Kennung = keine).
    std::function<std::vector<services::SidechainSource>()> getSidechainSources;
    std::function<void (const juce::String& sourceTrackId)> onSidechainChosen;

    /// Klick auf das Bake-Feld links im Slot (nur belegte Slots).
    std::function<void()> onBakeMarkClicked;

    /// Eine Aktion im Kontextmenü ging nicht (Presets); der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    /// Presets im Kontextmenü (eigene und Werks-Programme); ohne Service kein Preset-Menü.
    void setPresetSource (services::PluginService* service) noexcept { presetSource = service; }

    void setInsert (std::optional<services::InsertInfo> newInsert);

    /// Pegelabsenkung (≤ 0 dB) eines Dynamik-Plugins; leer = kein Meter.
    void setMeterDb (std::optional<float> gainReductionDb);

    /// Markiert für "Bake Inserts" (Inspector); ohne Rückruf onBakeMarkClicked kein Feld.
    void setBakeMarked (bool marked);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static constexpr int bakeMarkWidth = 14;

    void showContextMenu();
    void addPresetMenu (juce::PopupMenu& menu, std::vector<juce::String>& userPresets, std::vector<juce::String>& programs);
    void handlePresetChoice (int result, const std::vector<juce::String>& userPresets);
    void askForPresetName();
    void report (const core::Result<void>& result) const;
    [[nodiscard]] juce::Rectangle<int> bakeMarkArea() const;

    std::optional<services::InsertInfo> insert;
    std::optional<float> meterDb;
    bool bakeMarked = false;
    services::PluginService* presetSource = nullptr;
};

} // namespace fk::ui
