#include "fk/services/SendService.h"

#include "fk/services/PluginService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::TrackKind;
using fk::tests::TestEnvironment;

TEST_CASE ("SendService: FX-Spur hat Bus und Aux-Return, Sends entstehen beim ersten Pegel", "[services][sends]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::SendService sends { env.projectService, env.trackService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Sends").has_value());

    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* reverb = *sends.createFxTrack ("Hall");
    auto* delay = *sends.createFxTrack ("Delay");

    REQUIRE (env.trackService.getTrackInfo (*reverb).kind == TrackKind::fx);
    const auto fxTracks = sends.getFxTracks();
    REQUIRE (fxTracks.size() == 2);
    REQUIRE (fxTracks[0].bus == 0);
    REQUIRE (fxTracks[1].bus == 1);

    // FX-Spuren haben keine Sends, normale Spuren einen Eintrag je FX-Spur (noch ohne Plugin).
    REQUIRE (sends.getSends (*reverb).empty());
    auto vocalSends = sends.getSends (*vocals);
    REQUIRE (vocalSends.size() == 2);
    REQUIRE_FALSE (vocalSends[0].exists);

    REQUIRE (sends.setSendLevel (*vocals, fxTracks[0].id, -6.0f).has_value());
    vocalSends = sends.getSends (*vocals);
    REQUIRE (vocalSends[0].exists);
    REQUIRE (vocalSends[0].gainDb == Catch::Approx (-6.0f).margin (0.1f));
    REQUIRE_FALSE (vocalSends[0].preFader);
    REQUIRE_FALSE (vocalSends[1].exists);

    REQUIRE (sends.setSendPreFader (*vocals, fxTracks[0].id, true).has_value());
    REQUIRE (sends.getSends (*vocals)[0].preFader);
    REQUIRE (sends.setSendPreFader (*vocals, fxTracks[0].id, false).has_value());
    REQUIRE_FALSE (sends.getSends (*vocals)[0].preFader);

    // Ein Insert auf der FX-Spur landet hinter dem Aux-Return, nicht davor.
    REQUIRE (pluginService.insertPlugin (*reverb, pluginService.getAvailableEffects().front().id).has_value());
    REQUIRE (pluginService.getInserts (*reverb).size() == 1);

    REQUIRE (sends.removeSend (*vocals, fxTracks[0].id).has_value());
    REQUIRE_FALSE (sends.getSends (*vocals)[0].exists);
    REQUIRE_FALSE (sends.setSendLevel (*vocals, "gibt-es-nicht", 0.0f).has_value());
    juce::ignoreUnused (delay);
}

TEST_CASE ("SendService: Sends und FX-Spuren überleben Speichern und Öffnen", "[services][sends]")
{
    TestEnvironment env;
    fk::services::SendService sends { env.projectService, env.trackService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "SendsPersist").has_value());

    auto* track = *env.trackService.createAudioTrack ("Keys", 2);
    auto* fx = *sends.createFxTrack ("Hall");
    REQUIRE (sends.setSendLevel (*track, env.trackService.getTrackId (*fx), -12.0f).has_value());
    REQUIRE (sends.setSendPreFader (*track, env.trackService.getTrackId (*fx), true).has_value());
    REQUIRE (env.projectService.saveProject().has_value());

    const auto file = env.tempFolder.getChildFile ("SendsPersist/SendsPersist.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());

    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 2);
    REQUIRE (env.trackService.getTrackInfo (*tracks[1]).kind == TrackKind::fx);
    const auto restored = sends.getSends (*tracks[0]);
    REQUIRE (restored.size() == 1);
    REQUIRE (restored[0].exists);
    REQUIRE (restored[0].gainDb == Catch::Approx (-12.0f).margin (0.1f));
    REQUIRE (restored[0].preFader);
}
