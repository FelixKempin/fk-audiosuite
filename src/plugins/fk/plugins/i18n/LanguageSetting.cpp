#include "fk/plugins/i18n/LanguageSetting.h"

namespace fk::plugins::i18n
{

LanguageSetting::LanguageSetting (SharedSettings sharedSettings)
    : settings (std::move (sharedSettings))
{
}

std::optional<Language> LanguageSetting::getChosen() const
{
    return fromCode (settings.getValue (settingKey));
}

Language LanguageSetting::resolve (const juce::String& systemLanguage) const
{
    return getChosen().value_or (fromSystem (systemLanguage));
}

Language LanguageSetting::resolve() const
{
    return resolve (juce::SystemStats::getUserLanguage());
}

core::Result<void> LanguageSetting::choose (Language language)
{
    return settings.setValue (settingKey, toCode (language));
}

Language LanguageSetting::fromSystem (const juce::String& systemLanguage)
{
    return fromCode (systemLanguage).value_or (Language::english);
}

} // namespace fk::plugins::i18n
