#pragma once

#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

namespace fk::ui
{

/// Controller-Lane unter dem Notenraster: zeigt die Ereignisse einer Art (Sustain, Mod-Wheel,
/// Pitch-Bend, Aftertouch oder ein frei gewählter CC) als Stufenlinie mit Punkten. Klick ins
/// Leere setzt einen Punkt, Ziehen zeichnet (ein Punkt je Rasterschritt, alte im Bereich weg),
/// Ziehen an einem Punkt verschiebt ihn, Rechtsklick löscht ihn. Rollt mit dem Raster wie die
/// Velocity-Lane. Die Art wählt eine Liste links oben, Minus entfernt die Lane.
class ControllerLane final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int preferredHeight = 72;

    /// Breite des Kopfs links: gleich der Klaviatur, damit die Zeitachse bündig unter den Noten liegt.
    static constexpr int headerWidth = 56;

    /// Minus-Button gedrückt: Der Key Editor entfernt diese Lane.
    std::function<void()> onRemoveRequested;

    /// Eine Änderung wurde abgelehnt; der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    explicit ControllerLane (services::MidiControllerService& midiControllerService);

    void setClip (std::optional<services::MidiClipInfo> clip);
    void setPixelsPerBeat (double pixelsPerBeat);
    void setScrollX (int x);
    void setGridBeats (double gridBeats);

    void setControllerType (int type);
    [[nodiscard]] int getControllerType() const noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static constexpr int typeBoxWidth = 170;
    static constexpr int pointSize = 7;
    static constexpr int topMargin = 6;

    /// Laufende Mausaktion: einen Punkt ziehen oder frei zeichnen.
    struct Drag
    {
        bool drawing = false;
        int eventIndex = -1; // beim Verschieben
        double beats = 0.0;
        int value = 0;
        double drawFromBeats = 0.0; // beim Zeichnen: Bereich, der neu entsteht
        double drawToBeats = 0.0;
        std::vector<std::pair<double, int>> drawn; // gezeichnete Punkte (Vorschau)
    };

    [[nodiscard]] juce::Rectangle<int> plotArea() const;
    [[nodiscard]] int xForBeat (double beat) const;
    [[nodiscard]] double beatForX (int x) const;
    [[nodiscard]] int valueForY (int y) const;
    [[nodiscard]] int yForValue (int value) const;
    [[nodiscard]] const services::ControllerEvent* findEventAt (juce::Point<int> position) const;
    void rebuildTypeChoices();
    void reportError (const core::Error& error);

    services::MidiControllerService& midiControllers;
    std::optional<services::MidiClipInfo> clip;
    int controllerType = services::controllerTypes::sustain;
    double pixelsPerBeat = 64.0;
    double gridBeats = 0.25;
    int scrollX = 0;
    std::optional<Drag> drag;
    juce::ComboBox typeBox;
    IconButton removeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerLane)
};

} // namespace fk::ui
