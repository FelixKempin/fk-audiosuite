#include "fk/ui/PluginMenu.h"

#include <map>

namespace fk::ui
{

namespace
{
juce::String displayName (const services::PluginInfo& plugin)
{
    return plugin.format == "FK" ? plugin.name : plugin.name + "  (" + plugin.format + ")";
}
} // namespace

juce::PopupMenu buildPluginMenu (const std::vector<services::PluginInfo>& plugins, int firstId)
{
    juce::PopupMenu menu;

    if (plugins.empty())
    {
        menu.addItem (-1, TRANS ("No plugins found (Plugin Manager: Search and Scan)"), false);
        return menu;
    }

    bool anyFavourite = false;

    for (std::size_t i = 0; i < plugins.size(); ++i)
        if (plugins[i].favourite)
        {
            if (! anyFavourite)
                menu.addSectionHeader (TRANS ("Favorites"));

            anyFavourite = true;
            menu.addItem (firstId + static_cast<int> (i), displayName (plugins[i]));
        }

    if (anyFavourite)
        menu.addSeparator();

    // Kategorien in fester Reihenfolge, leere fallen weg.
    for (const auto& group : services::PluginService::getGroupNames())
    {
        juce::PopupMenu groupMenu;

        for (std::size_t i = 0; i < plugins.size(); ++i)
            if (plugins[i].group == group)
                groupMenu.addItem (firstId + static_cast<int> (i), displayName (plugins[i]));

        if (groupMenu.getNumItems() > 0)
            menu.addSubMenu (services::PluginService::getGroupDisplayName (group), groupMenu);
    }

    std::map<juce::String, juce::PopupMenu> vendors;

    for (std::size_t i = 0; i < plugins.size(); ++i)
    {
        const auto vendor = plugins[i].manufacturer.isNotEmpty() ? plugins[i].manufacturer : TRANS ("No vendor");
        vendors[vendor].addItem (firstId + static_cast<int> (i), displayName (plugins[i]));
    }

    juce::PopupMenu byVendor;

    for (auto& [vendor, vendorMenu] : vendors)
        byVendor.addSubMenu (vendor, vendorMenu);

    menu.addSeparator();
    menu.addSubMenu (TRANS ("By vendor"), byVendor);
    return menu;
}

} // namespace fk::ui
