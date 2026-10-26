#include "fk/ui/PluginLibraryPanel.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int automaticGroupId = 1; // ComboBox-ID "automatisch"; die Kategorien folgen ab 2

/// Kategorie-Auswahl in der Tabelle; merkt sich, zu welchem Plugin sie gerade gehört.
class GroupBox final : public juce::ComboBox
{
public:
    juce::String pluginId;
};
} // namespace

PluginLibraryPanel::PluginLibraryPanel (services::PluginService& pluginService)
    : plugins (pluginService)
{
    search.setTextToShowWhenEmpty (TRANS ("Search plugin, vendor or category"), theme::textSecondary);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    auto& header = table.getHeader();
    header.addColumn (juce::String::fromUTF8 ("★"), favouriteColumn, 32, 32, 32, juce::TableHeaderComponent::notResizable);
    header.addColumn (TRANS ("Name"), nameColumn, 220);
    header.addColumn (TRANS ("Vendor"), vendorColumn, 150);
    header.addColumn (TRANS ("Format"), formatColumn, 60);
    header.addColumn (TRANS ("Category"), groupColumn, 150);
    header.addColumn (TRANS ("Hidden"), hiddenColumn, 90);
    table.setModel (this);
    table.setRowHeight (26);
    table.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    table.setTooltip (TRANS (juce::String::fromUTF8 ("Click ★: favorite (at the top of the plugin menu). Click Hidden: missing from the menus, but stays in projects that use it.")));
    addAndMakeVisible (table);

    hint.setColour (juce::Label::textColourId, theme::textSecondary);
    hint.setText (TRANS ("Favorites are at the very top of the plugin menu, below them the categories and all plugins by vendor. New plugins are found by the \"Search and Scan\" tab."),
                  juce::dontSendNotification);
    addAndMakeVisible (hint);

    refresh();
}

PluginLibraryPanel::~PluginLibraryPanel()
{
    table.setModel (nullptr);
}

void PluginLibraryPanel::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingMedium);
    search.setBounds (area.removeFromTop (28));
    area.removeFromTop (theme::spacingSmall);
    hint.setBounds (area.removeFromBottom (36));
    table.setBounds (area);
}

void PluginLibraryPanel::visibilityChanged()
{
    if (isVisible())
        refresh(); // nach einem Scan im anderen Reiter
}

void PluginLibraryPanel::refresh()
{
    const auto text = search.getText().trim();
    rows.clear();

    for (auto& plugin : plugins.getAllPlugins())
        if (text.isEmpty() || plugin.name.containsIgnoreCase (text) || plugin.manufacturer.containsIgnoreCase (text) ||
            plugin.group.containsIgnoreCase (text) || plugin.format.containsIgnoreCase (text))
            rows.push_back (std::move (plugin));

    table.updateContent();
    table.repaint();
}

void PluginLibraryPanel::setSearchText (const juce::String& text)
{
    search.setText (text, false);
    refresh();
}

int PluginLibraryPanel::getNumRows()
{
    return static_cast<int> (rows.size());
}

void PluginLibraryPanel::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    if (selected)
        g.fillAll (theme::accentSoft);
    else if (row % 2 == 1)
        g.fillAll (theme::backgroundElevated.withAlpha (0.4f));
}

void PluginLibraryPanel::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool)
{
    if (row < 0 || row >= static_cast<int> (rows.size()))
        return;

    const auto& plugin = rows[static_cast<std::size_t> (row)];
    g.setColour (plugin.hidden ? theme::textDisabled : theme::textPrimary);
    g.setFont (juce::FontOptions (13.0f));

    switch (column)
    {
        case favouriteColumn:
            g.setColour (plugin.favourite ? theme::solo : theme::textDisabled);
            g.setFont (juce::FontOptions (16.0f));
            g.drawText (juce::String::fromUTF8 (plugin.favourite ? "★" : "☆"), 0, 0, width, height, juce::Justification::centred);
            break;
        case nameColumn:
            g.drawText (plugin.name, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;
        case vendorColumn:
            g.drawText (plugin.manufacturer, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;
        case formatColumn:
            g.drawText (plugin.format, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;
        case hiddenColumn:
            g.drawText (plugin.hidden ? TRANS ("hidden") : TRANS ("visible"), 6, 0, width - 8, height,
                        juce::Justification::centredLeft, true);
            break;
        default:
            break;
    }
}

void PluginLibraryPanel::cellClicked (int row, int column, const juce::MouseEvent&)
{
    clickCell (row, column);
}

void PluginLibraryPanel::clickCell (int row, int column)
{
    if (row < 0 || row >= static_cast<int> (rows.size()))
        return;

    const auto& plugin = rows[static_cast<std::size_t> (row)];

    if (column == favouriteColumn)
        report (plugins.setFavourite (plugin.id, ! plugin.favourite));
    else if (column == hiddenColumn)
        report (plugins.setHidden (plugin.id, ! plugin.hidden));
    else
        return;

    refresh();
}

juce::Component* PluginLibraryPanel::refreshComponentForCell (int row, int column, bool, juce::Component* existing)
{
    if (column != groupColumn || row < 0 || row >= static_cast<int> (rows.size()))
    {
        delete existing; // JUCE-Vertrag: nicht mehr gebrauchte Zellkomponente selbst freigeben
        return nullptr;
    }

    auto* box = dynamic_cast<GroupBox*> (existing);

    if (box == nullptr)
    {
        delete existing;
        box = new GroupBox(); // JUCE-Vertrag: die Tabelle besitzt die Zellkomponente
        box->addItem (TRANS ("automatic"), automaticGroupId);

        for (int i = 0; i < services::PluginService::getGroupNames().size(); ++i)
            box->addItem (services::PluginService::getGroupDisplayName (services::PluginService::getGroupNames()[i]), automaticGroupId + 1 + i);

        box->setTooltip (TRANS ("Category in the plugin menu. \"automatic\" uses the vendor's own information."));
        box->onChange = [this, box]
        {
            const auto id = box->getSelectedId();
            const auto group =
                id > automaticGroupId ? services::PluginService::getGroupNames()[id - automaticGroupId - 1] : juce::String();
            report (plugins.setGroup (box->pluginId, group));
            refresh();
        };
    }

    const auto& plugin = rows[static_cast<std::size_t> (row)];
    box->pluginId = plugin.id;
    // Angezeigt wird die gültige Kategorie; ob sie automatisch ist, verrät der Service nicht, also
    // steht hier die Kategorie selbst. "automatisch" setzt zurück.
    box->setSelectedId (automaticGroupId + 1 + services::PluginService::getGroupNames().indexOf (plugin.group),
                        juce::dontSendNotification);
    return box;
}

void PluginLibraryPanel::report (const core::Result<void>& result)
{
    if (! result)
        hint.setText (services::errorText (result.error()), juce::dontSendNotification);
}

} // namespace fk::ui
