#pragma once

#include "fk/core/Result.h"
#include "fk/engine/EngineContext.h"
#include "fk/services/ProjectService.h"

namespace fk::services
{

/// Wo die App ihre Berichte ablegt (die App legt die Ordner fest, Tests nehmen Temp-Ordner).
struct DiagnosticsFolders
{
    juce::File logs;         ///< Log-Dateien, eine je Programmstart
    juce::File hangReports;  ///< Berichte des Hänger-Wächters
    juce::File crashReports; ///< Stack-Traces nach Abstürzen
};

/// Sammelt alles, was bei einer Fehlersuche hilft, in eine ZIP-Datei: Systembeschreibung
/// (Version, Betriebssystem, Audiogerät, Plugins, Projekt) und die neuesten Logs, Hänger- und
/// Absturzberichte. Der Nutzer schickt die Datei; es wird nichts automatisch versendet.
class DiagnosticsService final
{
public:
    static constexpr int maxFilesPerFolder = 5;

    DiagnosticsService (engine::EngineContext& engineContext,
                        ProjectService& projectService,
                        DiagnosticsFolders folders);

    /// Lesbare Beschreibung von App, System, Audiogerät und offenem Projekt.
    [[nodiscard]] juce::String describeSystem (const juce::String& appVersion) const;

    /// Schreibt den Bericht als ZIP nach zipFile (vorhandene Datei wird ersetzt).
    [[nodiscard]] core::Result<void> exportReport (const juce::File& zipFile, const juce::String& appVersion) const;

    [[nodiscard]] const DiagnosticsFolders& getFolders() const noexcept;

private:
    engine::EngineContext& engineContext;
    ProjectService& projectService;
    DiagnosticsFolders folders;
};

} // namespace fk::services
