#include "fk/core/Error.h"

namespace fk::core
{

std::string_view toString (ErrorCode code) noexcept
{
    // Jeder Enum-Wert hat genau einen Fall. Fehlt einer, warnt der Compiler (-Wswitch),
    // deshalb bewusst kein "default:".
    switch (code)
    {
        case ErrorCode::unknown:
            return "unknown";
        case ErrorCode::invalidArgument:
            return "invalidArgument";
        case ErrorCode::notFound:
            return "notFound";
        case ErrorCode::ioError:
            return "ioError";
        case ErrorCode::deviceError:
            return "deviceError";
        case ErrorCode::cancelled:
            return "cancelled";
        case ErrorCode::unsupported:
            return "unsupported";
    }

    return "unknown";
}

} // namespace fk::core
