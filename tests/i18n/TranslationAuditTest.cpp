#include "TranslationAudit.h"

#include <catch2/catch_test_macros.hpp>

using fk::tests::i18n::TranslationAudit;

TEST_CASE ("TranslationAudit: findet TRANS, NEEDS_TRANS und plural, aber nichts in Kommentaren", "[i18n]")
{
    const juce::String source = R"cpp(
        // TRANS ("im Kommentar")
        /* TRANS ("im Blockkommentar") */
        const auto url = "https://example.org"; // kein Kommentar im Literal
        label.setText (TRANS ("Save project"), juce::dontSendNotification);
        button.setTooltip (TRANS ("Opens \"Settings\" (F2)"));
        const auto box = TRANS (juce::String::fromUTF8 ("4x12 cabinet"));
        static constexpr const char* names[] = { NEEDS_TRANS ("Off"), NEEDS_TRANS ("On") };
        const auto text = i18n::plural (count, "{count} track", "{count} tracks");
    )cpp";

    const auto keys = TranslationAudit::extractKeys (source);

    CHECK (keys.contains ("Save project"));
    CHECK (keys.contains ("Opens \"Settings\" (F2)"));
    CHECK (keys.contains ("4x12 cabinet"));
    CHECK (keys.contains ("Off"));
    CHECK (keys.contains ("On"));
    CHECK (keys.contains ("{count} track"));
    CHECK (keys.contains ("{count} tracks"));
    CHECK_FALSE (keys.contains ("im Kommentar"));
    CHECK_FALSE (keys.contains ("im Blockkommentar"));
}

TEST_CASE ("TranslationAudit: liest Tabellen wie JUCE und meldet Doppeltes und Leeres", "[i18n]")
{
    const auto table = TranslationAudit::parseTable ("language: German\n"
                                                     "\"Save\" = \"Speichern\"\n"
                                                     "\"Say \\\"hi\\\"\" = \"Sag \\\"hallo\\\"\"\n"
                                                     "\"Save\" = \"Sichern\"\n"
                                                     "\"Empty\" = \"\"\n"
                                                     "\"Broken\" \"kein Gleich\"\n",
                                                     "de.txt");

    REQUIRE (table.entries.size() == 4);
    CHECK (table.entries[1].key == "Say \"hi\"");
    CHECK (table.entries[1].value == "Sag \"hallo\"");
    REQUIRE (table.problems.size() == 3); // doppelt, leer, unlesbar
}

TEST_CASE ("TranslationAudit: meldet fehlende, verwaiste und abweichende Platzhalter", "[i18n]")
{
    const auto foundation = TranslationAudit::parseTable ("\"Cancel\" = \"Abbrechen\"\n", "fundament");
    const auto own = TranslationAudit::parseTable ("\"Track {name}\" = \"Spur {nmae}\"\n"
                                                   "\"Old text\" = \"Alter Text\"\n"
                                                   "\"Cancel\" = \"Abbrechen\"\n",
                                                   "daw");

    const auto report = TranslationAudit::audit ({ "Track {name}", "Cancel", "New text" }, own, { foundation });

    CHECK (report.missing == juce::StringArray { "New text" });
    CHECK (report.unused == juce::StringArray { "Old text" });
    CHECK (report.problems.size() == 2); // Platzhalter, Doppelung mit dem Fundament
    CHECK_FALSE (report.isClean());
}

TEST_CASE ("TranslationAudit: liest Anzeigetexte aus JSON in jeder Tiefe", "[i18n]")
{
    const juce::TemporaryFile temporary;
    const auto directory = temporary.getFile().getSiblingFile ("fk-i18n-json-test");
    directory.deleteRecursively();
    REQUIRE (directory.createDirectory());
    REQUIRE (directory.getChildFile ("amp.json")
                 .replaceWithText (R"({"id":"a","name":"Amp","description":"Clean amp",
                                       "channels":[{"name":"Lead","description":"Hot channel"}]})"));

    const auto keys = TranslationAudit::collectJsonKeys (directory, { "description" });

    CHECK (keys.size() == 2);
    CHECK (keys.contains ("Clean amp"));
    CHECK (keys.contains ("Hot channel"));
    directory.deleteRecursively();
}
