#pragma once

#include <string_view>

namespace fk::plugins::edition
{

/// Ausgabe eines Produkts: Lite ist der kostenlose Download mit Grenzen,
/// Pro schaltet alles frei. Ein Code für beide; die Ausgabe ist Laufzeitzustand.
enum class Edition
{
    lite,
    pro
};

[[nodiscard]] constexpr std::string_view toString (Edition edition) noexcept
{
    return edition == Edition::pro ? "Pro" : "Lite";
}

} // namespace fk::plugins::edition
