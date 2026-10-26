#pragma once

#include "fk/services/MidiClipService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace fk::ui
{

/// Anschlagsstärke-Lane unter dem Notenraster: je Note ein Balken am Notenanfang, Höhe =
/// Velocity. Ziehen am Balken ändert die Velocity (Vorschau beim Ziehen, ein Undo-Schritt beim
/// Loslassen). Gehört der Balken zur Auswahl im Raster, ändern sich alle ausgewählten Noten um
/// denselben Betrag (wie Cubase). Rollt waagerecht mit dem Notenraster (setScrollX), gleiche
/// Skala (setPixelsPerBeat).
class VelocityLane final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int preferredHeight = 72;

    /// Eine Änderung wurde abgelehnt (zum Beispiel gebackene Spur); der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    explicit VelocityLane (services::MidiClipService& midiClipService);

    /// Auswahl im Notenraster: diese Balken sind hervorgehoben und ziehen gemeinsam.
    void setSelectedNotes (const std::vector<int>& noteIndices);

    void setClip (std::optional<services::MidiClipInfo> clip);
    void setPixelsPerBeat (double pixelsPerBeat);
    void setScrollX (int x);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static constexpr int barWidth = 7;
    static constexpr int topMargin = 6;

    struct Drag
    {
        int noteIndex = -1;
        int originalVelocity = 100;
        int velocity = 100;
        bool withSelection = false; ///< gegriffene Note gehört zur Auswahl: alle ausgewählten mit
    };

    [[nodiscard]] int shownVelocity (const services::EditableNote& note) const;

    [[nodiscard]] int xForBeat (double beat) const;
    [[nodiscard]] int velocityForY (int y) const;
    [[nodiscard]] int yForVelocity (int velocity) const;
    [[nodiscard]] const services::EditableNote* findNoteAt (juce::Point<int> position) const;

    services::MidiClipService& midiClips;
    std::optional<services::MidiClipInfo> clip;
    double pixelsPerBeat = 64.0;
    int scrollX = 0;
    std::optional<Drag> drag;
    std::set<int> selected;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelocityLane)
};

} // namespace fk::ui
