#pragma once

#include "fk/services/MidiClipService.h"

#include <juce_graphics/juce_graphics.h>

#include <map>
#include <vector>

namespace fk::ui
{

/// Eine laufende Mausaktion im Notenraster als reine Rechnung, ohne Komponente: Aus der
/// Mausbewegung seit dem Druck entstehen Vorschau-Noten (verschoben, getrimmt, neuer Anschlag),
/// eine aufgezogene neue Note oder ein Auswahlrechteck. Das NoteGrid zeichnet die Vorschau und
/// gibt sie beim Loslassen an den MidiClipService.
class NoteDrag final
{
public:
    enum class Mode
    {
        none,
        move,      ///< ausgewählte Noten verschieben (Zeit und Tonhöhe)
        trimStart, ///< Anfang ziehen, Ende bleibt
        trimEnd,   ///< Ende ziehen, Anfang bleibt
        velocity,  ///< Anschlagsstärke nach oben/unten
        draw,      ///< neue Note mit dem Stift aufziehen
        lasso      ///< Auswahlrechteck
    };

    /// Raster und Maßstab des Notenrasters.
    struct Scale
    {
        double gridBeats = 0.25;
        double pixelsPerBeat = 64.0;
        int rowHeight = 12;
        int numNotes = 128;
    };

    /// Noten bearbeiten; anchorIndex ist die gegriffene Note, die am Raster einrastet.
    void beginEdit (Mode newMode, const std::vector<services::EditableNote>& notes, int anchorIndex, bool asCopy);
    void beginDraw (services::EditableNote note);
    void beginLasso (juce::Point<int> start);

    /// distance: Mausweg seit dem Druck in Pixeln (y nach unten); position: aktuelle Mausposition.
    void update (juce::Point<int> distance, juce::Point<int> position, const Scale& scale);

    [[nodiscard]] Mode getMode() const noexcept { return mode; }
    [[nodiscard]] bool isCopy() const noexcept { return copy; }
    [[nodiscard]] bool hasMoved() const noexcept { return moved; }
    [[nodiscard]] int getAnchorIndex() const noexcept { return anchor; }
    [[nodiscard]] const std::map<int, services::EditableNote>& getPreviews() const noexcept { return previews; }
    [[nodiscard]] const services::EditableNote& getDrawn() const noexcept { return drawn; }
    [[nodiscard]] juce::Rectangle<int> getLasso() const noexcept { return lasso; }

    /// Klick ohne Ziehen in eine Mehrfachauswahl wählt am Ende nur die gegriffene Note (wie Cubase).
    void setSelectOnlyOnClick (bool shouldSelectOnly) noexcept { selectOnlyOnClick = shouldSelectOnly; }
    [[nodiscard]] bool shouldSelectOnlyOnClick() const noexcept { return selectOnlyOnClick; }

private:
    static constexpr int pixelsPerVelocityStep = 2;
    static constexpr int moveThresholdPixels = 2;

    Mode mode = Mode::none;
    bool copy = false;
    bool moved = false;
    bool selectOnlyOnClick = false;
    int anchor = -1;
    std::map<int, services::EditableNote> originals; ///< Index → Stand beim Mausdruck
    std::map<int, services::EditableNote> previews;  ///< Index → Stand während des Ziehens
    services::EditableNote drawn;
    double drawnMinimumLength = 0.0;
    juce::Point<int> lassoStart;
    juce::Rectangle<int> lasso;
};

} // namespace fk::ui
