#pragma once

#include "fk/services/PluginService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace fk::ui
{

/// "Meine Plugins" im Plugin-Manager: alle Plugins als Tabelle. Klick auf den Stern macht ein Plugin
/// zum Favoriten, Klick in "Ausgeblendet" nimmt es aus den Menüs, die Kategorie ist wählbar.
class PluginLibraryPanel final : public juce::Component, private juce::TableListBoxModel
{
public:
    enum Column
    {
        favouriteColumn = 1,
        nameColumn,
        vendorColumn,
        formatColumn,
        groupColumn,
        hiddenColumn
    };

    explicit PluginLibraryPanel (services::PluginService& pluginService);
    ~PluginLibraryPanel() override;

    void resized() override;
    void visibilityChanged() override;

    /// Liest die Liste neu (nach einem Scan oder einer Änderung).
    void refresh();
    void setSearchText (const juce::String& text);

    /// Wie ein Klick in die Zelle; für Tests.
    void clickCell (int row, int column);
    [[nodiscard]] int getNumRows() override;
    [[nodiscard]] const services::PluginInfo& getRow (int row) const { return rows[static_cast<std::size_t> (row)]; }

private:
    void paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent& event) override;
    juce::Component* refreshComponentForCell (int row, int column, bool selected, juce::Component* existing) override;
    void report (const core::Result<void>& result);

    services::PluginService& plugins;
    std::vector<services::PluginInfo> rows;
    juce::TextEditor search;
    juce::TableListBox table;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginLibraryPanel)
};

} // namespace fk::ui
