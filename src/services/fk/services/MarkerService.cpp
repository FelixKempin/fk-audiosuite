#include "fk/services/MarkerService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr double previousMarkerTolerance = 0.25; // Sekunden

MarkerInfo toInfo (te::MarkerClip& marker)
{
    return {
        marker.itemID.toString(), marker.getMarkerID(), marker.getName(), marker.getPosition().getStart().inSeconds()
    };
}
} // namespace

MarkerService::MarkerService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

MarkerService::~MarkerService()
{
    if (observedEdit != nullptr && projectService.getEdit() == observedEdit)
        observedEdit->getMarkerManager().removeChangeListener (this);

    projectService.removeListener (*this);
}

std::vector<MarkerInfo> MarkerService::getMarkers() const
{
    std::vector<MarkerInfo> markers;

    if (auto* edit = projectService.getEdit())
        for (auto* marker : edit->getMarkerManager().getMarkers())
            markers.push_back (toInfo (*marker));

    std::ranges::sort (markers,
                       [] (const MarkerInfo& a, const MarkerInfo& b) { return a.startSeconds < b.startSeconds; });
    return markers;
}

core::Result<juce::String> MarkerService::addMarker (double seconds, const juce::String& name)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Add marker"));
    edit->ensureMarkerTrack();
    auto& manager = edit->getMarkerManager();
    const int number = manager.getNextUniqueID();
    auto marker = manager.createMarker (
        number, te::TimePosition::fromSeconds (std::max (seconds, 0.0)), te::TimeDuration(), nullptr);

    if (marker == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the marker.").toStdString());

    marker->setName (name.isNotEmpty() ? name : plugins::i18n::format (TRANS ("Marker {number}"), { { "number", juce::String (number) } }));
    return marker->itemID.toString();
}

core::Result<void> MarkerService::moveMarker (const juce::String& markerId, double seconds)
{
    auto* marker = findMarker (markerId);

    if (marker == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Marker not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Move marker"));
    marker->setStart (te::TimePosition::fromSeconds (std::max (seconds, 0.0)), false, true);
    return {};
}

core::Result<void> MarkerService::renameMarker (const juce::String& markerId, const juce::String& name)
{
    auto* marker = findMarker (markerId);

    if (marker == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Marker not found.").toStdString());

    if (name.trim().isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The marker needs a name.").toStdString());

    projectService.beginUndoStep (TRANS ("Rename marker"));
    marker->setName (name.trim());
    return {};
}

core::Result<void> MarkerService::deleteMarker (const juce::String& markerId)
{
    auto* marker = findMarker (markerId);

    if (marker == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Marker not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Delete marker"));
    marker->removeFromParent();
    return {};
}

std::optional<double> MarkerService::getNextMarkerSeconds (double seconds) const
{
    for (const auto& marker : getMarkers())
        if (marker.startSeconds > seconds + 0.001)
            return marker.startSeconds;

    return std::nullopt;
}

std::optional<double> MarkerService::getPreviousMarkerSeconds (double seconds) const
{
    std::optional<double> previous;

    for (const auto& marker : getMarkers())
        if (marker.startSeconds < seconds - previousMarkerTolerance)
            previous = marker.startSeconds;

    return previous;
}

std::optional<std::pair<double, double>> MarkerService::getSection (const juce::String& markerId) const
{
    const auto markers = getMarkers();
    const auto it = std::ranges::find (markers, markerId, &MarkerInfo::id);

    if (it == markers.end())
        return std::nullopt;

    if (std::next (it) != markers.end())
        return std::pair { it->startSeconds, std::next (it)->startSeconds };

    // Letzter Marker: so lang, wie Tracktion ihn angelegt hat (mindestens ein Takt).
    if (const auto* marker = findMarker (markerId))
        return std::pair { it->startSeconds, marker->getPosition().getEnd().inSeconds() };

    return std::nullopt;
}

void MarkerService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void MarkerService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void MarkerService::projectChanged()
{
    // Das alte Edit ist zu diesem Zeitpunkt schon aus dem ProjectService genommen; seinen
    // MarkerManager gibt es aber noch (das Edit stirbt erst nach der Benachrichtigung).
    if (observedEdit != nullptr)
        observedEdit->getMarkerManager().removeChangeListener (this);

    observedEdit = projectService.getEdit();

    if (observedEdit != nullptr)
        observedEdit->getMarkerManager().addChangeListener (this);

    listeners.call ([] (Listener& listener) { listener.markersChanged(); });
}

void MarkerService::changeListenerCallback (juce::ChangeBroadcaster*)
{
    listeners.call ([] (Listener& listener) { listener.markersChanged(); });
}

te::MarkerClip* MarkerService::findMarker (const juce::String& markerId) const
{
    if (auto* edit = projectService.getEdit())
        for (auto* marker : edit->getMarkerManager().getMarkers())
            if (marker->itemID.toString() == markerId)
                return marker; // gehört der Markerspur, lebt länger als die Liste

    return nullptr;
}

} // namespace fk::services
