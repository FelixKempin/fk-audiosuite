#include "fk/services/BakeService.h"

#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
/// Instrumentenspur (FK Synth) mit einem Takt MIDI und zwei Inserts (FK Gain, FK EQ).
struct BakeProject
{
    explicit BakeProject (TestEnvironment& environment)
        : env (environment)
    {
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Bake").has_value());
        track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
        clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0);
        REQUIRE (midiClips.addNote (clipId, 60, 0.0, 2.0, 100).has_value());
        REQUIRE (plugins.insertPlugin (*track, effectId ("FK Gain")).has_value());
        REQUIRE (plugins.insertPlugin (*track, effectId ("FK EQ")).has_value());
    }

    juce::String effectId (const juce::String& name) const
    {
        for (const auto& effect : plugins.getAvailableEffects())
            if (effect.name == name)
                return effect.id;

        return {};
    }

    TestEnvironment& env;
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::BakeService bake { env.projectService, env.trackService };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};
} // namespace

TEST_CASE ("BakeService: Bake Track friert nur das Instrument ein, Inserts bleiben live", "[services][bake]")
{
    TestEnvironment env;
    BakeProject project (env);

    REQUIRE (project.bake.bakeTrack (*project.track).has_value());

    const auto state = project.bake.getBakeState (*project.track);
    REQUIRE (state.mode == fk::services::BakeMode::track);
    REQUIRE (state.bakedInserts == 0);
    REQUIRE (env.trackService.getTrackInfo (*project.track).baked);
    REQUIRE (fk::services::te::TemporaryFileManager::getFreezeFileForTrack (*project.track).existsAsFile());

    // Instrument und MIDI gesperrt, Inserts bearbeitbar.
    REQUIRE (project.plugins.getInstrument (*project.track)->baked);
    REQUIRE_FALSE (project.midiClips.addNote (project.clipId, 64, 1.0, 1.0, 100).has_value());
    REQUIRE_FALSE (env.clipService.moveClip (project.clipId, 4.0).has_value());
    const auto inserts = project.plugins.getInserts (*project.track);
    REQUIRE_FALSE (inserts[0].baked);
    REQUIRE (project.plugins.setInsertEnabled (inserts[0].id, false).has_value());

    // Lösen: alles wieder live, Datei weg.
    REQUIRE (project.bake.unbake (*project.track).has_value());
    REQUIRE (project.bake.getBakeState (*project.track).mode == fk::services::BakeMode::none);
    REQUIRE_FALSE (fk::services::te::TemporaryFileManager::getFreezeFileForTrack (*project.track).existsAsFile());
    REQUIRE (project.midiClips.addNote (project.clipId, 64, 1.0, 1.0, 100).has_value());
    REQUIRE_FALSE (project.plugins.getInstrument (*project.track)->baked);
}

TEST_CASE ("BakeService: Bake Inserts backt linear bis zum gewählten Slot", "[services][bake]")
{
    TestEnvironment env;
    BakeProject project (env);

    REQUIRE (project.bake.bakeInserts (*project.track, 1).has_value());
    REQUIRE (project.bake.getBakeState (*project.track).mode == fk::services::BakeMode::inserts);
    REQUIRE (project.bake.getBakeState (*project.track).bakedInserts == 1);

    auto inserts = project.plugins.getInserts (*project.track);
    REQUIRE (inserts[0].baked);       // FK Gain: gebacken
    REQUIRE_FALSE (inserts[1].baked); // FK EQ: live
    REQUIRE_FALSE (project.plugins.removeInsert (inserts[0].id).has_value());
    REQUIRE (project.plugins.setInsertEnabled (inserts[1].id, false).has_value());

    // Ein neuer Insert landet hinter den gebackenen und bleibt live.
    REQUIRE (project.plugins.insertPlugin (*project.track, project.effectId ("FK Limiter")).has_value());
    inserts = project.plugins.getInserts (*project.track);
    REQUIRE (inserts.size() == 3);
    REQUIRE_FALSE (inserts.back().baked);

    // Alle Inserts (-1): neu backen ersetzt den alten Bake.
    REQUIRE (project.bake.bakeInserts (*project.track).has_value());
    REQUIRE (project.bake.getBakeState (*project.track).bakedInserts == 3);
    REQUIRE (project.bake.unbake (*project.track).has_value());

    for (const auto& insert : project.plugins.getInserts (*project.track))
        REQUIRE_FALSE (insert.baked);
}

TEST_CASE ("BakeService: Gruppen-Bake sperrt alle zuführenden Spuren, Spuren vor Gruppen lassen sich backen",
           "[services][bake]")
{
    TestEnvironment env;
    BakeProject project (env);
    auto* group = *env.trackService.createGroupTrack ("Band");
    REQUIRE (env.trackService.setOutput (*project.track, env.trackService.getTrackId (*group)).has_value());

    // Eine Spur vor einer Gruppe: Tracktion allein lehnt das ab, der Service leitet kurz um.
    REQUIRE (project.bake.bakeTrack (*project.track).has_value());
    REQUIRE (env.trackService.getOutputId (*project.track) == env.trackService.getTrackId (*group));
    REQUIRE (project.bake.unbake (*project.track).has_value());

    REQUIRE (project.bake.bakeInserts (*group).has_value());
    REQUIRE (project.bake.getBakeState (*group).mode == fk::services::BakeMode::group);

    const auto state = project.bake.getBakeState (*project.track);
    REQUIRE (state.lockedByGroup);
    REQUIRE (state.lockingGroupName == "Band");
    REQUIRE (env.trackService.getTrackInfo (*project.track).bakedIntoGroup == "Band");
    REQUIRE_FALSE (project.midiClips.addNote (project.clipId, 64, 1.0, 1.0, 100).has_value());
    REQUIRE_FALSE (project.plugins.insertPlugin (*project.track, project.effectId ("FK Limiter")).has_value());
    REQUIRE_FALSE (project.bake.bakeTrack (*project.track).has_value()); // erst die Gruppe lösen

    REQUIRE (project.bake.unbake (*group).has_value());
    REQUIRE_FALSE (project.bake.getBakeState (*project.track).lockedByGroup);
    REQUIRE (project.midiClips.addNote (project.clipId, 64, 1.0, 1.0, 100).has_value());
}
