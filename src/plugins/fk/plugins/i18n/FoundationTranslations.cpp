#include "fk/plugins/i18n/FoundationTranslations.h"

#include "FkFoundationTranslations.h"

namespace fk::plugins::i18n
{

std::vector<TranslationTable> foundationTranslations()
{
    return { { Language::german,
               { FkFoundationTranslations::de_txt, static_cast<std::size_t> (FkFoundationTranslations::de_txtSize) } } };
}

} // namespace fk::plugins::i18n
