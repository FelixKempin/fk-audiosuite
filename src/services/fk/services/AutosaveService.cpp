#include "fk/services/AutosaveService.h"

#include "fk/services/ErrorText.h"

#include <algorithm>

namespace fk::services
{

namespace
{
// Tracktion hat keine Einstellungs-ID für Fremde; als Eintrag unter "invalid" kollidiert nichts.
constexpr auto intervalItem = "fkAutosaveSeconds";
} // namespace

AutosaveService::AutosaveService (ProjectService& service, engine::EngineContext& context)
    : projectService (service)
    , engineContext (context)
    , interval (defaultInterval)
{
    const int stored = engineContext.getEngine().getPropertyStorage().getPropertyItem (
        te::SettingID::invalid, intervalItem, static_cast<int> (defaultInterval.count()));
    interval = std::chrono::seconds (std::max (stored, 0));

    if (interval.count() > 0)
        startTimer (static_cast<int> (std::chrono::milliseconds (interval).count()));
}

AutosaveService::~AutosaveService()
{
    stopTimer();
}

void AutosaveService::setInterval (std::chrono::seconds autosaveInterval)
{
    interval = std::max (autosaveInterval, std::chrono::seconds (0));

    if (interval.count() > 0)
        startTimer (static_cast<int> (std::chrono::milliseconds (interval).count()));
    else
        stopTimer();

    auto& storage = engineContext.getEngine().getPropertyStorage();
    storage.setPropertyItem (te::SettingID::invalid, intervalItem, static_cast<int> (interval.count()));
    storage.flushSettingsToDisk();
}

std::chrono::seconds AutosaveService::getInterval() const noexcept
{
    return interval;
}

juce::Time AutosaveService::getLastAutosaveTime() const noexcept
{
    return lastAutosaveTime;
}

void AutosaveService::autosaveNow()
{
    if (! projectService.isModified())
        return;

    // Ein Fehler hier darf die Arbeit nicht unterbrechen: protokollieren, beim nächsten Takt erneut versuchen.
    if (const auto result = projectService.writeAutosave(); ! result)
        juce::Logger::writeToLog (errorText (result.error()));
    else
        lastAutosaveTime = juce::Time::getCurrentTime();
}

void AutosaveService::timerCallback()
{
    autosaveNow();
}

} // namespace fk::services
