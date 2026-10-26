#pragma once

#include "fk/services/MidiClipService.h"
#include "fk/services/TimelineService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

namespace fk::ui
{

/// Listen-Editor (Cubase: Listen-Editor): alle Noten eines MIDI-Clips als Tabelle. Doppelklick auf
/// eine Zelle bearbeitet Position (Takt.Schlag.Tick), Note (C3 oder 60), Länge (Schläge) und Anschlag.
class ListEditor final : public juce::Component, private juce::TableListBoxModel, private services::MidiClipService::Listener
{
public:
    enum Column
    {
        positionColumn = 1,
        noteColumn,
        lengthColumn,
        velocityColumn
    };

    std::function<void (const juce::String& message)> onError;

    ListEditor (services::MidiClipService& midiClipService, services::TimelineService& timelineService);
    ~ListEditor() override;

    void setClip (const juce::String& clipId);
    [[nodiscard]] juce::String getClipId() const { return clipId; }

    /// Wie eine Eingabe in die Zelle; für Tests. Liefert false bei ungültigem Text.
    bool editCell (int row, int column, const juce::String& text);
    [[nodiscard]] juce::String getCellText (int row, int column) const;
    [[nodiscard]] int getNumRows() override;
    void selectRow (int row);
    void deleteSelected();

    /// Notenname oder Nummer lesen: "C3" = 60 (Mitte C = C3), "F#2", "Bb4", "64".
    [[nodiscard]] static std::optional<int> parseNoteName (const juce::String& text);

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected) override;
    juce::Component* refreshComponentForCell (int row, int column, bool selected, juce::Component* existing) override;
    void deleteKeyPressed (int lastRowSelected) override;
    void midiClipChanged() override;
    void reload();
    void reportError (const core::Error& error);

    services::MidiClipService& midiClips;
    services::TimelineService& timeline;
    juce::String clipId;
    double clipStartSeconds = 0.0;
    double clipStartBeats = 0.0;
    std::vector<services::EditableNote> notes; ///< nach Anfang sortiert, wie angezeigt

    juce::Label titleLabel;
    IconButton deleteButton;
    juce::TableListBox table;
    juce::Label emptyHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ListEditor)
};

} // namespace fk::ui
