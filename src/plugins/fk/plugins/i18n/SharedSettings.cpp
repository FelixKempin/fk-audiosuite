#include "fk/plugins/i18n/SharedSettings.h"

#include "fk/plugins/i18n/Text.h"

namespace fk::plugins::i18n
{

SharedSettings::SharedSettings (juce::File settingsFile)
    : file (std::move (settingsFile))
{
}

juce::File SharedSettings::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FKAudioSuite")
        .getChildFile ("common.json");
}

juce::var SharedSettings::load() const
{
    if (! file.existsAsFile())
        return {};

    return juce::JSON::parse (file.loadFileAsString());
}

juce::String SharedSettings::getValue (const juce::String& key) const
{
    const auto settings = load();
    return settings.isObject() ? settings.getProperty (key, {}).toString() : juce::String();
}

core::Result<void> SharedSettings::setValue (const juce::String& key, const juce::String& value)
{
    auto settings = load();

    // Kaputte oder fremde Datei: neu anfangen statt die Einstellung zu verweigern.
    if (! settings.isObject())
        settings = juce::var (new juce::DynamicObject());

    settings.getDynamicObject()->setProperty (key, value);

    // replaceWithText schreibt eine Temporärdatei und benennt sie um: Ein gleichzeitiger Leser
    // sieht immer die alte oder die neue Fassung, nie eine halbe.
    if (! file.getParentDirectory().createDirectory() || ! file.replaceWithText (juce::JSON::toString (settings)))
        return core::fail (core::ErrorCode::ioError,
                           format (TRANS ("Could not save settings: {file}"), { { "file", file.getFullPathName() } })
                               .toStdString());

    return {};
}

const juce::File& SharedSettings::getFile() const noexcept
{
    return file;
}

} // namespace fk::plugins::i18n
