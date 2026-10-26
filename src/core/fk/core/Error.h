#pragma once

#include <string>
#include <string_view>

namespace fk::core
{

/// Fehlerkategorie. Grob genug, dass der Aufrufer entscheiden kann, wie er reagiert
/// (Dialog zeigen, erneut versuchen, abbrechen). Details stehen in Error::message.
enum class ErrorCode
{
    unknown,
    invalidArgument, ///< Aufrufer hat ungültige Daten übergeben
    notFound,        ///< Datei, Gerät oder Objekt existiert nicht
    ioError,         ///< Lesen oder Schreiben fehlgeschlagen
    deviceError,     ///< Audio- oder MIDI-Gerät meldet einen Fehler
    unsupported,     ///< Format oder Funktion wird nicht unterstützt
    cancelled        ///< der Benutzer hat abgebrochen; keine Fehlermeldung zeigen
};

/// Stabiler, englischer Bezeichner eines ErrorCode für Logdateien und Diagnose.
[[nodiscard]] std::string_view toString (ErrorCode code) noexcept;

/// Ein Fehler, wie ihn Result<T> transportiert: Kategorie plus Klartext für den Nutzer.
/// Reiner Datenträger ohne Invarianten, deshalb ein struct (CODING_GUIDELINES.md §5).
struct Error
{
    ErrorCode code = ErrorCode::unknown;
    std::string message;
};

} // namespace fk::core
