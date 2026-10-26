#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <optional>

namespace fk::services::bakeLock
{

/// Sperrregeln für gebackene Spuren (ADR 0009) an einer Stelle, damit Plugin-, Clip- und
/// MIDI-Service dieselbe Antwort geben. Freie Funktionen statt Klasse: Sie haben keinen
/// Zustand, sie lesen nur den Zustand der Spur.

/// Die Spur ist selbst gebacken (Bake Track, Bake Inserts oder Gruppen-Bake).
[[nodiscard]] bool isBaked (const te::AudioTrack& track);

/// Die gebackene Gruppe, in die die Spur (auch über Zwischengruppen) mündet, sonst nullptr.
[[nodiscard]] te::AudioTrack* findBakedDestination (te::AudioTrack& track);

/// Clips der Spur sind gesperrt: Spur gebacken oder in eine gebackene Gruppe gebacken.
[[nodiscard]] bool areClipsLocked (te::AudioTrack& track);

/// Das Plugin ist gesperrt: eingefroren (vor dem Freeze-Punkt) oder seine Spur steckt in einer
/// gebackenen Gruppe.
[[nodiscard]] bool isPluginLocked (te::Plugin& plugin);

/// Fehlermeldung für einen abgelehnten Eingriff, mit dem Weg hinaus ("Bake lösen").
[[nodiscard]] core::Error lockedError (te::AudioTrack& track);

/// Prüfung für die Services: leer, wenn die Clips der Spur geändert werden dürfen.
[[nodiscard]] core::Result<void> checkClipsUnlocked (te::AudioTrack& track);

} // namespace fk::services::bakeLock
