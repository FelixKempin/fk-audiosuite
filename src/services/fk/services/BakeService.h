#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

namespace fk::services
{

/// Was an einer Spur gebacken ist (ADR 0009).
enum class BakeMode
{
    none,    ///< alles live
    track,   ///< Bake Track: nur das Instrument, Inserts bleiben live
    inserts, ///< Bake Inserts: Instrument (falls vorhanden) und Inserts 1 bis bakedInserts
    group    ///< Gruppen-Bake: Gruppe samt allen zuführenden Spuren
};

/// Momentaufnahme für die Anzeige.
struct BakeState
{
    BakeMode mode = BakeMode::none;
    int bakedInserts = 0;       ///< bei inserts und group: so viele Inserts von vorn sind gebacken
    bool lockedByGroup = false; ///< Spur steckt in einer gebackenen Gruppe (gesperrt, aber nicht selbst gebacken)
    juce::String lockingGroupName;
    bool canBakeTrack = false;   ///< nur Instrumentenspuren
    bool canBakeInserts = false; ///< Audio-, Instrumenten- und Gruppenspuren
};

/// Track Bake: Teile einer Spur zu Audio rendern, damit sie keine Rechenleistung kosten, und
/// mit unbake wieder live schalten. Baut auf Tracktions Freeze auf (Freeze-Punkt in der
/// Plugin-Liste); Rendern blockiert, bis die Datei fertig ist. Bake ist kein Undo-Schritt.
/// Gesperrt wird in den anderen Services über bakeLock (BakeLock.h). Änderungen meldet der
/// TrackService (contentChanged), weil Tracktion den Bake-Zustand an der Spur speichert.
class BakeService final
{
public:
    BakeService (ProjectService& projectService, TrackService& trackService);

    /// Instrument rendern; Inserts bleiben live. Nur Instrumentenspuren.
    [[nodiscard]] core::Result<void> bakeTrack (TrackHandle& track);

    /// Inserts 1 bis insertCount rendern (auf Instrumentenspuren samt Instrument). Linear: Wer
    /// einzelne Slots markiert, übergibt den höchsten. insertCount < 0 = alle Inserts. Auf einer
    /// Gruppenspur backt das die Gruppe mit allen zuführenden Spuren.
    [[nodiscard]] core::Result<void> bakeInserts (TrackHandle& track, int insertCount = -1);

    /// Bake lösen: alles wieder live und bearbeitbar. Ohne Bake ohne Wirkung.
    [[nodiscard]] core::Result<void> unbake (TrackHandle& track);

    [[nodiscard]] BakeState getBakeState (TrackHandle& track) const;

private:
    /// Freeze-Punkt hinter das Plugin an pluginIndex (-1 = vor alle Plugins) setzen und rendern.
    [[nodiscard]] core::Result<void> bakeUpTo (TrackHandle& track, int pluginIndex, BakeMode mode);

    ProjectService& projectService;
    TrackService& tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BakeService)
};

} // namespace fk::services
