#pragma once

#include "fk/core/Error.h"

#include <expected>
#include <string>
#include <utility>

namespace fk::core
{

/// Ergebnis einer Operation, die fehlschlagen kann: entweder ein Wert vom Typ T oder ein Error.
/// Der Aufrufer muss prüfen, welcher Fall vorliegt; Ausnahmen werfen wir nicht.
/// → LEARNING.md §9 (Fehlerbehandlung ohne Ausnahmen)
template <typename T>
using Result = std::expected<T, Error>;

/// Erzeugt den Fehlerfall für ein Result. Der Rückgabetyp wandelt sich automatisch
/// in jedes Result<T> um, deshalb reicht: return fail (ErrorCode::notFound, "Datei fehlt");
[[nodiscard]] inline std::unexpected<Error> fail (ErrorCode code, std::string message)
{
    return std::unexpected (Error { code, std::move (message) });
}

} // namespace fk::core
