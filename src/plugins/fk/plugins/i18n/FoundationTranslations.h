#pragma once

#include "fk/plugins/i18n/Translator.h"

#include <vector>

namespace fk::plugins::i18n
{

/// Tabellen des Fundaments (src/plugins/lang/*.txt): Texte von edition/, ui/, presets/ und den
/// eingebauten Effekten. DAW und Produkte setzen sie immer zuerst, ihre eigenen Tabellen danach.
[[nodiscard]] std::vector<TranslationTable> foundationTranslations();

} // namespace fk::plugins::i18n
