#pragma once

#include "fk/services/ProjectService.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Bearbeitungsverlauf als Liste (Cubase: Bearbeitungsprotokoll). Oben der Projektanfang, darunter
/// jeder Schritt; rückgängig gemachte Schritte stehen grau darunter. Klick auf eine Zeile springt
/// dorthin: Alles darüber ist ausgeführt, alles darunter rückgängig.
class UndoHistoryPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit UndoHistoryPanel (services::ProjectService& projectService);
    ~UndoHistoryPanel() override;

    void resized() override;

    /// Liest den Verlauf neu (der Timer tut das auch, Tests rufen es direkt).
    void refresh();

    /// Zeile wählen wie per Klick: 0 = Projektanfang, n = nach dem n-ten Schritt.
    void jumpTo (int row);

    [[nodiscard]] int getNumRows() override;
    [[nodiscard]] int getCurrentRow() const noexcept { return static_cast<int> (history.done.size()); }

    static void show (services::ProjectService& projectService);

private:
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& event) override;
    void timerCallback() override;

    services::ProjectService& project;
    services::ProjectService::UndoHistory history;
    juce::ListBox list;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoHistoryPanel)
};

} // namespace fk::ui
