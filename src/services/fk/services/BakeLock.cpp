#include "fk/services/BakeLock.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services::bakeLock
{

bool isBaked (const te::AudioTrack& track)
{
    return track.isFrozen (te::Track::individualFreeze);
}

te::AudioTrack* findBakedDestination (te::AudioTrack& track)
{
    // Tracktion verhindert Routing-Schleifen; die Grenze schützt trotzdem vor einem Endlos-Lauf.
    constexpr int maxDepth = 64;
    auto* destination = track.getOutput().getDestinationTrack();

    for (int depth = 0; destination != nullptr && depth < maxDepth; ++depth)
    {
        if (isBaked (*destination))
            return destination;

        destination = destination->getOutput().getDestinationTrack();
    }

    return nullptr;
}

bool areClipsLocked (te::AudioTrack& track)
{
    return isBaked (track) || findBakedDestination (track) != nullptr;
}

bool isPluginLocked (te::Plugin& plugin)
{
    if (plugin.isFrozen())
        return true;

    if (auto* owner = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        return findBakedDestination (*owner) != nullptr;

    return false;
}

core::Error lockedError (te::AudioTrack& track)
{
    if (auto* group = findBakedDestination (track))
        return { core::ErrorCode::invalidArgument,
                 plugins::i18n::format (TRANS ("\"{track}\" is baked into the group \"{group}\". To edit it, unbake it there."),
                                        { { "track", track.getName() }, { "group", group->getName() } })
                     .toStdString() };

    return { core::ErrorCode::invalidArgument,
             plugins::i18n::format (TRANS ("\"{track}\" is baked. To edit it, unbake it first (track header or inspector)."),
                                    { { "track", track.getName() } })
                 .toStdString() };
}

core::Result<void> checkClipsUnlocked (te::AudioTrack& track)
{
    if (areClipsLocked (track))
        return std::unexpected (lockedError (track));

    return {};
}

} // namespace fk::services::bakeLock
