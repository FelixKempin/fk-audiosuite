#include "TranslationAudit.h"

#include <catch2/catch_test_macros.hpp>

using fk::tests::i18n::TranslationAudit;

// Jeder Text im Code hat eine deutsche Übersetzung, keine Übersetzung ist verwaist (ADR 0010).
// FK_SOURCE_DIR setzt tests/CMakeLists.txt.

namespace
{
juce::File sourceDir()
{
    return juce::File (FK_SOURCE_DIR);
}
} // namespace

TEST_CASE ("Übersetzungen: Fundament (src/plugins) ist vollständig deutsch", "[i18n]")
{
    const auto keys = TranslationAudit::collectSourceKeys ({ sourceDir().getChildFile ("src/plugins/fk") });
    const auto table = TranslationAudit::loadTable (sourceDir().getChildFile ("src/plugins/lang/de.txt"));

    const auto report = TranslationAudit::audit (keys, table);

    INFO (report.describe());
    CHECK (report.isClean());
}

#if FK_TEST_DAW_TRANSLATIONS
TEST_CASE ("Übersetzungen: DAW (app, ui, services, engine) ist vollständig deutsch", "[i18n]")
{
    const auto keys = TranslationAudit::collectSourceKeys ({ sourceDir().getChildFile ("src/app"),
                                                             sourceDir().getChildFile ("src/ui"),
                                                             sourceDir().getChildFile ("src/services"),
                                                             sourceDir().getChildFile ("src/engine") });
    const auto foundation = TranslationAudit::loadTable (sourceDir().getChildFile ("src/plugins/lang/de.txt"));
    const auto table = TranslationAudit::loadTable (sourceDir().getChildFile ("resources/lang/de.txt"));

    const auto report = TranslationAudit::audit (keys, table, { foundation });

    INFO (report.describe());
    CHECK (report.isClean());
}
#endif
