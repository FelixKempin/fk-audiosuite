#include "fk/core/Result.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using fk::core::ErrorCode;
using fk::core::Result;

// Beispiel-Funktion, wie sie ein Service schreiben würde: Erfolg oder Fehler, keine Ausnahme.
Result<int> parsePositive (int value)
{
    if (value <= 0)
        return fk::core::fail (ErrorCode::invalidArgument, "Wert muss positiv sein");

    return value;
}

} // namespace

TEST_CASE ("Result: Erfolgsfall enthält den Wert", "[core]")
{
    const auto result = parsePositive (42);

    REQUIRE (result.has_value());
    REQUIRE (*result == 42);
}

TEST_CASE ("Result: Fehlerfall enthält Code und Nachricht", "[core]")
{
    const auto result = parsePositive (-1);

    REQUIRE_FALSE (result.has_value());
    REQUIRE (result.error().code == ErrorCode::invalidArgument);
    REQUIRE (result.error().message == "Wert muss positiv sein");
}

TEST_CASE ("ErrorCode: toString liefert stabile Bezeichner", "[core]")
{
    REQUIRE (fk::core::toString (ErrorCode::notFound) == "notFound");
    REQUIRE (fk::core::toString (ErrorCode::deviceError) == "deviceError");
}
