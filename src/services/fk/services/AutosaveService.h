#pragma once

#include "fk/engine/EngineContext.h"
#include "fk/services/ProjectService.h"

#include <juce_events/juce_events.h>

#include <chrono>

namespace fk::services
{

/// Sichert das offene Projekt in festen Abständen nach Backups/<Name>.autosave.tracktionedit,
/// solange es ungespeicherte Änderungen gibt. Das Schreiben selbst macht ProjectService::writeAutosave;
/// hier liegt nur der Takt. Das Intervall gilt für alle Projekte und steht in den Engine-Einstellungen.
/// Läuft auf dem Message-Thread.
class AutosaveService final : private juce::Timer
{
public:
    static constexpr std::chrono::seconds defaultInterval { std::chrono::minutes (5) };

    /// Liest das gespeicherte Intervall (Vorgabe 5 Minuten).
    AutosaveService (ProjectService& projectService, engine::EngineContext& engineContext);
    ~AutosaveService() override;

    /// Neues Intervall, gespeichert für die nächsten Starts. 0 schaltet die automatische Sicherung aus.
    void setInterval (std::chrono::seconds interval);
    [[nodiscard]] std::chrono::seconds getInterval() const noexcept;

    /// Zeitpunkt der letzten erfolgreichen automatischen Sicherung; ungültig, wenn es noch keine gab.
    [[nodiscard]] juce::Time getLastAutosaveTime() const noexcept;

    /// Sichert sofort (der Timer ruft das auch).
    void autosaveNow();

private:
    void timerCallback() override;

    ProjectService& projectService;
    engine::EngineContext& engineContext;
    std::chrono::seconds interval;
    juce::Time lastAutosaveTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutosaveService)
};

} // namespace fk::services
