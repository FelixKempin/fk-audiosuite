#include "fk/plugins/i18n/FoundationTranslations.h"
#include "fk/plugins/i18n/Language.h"
#include "fk/plugins/i18n/LanguageSetting.h"
#include "fk/plugins/i18n/SharedSettings.h"
#include "fk/plugins/i18n/Text.h"
#include "fk/plugins/i18n/Translator.h"

#include <catch2/catch_test_macros.hpp>

using namespace fk::plugins::i18n;

namespace
{
constexpr std::string_view germanTable = "language: German\n"
                                         "\"Save\" = \"Speichern\"\n"
                                         "\"{count} track\" = \"{count} Spur\"\n"
                                         "\"{count} tracks\" = \"{count} Spuren\"\n"
                                         "\"Track {name} of {owner}\" = \"{owner}: Spur {name}\"\n";

constexpr std::string_view laterTable = "\"Save\" = \"Sichern\"\n";

/// Setzt die Zuordnung nach jedem Test zurück, damit andere Tests englischen Quelltext sehen.
struct ScopedTranslation
{
    ScopedTranslation (Language language, std::span<const TranslationTable> tables)
    {
        Translator::apply (language, tables);
    }
    ~ScopedTranslation() { Translator::apply (Language::english, {}); }
};

juce::File freshSettingsFile (const char* name)
{
    const juce::TemporaryFile temporary;
    const auto directory = temporary.getFile().getSiblingFile (name);
    directory.deleteRecursively();
    return directory.getChildFile ("common.json");
}
} // namespace

TEST_CASE ("Language: Codes hin und zurück, Systemformate werden verstanden", "[plugins][i18n]")
{
    for (const auto language : allLanguages)
        CHECK (fromCode (toCode (language)) == language);

    CHECK (fromCode ("de_DE") == Language::german);
    CHECK (fromCode ("de-AT") == Language::german);
    CHECK (fromCode ("EN") == Language::english);
    CHECK_FALSE (fromCode ("fr").has_value());
    CHECK_FALSE (fromCode ("").has_value());
    CHECK (nativeName (Language::german) == "Deutsch");
}

TEST_CASE ("Translator: Englisch ist der Quelltext, Deutsch kommt aus der Tabelle", "[plugins][i18n]")
{
    const std::array tables { TranslationTable { Language::german, germanTable } };

    {
        const ScopedTranslation german (Language::german, tables);
        CHECK (TRANS ("Save") == "Speichern");
        CHECK (TRANS ("Not in the table") == "Not in the table"); // Rückfall auf Englisch
    }

    CHECK (TRANS ("Save") == "Save");
    CHECK (Translator::build (Language::english, tables) == nullptr);
}

TEST_CASE ("Translator: spätere Tabellen überschreiben frühere", "[plugins][i18n]")
{
    const std::array tables { TranslationTable { Language::german, germanTable },
                              TranslationTable { Language::german, laterTable } };

    const auto strings = Translator::build (Language::german, tables);

    REQUIRE (strings != nullptr);
    CHECK (strings->translate ("Save") == "Sichern");
    CHECK (strings->translate ("{count} tracks") == "{count} Spuren");
}

TEST_CASE ("Text: Platzhalter dürfen in der Übersetzung umgestellt sein, Plural wählt die Form", "[plugins][i18n]")
{
    const std::array tables { TranslationTable { Language::german, germanTable } };
    const ScopedTranslation german (Language::german, tables);

    CHECK (format (TRANS ("Track {name} of {owner}"), { { "name", "Bass" }, { "owner", "Felix" } })
           == "Felix: Spur Bass");
    CHECK (plural (1, "{count} track", "{count} tracks") == "1 Spur");
    CHECK (plural (3, "{count} track", "{count} tracks") == "3 Spuren");
    CHECK (plural (0, "{count} track", "{count} tracks") == "0 Spuren");
}

TEST_CASE ("SharedSettings: schreibt atomar, liest neu und lässt fremde Felder stehen", "[plugins][i18n]")
{
    const auto file = freshSettingsFile ("fk-i18n-settings-test");
    SharedSettings settings (file);

    CHECK (settings.getValue ("language").isEmpty()); // keine Datei: leer, kein Fehler

    REQUIRE (file.getParentDirectory().createDirectory());
    REQUIRE (file.replaceWithText (R"({"other":"keep"})"));
    REQUIRE (settings.setValue ("language", "de").has_value());

    CHECK (SharedSettings (file).getValue ("language") == "de");
    CHECK (SharedSettings (file).getValue ("other") == "keep");

    REQUIRE (file.replaceWithText ("kaputt"));
    CHECK (settings.getValue ("language").isEmpty());
    REQUIRE (settings.setValue ("language", "en").has_value()); // kaputte Datei wird ersetzt
    CHECK (settings.getValue ("language") == "en");
    file.getParentDirectory().deleteRecursively();
}

TEST_CASE ("LanguageSetting: ohne Wahl folgt die Systemsprache, eine Wahl bleibt nach Neustart", "[plugins][i18n]")
{
    const auto file = freshSettingsFile ("fk-i18n-language-test");

    SECTION ("erster Start")
    {
        const LanguageSetting setting { SharedSettings (file) };
        CHECK_FALSE (setting.getChosen().has_value());
        CHECK (setting.resolve ("en_US") == Language::english);
        CHECK (setting.resolve ("de_DE") == Language::german);
        CHECK (setting.resolve ("fr_FR") == Language::english); // nicht unterstützt: Englisch
        CHECK (setting.resolve ("") == Language::english);
    }

    SECTION ("Wahl gewinnt gegen das System und überlebt einen Neustart")
    {
        LanguageSetting first { SharedSettings (file) };
        REQUIRE (first.choose (Language::english).has_value());

        const LanguageSetting afterRestart { SharedSettings (file) };
        CHECK (afterRestart.getChosen() == Language::english);
        CHECK (afterRestart.resolve ("de_DE") == Language::english);

        REQUIRE (first.choose (Language::german).has_value());
        CHECK (LanguageSetting { SharedSettings (file) }.resolve ("en_US") == Language::german);
    }

    file.getParentDirectory().deleteRecursively();
}

TEST_CASE ("Fundament-Tabelle ist eingebettet und deutsch", "[plugins][i18n]")
{
    const auto tables = foundationTranslations();

    REQUIRE_FALSE (tables.empty());
    const auto strings = Translator::build (Language::german, tables);
    REQUIRE (strings != nullptr);
    CHECK (strings->getLanguageName() == "German");
}
