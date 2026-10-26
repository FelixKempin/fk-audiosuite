#pragma once

#include "fk/services/MidiClipService.h"

#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace fk::ui
{

/// Auswahl und Bearbeitungsbefehle des Key Editors, ohne Darstellung: Das NoteGrid übersetzt
/// Maus und Tastatur in diese Befehle und zeichnet den Stand. Jede Änderung ist ein Undo-Schritt
/// im MidiClipService. Nach einer Änderung kennt man die neuen Indizes noch nicht; die geänderten
/// Noten werden beim nächsten setClip wiedergefunden und bleiben ausgewählt.
class NoteEditing final
{
public:
    std::function<void()> onSelectionChanged;

    /// Eine Änderung wurde abgelehnt (zum Beispiel gebackene Spur); der Text ist für den Benutzer.
    std::function<void (const juce::String& message)> onError;

    explicit NoteEditing (services::MidiClipService& midiClipService);

    void setClip (std::optional<services::MidiClipInfo> newClip);
    [[nodiscard]] const std::optional<services::MidiClipInfo>& getClip() const noexcept;

    [[nodiscard]] bool isSelected (int noteIndex) const;
    [[nodiscard]] const std::set<int>& getSelection() const noexcept;
    [[nodiscard]] std::vector<services::EditableNote> getSelectedNotes() const;
    void setSelection (std::set<int> indices);
    void selectOnly (int noteIndex);
    void toggle (int noteIndex);
    void add (int noteIndex);
    void selectAll();

    void addNotes (const std::vector<services::EditableNote>& notes, const juce::String& actionName);
    void updateNotes (const std::vector<services::EditableNote>& notes, const juce::String& actionName);
    void removeNotes (const std::vector<int>& noteIndices);
    void removeSelection();

    /// Ändert jede ausgewählte Note mit change (Transponieren, Verschieben ...), ein Undo-Schritt.
    void changeSelection (const std::function<void (services::EditableNote&)>& change, const juce::String& actionName);

    void addChord (const std::vector<int>& noteNumbers, double startBeats, double lengthBeats, int velocity);

private:
    void handle (const core::Result<void>& result);

    services::MidiClipService& midiClips;
    std::optional<services::MidiClipInfo> clip;
    std::set<int> selected;
    std::vector<services::EditableNote> expectedAfterEdit;
};

} // namespace fk::ui
