#pragma once

#include "fk/services/MixerService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/LevelMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace fk::ui
{

/// Was der Bake-Button im Spurkopf auslöst (ADR 0009); das Projektfenster ruft den BakeService.
enum class BakeAction
{
    bakeTrack,   ///< Instrument zu Audio, Inserts bleiben live
    bakeInserts, ///< Instrument und alle Inserts (Gruppe: samt zuführender Spuren)
    unbake       ///< alles wieder live und bearbeitbar
};

/// Spurkopf links neben der Spur: Name, Eingangswahl, Rec/Mute/Solo, Pegel.
/// Hält die Spur nur als Handle und ändert sie ausschließlich über die Services.
class TrackHeader final : public juce::Component
{
public:
    /// Wird gerufen, wenn der Nutzer in den Kopf klickt (Spur auswählen).
    std::function<void()> onSelected;

    /// Automationsspur ein- oder ausblenden (Button mit Kurvensymbol).
    std::function<void (bool shown)> onAutomationToggled;

    /// Plugin-Fenster des Spurinstruments öffnen (Button nur auf Instrumentenspuren).
    std::function<void()> onOpenInstrument;

    /// Eintrag im Bake-Menü gewählt.
    std::function<void (BakeAction action)> onBakeRequested;

    /// Spur am Kopf ziehen: Zeiger bewegt (Bildschirm-y) und losgelassen. Das Arrangement zeigt die
    /// Einfügelinie und verschiebt die Spur.
    std::function<void (int screenY)> onReorderDrag;
    std::function<void (int screenY)> onReorderDrop;

    /// Abgelehnte Aktion (z. B. gebackene Spur duplizieren), als Hinweis anzeigen.
    std::function<void (const juce::String& message)> onError;

    TrackHeader (services::TrackService& trackService,
                 services::MixerService& mixerService,
                 services::TrackHandle& track);
    ~TrackHeader() override;

    void setSelected (bool shouldBeSelected);
    void setAutomationShown (bool shown);

    /// Liest den aktuellen Zustand aus dem Service und aktualisiert alle Bedienelemente.
    void refresh();

    /// Pegel neu lesen; der ArrangeView ruft das für alle Köpfe aus einem Timer (statt je Kopf einer).
    void updateMeter();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    void rebuildInputChoices (const services::TrackInfo& info);
    void showBakeMenu();
    void showContextMenu();
    [[nodiscard]] bool isDragHandle (const juce::MouseEvent& event) const;

    services::TrackService& tracks;
    services::MixerService& mixer;
    services::TrackHandle&
        track; // Handle: gehört dem Edit, lebt länger als dieser Header (ArrangeView baut bei Änderungen neu)
    juce::Colour colour;
    bool selected = false;
    bool reordering = false;

    juce::Label nameLabel;
    juce::ComboBox inputBox;
    juce::TextButton armButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    IconButton automationButton;
    IconButton instrumentButton;
    IconButton bakeButton;
    LevelMeter meter;
    services::TrackInfo lastInfo; ///< Stand des letzten refresh() für Menü und Zeichnen
    std::vector<services::AudioInputInfo> inputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};

} // namespace fk::ui
