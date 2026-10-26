#pragma once

#include "fk/services/MidiClipService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TimelineService.h"
#include "fk/ui/NoteDrag.h"
#include "fk/ui/NoteEditing.h"
#include "fk/ui/NoteGridBackground.h"
#include "fk/ui/Tool.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Notenraster des Key Editors: Zeit nach rechts (Schläge), Tonhöhe nach oben. Bedienung wie
/// in Cubase:
/// - Klick wählt eine Note, Strg+Klick schaltet um, Shift+Klick nimmt dazu, Ziehen ins Leere
///   (Auswahlwerkzeug) zieht ein Auswahlrechteck auf.
/// - Ziehen verschiebt alle ausgewählten Noten, mit Alt als Kopie; die Ränder trimmen Anfang
///   oder Ende; Strg+Shift+Ziehen nach oben/unten ändert die Anschlagsstärke.
/// - Stift: Klick setzt eine Note in der eingestellten Länge, Ziehen nach rechts verlängert sie.
/// - Tasten: Entf löscht, Strg+A wählt alles, Pfeil hoch/runter transponiert (Shift = Oktave),
///   Pfeil links/rechts verschiebt um einen Rasterschritt.
/// Jede Aktion ist ein Undo-Schritt im MidiClipService; die Auswahl überlebt die Änderung.
class NoteGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int rowHeight = 12;
    static constexpr int numNotes = 128;

    /// Die Auswahl hat sich geändert (Velocity-Lane hebt dieselben Noten hervor).
    std::function<void()> onSelectionChanged;

    /// Vorhören: Note anschlagen (Klick auf Note, Setzen, beim Ziehen neue Tonhöhe) und beenden.
    std::function<void (int noteNumber, int velocity)> onPreviewNote;
    std::function<void()> onPreviewStop;

    /// Eine Änderung wurde abgelehnt (zum Beispiel gebackene Spur); der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    NoteGrid (services::MidiClipService& midiClipService, services::TimelineService& timelineService);

    void setClip (std::optional<services::MidiClipInfo> clip);

    /// Tonart am Clip-Anfang: Zeilen außerhalb der Skala werden gedimmt.
    void setKey (services::KeyPointInfo key);

    /// Akkord, den Alt+Klick mit dem Stift an der Mausposition einfügt (Oktave aus der Zeile).
    struct ChordChoice
    {
        int degree = 1;
        services::ChordKind kind = services::ChordKind::triad;
        int inversion = 0;
    };
    void setChordChoice (ChordChoice choice);
    void setTool (Tool tool);

    /// Raster: Einrasten beim Setzen und Ziehen, Quantisieren.
    void setGridBeats (double gridBeats);

    /// Länge neuer Noten in Schlägen.
    void setNoteLengthBeats (double lengthBeats);
    void setPixelsPerBeat (double pixelsPerBeat);

    /// Positionszeiger in Schlägen ab Clip-Anfang; leer = außerhalb des Clips (nicht zeichnen).
    void setPlayheadBeats (std::optional<double> beats);

    /// Step-Eingabe: Cursor, an dem die nächste Note entsteht (leer = aus).
    void setStepCursorBeats (std::optional<double> beats);

    /// Zeichen der Spielweisen (Kennung → Zeichen), auf den Notenbalken gezeigt.
    void setArticulationSymbols (std::map<juce::String, juce::String> symbols);
    [[nodiscard]] double getPixelsPerBeat() const noexcept;

    /// Indizes der ausgewählten Noten (Stand des aktuellen Clips).
    [[nodiscard]] std::vector<int> getSelectedNotes() const;
    void selectAll();

    void resized() override;
    void paintOverChildren (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& keyPress) override;

private:
    void updateSize();
    void updateBackground();
    void paintNotes (juce::Graphics& g) const;
    [[nodiscard]] juce::Rectangle<int> rectForNote (const services::EditableNote& note) const;
    [[nodiscard]] const services::EditableNote* findNoteAt (juce::Point<int> position) const;
    [[nodiscard]] NoteDrag::Mode modeAt (const services::EditableNote& note, juce::Point<int> position) const;
    [[nodiscard]] NoteDrag::Scale getScale() const noexcept;
    [[nodiscard]] double beatAt (int x) const;
    [[nodiscard]] double snappedBeat (double beat) const;
    [[nodiscard]] double floorToGrid (int x) const;
    [[nodiscard]] int noteNumberAt (int y) const;
    void commitDrag();
    void addChordAt (juce::Point<int> position);
    void previewNote (int noteNumber, int velocity);

    services::TimelineService& timeline;
    NoteEditing editing;
    NoteGridBackground background;
    services::KeyPointInfo key;
    ChordChoice chordChoice;
    Tool tool = Tool::draw;
    double gridBeats = 0.25;
    double noteLengthBeats = 0.25;
    double pixelsPerBeat = 64.0;
    NoteDrag drag;
    std::optional<double> playheadBeats;
    std::optional<double> stepCursorBeats;
    std::map<juce::String, juce::String> articulationSymbols;
    int previewedNote = -1; ///< zuletzt angeschlagene Tonhöhe beim Ziehen
};

} // namespace fk::ui
