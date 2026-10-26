#pragma once

#include "fk/services/LibraryService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace fk::ui
{

/// Instrumenten- und Preset-Browser (Reiter „Instrumente“): Plugin-Instrumente, Instrumente aus den
/// Bibliotheken und eigene Presets in einer Liste mit Suche, Tag-Filter und Favoriten. Doppelklick
/// legt eine Spur mit dem Instrument an.
class InstrumentBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void (const juce::String& message)> onError;
    std::function<void()> onOpenContentManager;

    explicit InstrumentBrowser (services::LibraryService& libraryService);
    ~InstrumentBrowser() override;

    /// Liste neu lesen (nach Installation, Scan, neuem Preset).
    void refresh();

    /// Für Tests: Suche, Tag, Favoriten setzen; Zeilen lesen; Spur anlegen wie ein Doppelklick.
    void setSearchText (const juce::String& text);
    void setTagFilter (const juce::String& tag);
    void setFavouritesOnly (bool only);
    [[nodiscard]] const std::vector<services::LibraryItem>& getItems() const noexcept { return items; }
    void createTrackForRow (int row);
    void toggleFavourite (int row);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void visibilityChanged() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& event) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override;
    void showItemMenu (int row);
    void askForTag (int row);
    void rebuildTags();
    void report (const core::Result<void>& result);

    services::LibraryService& library;
    std::vector<services::LibraryItem> items;

    juce::TextEditor search;
    juce::ComboBox tagBox;
    IconButton favouritesButton;
    IconButton newTrackButton;
    IconButton contentButton;
    juce::ListBox list;
    juce::Label emptyHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentBrowser)
};

} // namespace fk::ui
