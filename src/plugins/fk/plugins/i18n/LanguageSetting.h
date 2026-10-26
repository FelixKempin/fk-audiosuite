#pragma once

#include "fk/core/Result.h"
#include "fk/plugins/i18n/Language.h"
#include "fk/plugins/i18n/SharedSettings.h"

#include <optional>

namespace fk::plugins::i18n
{

/// Welche Sprache ein FK-Programm zeigt. Ohne ausdrückliche Wahl folgt sie der Systemsprache
/// (Deutsch bei deutschem System, sonst Englisch); das ist der "Vorschlag" beim ersten Start, ohne
/// Dialog, der in einem Host-Prozess fehl am Platz wäre. Eine Wahl gilt für alle FK-Programme.
class LanguageSetting final
{
public:
    static constexpr auto settingKey = "language";

    explicit LanguageSetting (SharedSettings sharedSettings);

    /// Die ausdrücklich gewählte Sprache, falls es eine gibt.
    [[nodiscard]] std::optional<Language> getChosen() const;

    /// Gewählte Sprache, sonst die zur Systemsprache passende (testbar mit beliebigem Systemwert).
    [[nodiscard]] Language resolve (const juce::String& systemLanguage) const;

    /// resolve() mit der Systemsprache dieses Rechners.
    [[nodiscard]] Language resolve() const;

    core::Result<void> choose (Language language);

    /// Unterstützte Systemsprache oder Englisch.
    [[nodiscard]] static Language fromSystem (const juce::String& systemLanguage);

private:
    SharedSettings settings;
};

} // namespace fk::plugins::i18n
