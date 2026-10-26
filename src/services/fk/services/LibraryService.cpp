#include "fk/services/LibraryService.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto pluginPrefix = "plugin:";
constexpr auto sfzPrefix = "sfz:";
constexpr auto presetPrefix = "preset:";
} // namespace

LibraryService::LibraryService (PluginService& pluginService,
                                ContentService& contentService,
                                SamplerService& samplerService,
                                TrackService& trackService,
                                juce::File file)
    : plugins (pluginService)
    , content (contentService)
    , sampler (samplerService)
    , tracks (trackService)
    , settingsFile (std::move (file))
{
}

juce::var LibraryService::loadSettings() const
{
    const auto parsed = juce::JSON::parse (settingsFile.loadFileAsString());
    return parsed.isObject() ? parsed : juce::var (new juce::DynamicObject());
}

void LibraryService::storeSettings (const juce::var& settings)
{
    settingsFile.getParentDirectory().createDirectory();
    settingsFile.replaceWithText (juce::JSON::toString (settings));
}

std::vector<LibraryItem> LibraryService::getItems() const
{
    const auto settings = loadSettings();
    std::vector<LibraryItem> items;

    const auto addUserData = [&settings] (LibraryItem& item)
    {
        const auto entry = settings.getProperty (item.key, {});

        if (const auto* own = entry.getProperty ("tags", {}).getArray())
            for (const auto& tag : *own)
                item.tags.addIfNotAlreadyThere (tag.toString());

        if (item.kind != LibraryItem::Kind::plugin)
            item.favourite = entry.getProperty ("favourite", false);
    };

    for (const auto& plugin : plugins.getAllPlugins())
    {
        if (! plugin.isInstrument || plugin.hidden)
            continue;

        LibraryItem item { pluginPrefix + plugin.id, LibraryItem::Kind::plugin, plugin.name, plugin.manufacturer, {}, plugin.favourite };
        item.tags.add (TRANS ("Plugin"));
        item.tags.addIfNotAlreadyThere (plugin.format == "FK" ? juce::String ("FK") : plugin.format);

        if (plugin.manufacturer.isNotEmpty())
            item.tags.addIfNotAlreadyThere (plugin.manufacturer);

        addUserData (item);
        items.push_back (std::move (item));
    }

    for (const auto& instrument : content.getInstruments())
    {
        juce::String packageName = instrument.packageId;

        for (const auto& package : content.getPackages())
            if (package.id == instrument.packageId)
                packageName = package.name;

        LibraryItem item { sfzPrefix + instrument.sfzFile.getFullPathName(), LibraryItem::Kind::sfz, instrument.name, packageName, {}, false };
        item.tags.add (TRANS ("Library"));
        item.tags.add (packageName);

        // Ordner des Pakets als Tags ("Strings", "Woodwinds" ...), ohne den Archiv-Hauptordner.
        juce::StringArray folders;
        folders.addTokens (instrument.category, "/", {});

        for (int i = 1; i < folders.size(); ++i)
            item.tags.addIfNotAlreadyThere (folders[i]);

        addUserData (item);
        items.push_back (std::move (item));
    }

    for (const auto& preset : plugins.getAllUserPresets (true))
    {
        LibraryItem item { presetPrefix + preset.pluginId + "|" + preset.name, LibraryItem::Kind::preset, preset.name, preset.pluginName, {}, false };
        item.tags.add (TRANS ("Preset"));
        item.tags.add (preset.pluginName);
        addUserData (item);
        items.push_back (std::move (item));
    }

    // Favoriten zuerst, dann nach Name.
    std::ranges::stable_sort (items,
                              [] (const LibraryItem& a, const LibraryItem& b)
                              {
                                  if (a.favourite != b.favourite)
                                      return a.favourite;

                                  return a.name.compareNatural (b.name) < 0;
                              });
    return items;
}

std::vector<LibraryItem> LibraryService::search (const juce::String& text, const juce::String& tag, bool favouritesOnly) const
{
    juce::StringArray words;
    words.addTokens (text.trim(), " ", "\"");
    words.removeEmptyStrings();

    std::vector<LibraryItem> found;

    for (auto& item : getItems())
    {
        if (favouritesOnly && ! item.favourite)
            continue;

        if (tag.isNotEmpty() && ! item.tags.contains (tag, true))
            continue;

        const auto haystack = item.name + " " + item.source + " " + item.tags.joinIntoString (" ");
        const bool matches = std::all_of (words.begin(), words.end(),
                                          [&haystack] (const juce::String& word) { return haystack.containsIgnoreCase (word); });

        if (matches)
            found.push_back (std::move (item));
    }

    return found;
}

juce::StringArray LibraryService::getAllTags() const
{
    juce::StringArray tags;

    for (const auto& item : getItems())
        for (const auto& tag : item.tags)
            tags.addIfNotAlreadyThere (tag, true);

    tags.sortNatural();
    return tags;
}

bool LibraryService::exists (const juce::String& key) const
{
    const auto items = getItems();
    return std::ranges::any_of (items, [&key] (const LibraryItem& item) { return item.key == key; });
}

core::Result<void> LibraryService::setFavourite (const juce::String& key, bool shouldBeFavourite)
{
    if (key.startsWith (pluginPrefix))
        return plugins.setFavourite (key.fromFirstOccurrenceOf (pluginPrefix, false, false), shouldBeFavourite);

    if (! exists (key))
        return core::fail (core::ErrorCode::notFound, TRANS ("Entry not found.").toStdString());

    auto settings = loadSettings();
    auto entry = settings.getProperty (key, {});

    if (! entry.isObject())
        entry = juce::var (new juce::DynamicObject());

    entry.getDynamicObject()->setProperty ("favourite", shouldBeFavourite);
    settings.getDynamicObject()->setProperty (key, entry);
    storeSettings (settings);
    return {};
}

core::Result<void> LibraryService::addTag (const juce::String& key, const juce::String& tag)
{
    const auto clean = tag.trim();

    if (clean.isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Empty tag.").toStdString());

    if (! exists (key))
        return core::fail (core::ErrorCode::notFound, TRANS ("Entry not found.").toStdString());

    auto settings = loadSettings();
    auto entry = settings.getProperty (key, {});

    if (! entry.isObject())
        entry = juce::var (new juce::DynamicObject());

    juce::Array<juce::var> list;

    if (const auto* existing = entry.getProperty ("tags", {}).getArray())
        list = *existing;

    if (! list.contains (clean))
        list.add (clean);

    entry.getDynamicObject()->setProperty ("tags", list);
    settings.getDynamicObject()->setProperty (key, entry);
    storeSettings (settings);
    return {};
}

core::Result<void> LibraryService::removeTag (const juce::String& key, const juce::String& tag)
{
    auto settings = loadSettings();
    auto entry = settings.getProperty (key, {});
    const auto* existing = entry.getProperty ("tags", {}).getArray();

    if (existing == nullptr || ! existing->contains (tag))
        return core::fail (core::ErrorCode::notFound, TRANS ("Only your own tags can be removed.").toStdString());

    juce::Array<juce::var> list (*existing);
    list.removeAllInstancesOf (tag);
    entry.getDynamicObject()->setProperty ("tags", list);
    settings.getDynamicObject()->setProperty (key, entry);
    storeSettings (settings);
    return {};
}

core::Result<te::AudioTrack*> LibraryService::createTrack (const juce::String& key)
{
    if (key.startsWith (pluginPrefix))
    {
        const auto pluginId = key.fromFirstOccurrenceOf (pluginPrefix, false, false);

        for (const auto& plugin : plugins.getAllPlugins())
            if (plugin.id == pluginId)
                return tracks.createInstrumentTrack (plugin.name, plugin.id);

        return core::fail (core::ErrorCode::notFound, TRANS ("Instrument not found.").toStdString());
    }

    if (key.startsWith (sfzPrefix))
    {
        const juce::File file (key.fromFirstOccurrenceOf (sfzPrefix, false, false));
        return sampler.createTrackFromSfz (file, file.getFileNameWithoutExtension());
    }

    if (key.startsWith (presetPrefix))
    {
        const auto rest = key.fromFirstOccurrenceOf (presetPrefix, false, false);
        const auto pluginId = rest.upToLastOccurrenceOf ("|", false, false);
        const auto presetName = rest.fromLastOccurrenceOf ("|", false, false);
        auto created = tracks.createInstrumentTrack (presetName, pluginId);

        if (! created)
            return created;

        if (const auto instrument = plugins.getInstrument (**created))
            if (auto loaded = plugins.loadUserPreset (instrument->id, presetName); ! loaded)
                return std::unexpected (loaded.error());

        return created;
    }

    return core::fail (core::ErrorCode::invalidArgument, TRANS ("Unknown entry.").toStdString());
}

} // namespace fk::services
