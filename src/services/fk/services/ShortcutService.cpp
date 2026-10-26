#include "fk/services/ShortcutService.h"

namespace fk::services
{

namespace
{
constexpr auto shortcutsItem = "fkShortcuts"; // JSON-Objekt in den Engine-Einstellungen
} // namespace

ShortcutService::ShortcutService (engine::EngineContext& context)
    : engineContext (context)
{
}

std::map<juce::String, juce::StringArray> ShortcutService::getOverrides() const
{
    std::map<juce::String, juce::StringArray> overrides;
    const auto stored = juce::JSON::parse (
        engineContext.getEngine().getPropertyStorage().getPropertyItem (te::SettingID::invalid, shortcutsItem).toString());

    if (const auto* object = stored.getDynamicObject())
        for (const auto& [name, value] : object->getProperties())
        {
            juce::StringArray keys;

            if (const auto* list = value.getArray())
                for (const auto& key : *list)
                    keys.add (key.toString());

            overrides[name.toString()] = keys;
        }

    return overrides;
}

void ShortcutService::setOverride (const juce::String& commandKey, const juce::StringArray& keyDescriptions)
{
    auto overrides = getOverrides();
    overrides[commandKey] = keyDescriptions;
    store (overrides);
}

void ShortcutService::clearOverride (const juce::String& commandKey)
{
    auto overrides = getOverrides();
    overrides.erase (commandKey);
    store (overrides);
}

void ShortcutService::clearAll()
{
    store ({});
}

void ShortcutService::store (const std::map<juce::String, juce::StringArray>& overrides)
{
    auto object = std::make_unique<juce::DynamicObject>();

    for (const auto& [commandKey, keys] : overrides)
    {
        juce::Array<juce::var> list;

        for (const auto& key : keys)
            list.add (key);

        object->setProperty (commandKey, list);
    }

    auto& storage = engineContext.getEngine().getPropertyStorage();
    storage.setPropertyItem (te::SettingID::invalid, shortcutsItem, juce::JSON::toString (juce::var (object.release()), true));
    storage.flushSettingsToDisk();
}

} // namespace fk::services
