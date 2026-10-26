#include "fk/services/PluginService.h"

#include "fk/services/MixerService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{

struct PluginEnvironment
{
    explicit PluginEnvironment (TestEnvironment& environment)
        : env (environment)
    {
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Plugins").has_value());
        track = *env.trackService.createAudioTrack ("Vocals");
    }

    juce::String idOf (const juce::String& pluginName) const
    {
        for (const auto& effect : pluginService.getAvailableEffects())
            if (effect.name == pluginName)
                return effect.id;

        return {};
    }

    TestEnvironment& env;
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MixerService mixerService { env.projectService };
    fk::services::TrackHandle* track = nullptr;
};

} // namespace

TEST_CASE ("PluginService: die fünf eigenen Effekte sind verfügbar", "[services][plugins]")
{
    TestEnvironment env;
    PluginEnvironment plugins (env);

    const auto effects = plugins.pluginService.getAvailableEffects();
    REQUIRE (effects.size() >= 5);
    REQUIRE (plugins.idOf ("FK Gain").isNotEmpty());
    REQUIRE (plugins.idOf ("FK EQ").isNotEmpty());
    REQUIRE (plugins.idOf ("FK Compressor").isNotEmpty());
    REQUIRE (plugins.idOf ("FK Limiter").isNotEmpty());
    REQUIRE (plugins.idOf ("FK Distortion").isNotEmpty());

    // Eigene Plugins stehen vorn, mit Format "FK" und Hersteller; danach alphabetisch.
    REQUIRE (effects.front().format == "FK");
    REQUIRE (effects.front().manufacturer == "FK Audio Suite");
    REQUIRE (effects[0].name.compareIgnoreCase (effects[1].name) < 0);
}

TEST_CASE ("PluginService: einsetzen, umgehen, entfernen", "[services][plugins]")
{
    TestEnvironment env;
    PluginEnvironment plugins (env);

    const auto insertId = plugins.pluginService.insertPlugin (*plugins.track, plugins.idOf ("FK Compressor"));
    REQUIRE (insertId.has_value());

    auto inserts = plugins.pluginService.getInserts (*plugins.track);
    REQUIRE (inserts.size() == 1);
    REQUIRE (inserts[0].name == "FK Compressor");
    REQUIRE (inserts[0].enabled);

    REQUIRE (plugins.pluginService.setInsertEnabled (*insertId, false).has_value());
    REQUIRE_FALSE (plugins.pluginService.getInserts (*plugins.track)[0].enabled);

    REQUIRE (plugins.pluginService.removeInsert (*insertId).has_value());
    REQUIRE (plugins.pluginService.getInserts (*plugins.track).empty());

    const auto unknown = plugins.pluginService.insertPlugin (*plugins.track, "gibt-es-nicht");
    REQUIRE_FALSE (unknown.has_value());
}

TEST_CASE ("PluginService: Reihenfolge der Inserts bleibt, Editor ist erzeugbar", "[services][plugins]")
{
    TestEnvironment env;
    PluginEnvironment plugins (env);

    REQUIRE (plugins.pluginService.insertPlugin (*plugins.track, plugins.idOf ("FK EQ")).has_value());
    const auto second = plugins.pluginService.insertPlugin (*plugins.track, plugins.idOf ("FK Limiter"));
    REQUIRE (second.has_value());

    const auto inserts = plugins.pluginService.getInserts (*plugins.track);
    REQUIRE (inserts.size() == 2);
    REQUIRE (inserts[0].name == "FK EQ");
    REQUIRE (inserts[1].name == "FK Limiter");

    auto editor = plugins.pluginService.createEditor (*second);
    REQUIRE (editor.has_value());
    REQUIRE (*editor != nullptr);
    editor->reset(); // EditorPtr meldet dem Prozessor das Löschen
}

TEST_CASE ("PluginService: Inserts überleben Speichern und Öffnen", "[services][plugins]")
{
    TestEnvironment env;
    PluginEnvironment plugins (env);

    REQUIRE (plugins.pluginService.insertPlugin (*plugins.track, plugins.idOf ("FK Distortion")).has_value());
    REQUIRE (env.projectService.saveProject().has_value());

    const auto editFile = env.tempFolder.getChildFile ("Plugins/Plugins.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile).has_value());

    const auto tracks = env.trackService.getAudioTracks();
    REQUIRE (tracks.size() == 1);
    const auto inserts = plugins.pluginService.getInserts (*tracks.front());
    REQUIRE (inserts.size() == 1);
    REQUIRE (inserts[0].name == "FK Distortion");
}

TEST_CASE ("MixerService: Lautstärke und Panorama pro Spur und Master", "[services][mixer]")
{
    TestEnvironment env;
    PluginEnvironment plugins (env);

    plugins.mixerService.setVolumeDb (*plugins.track, -6.0f);
    plugins.mixerService.setPan (*plugins.track, 0.5f);
    plugins.mixerService.setMasterVolumeDb (-3.0f);

    REQUIRE (plugins.mixerService.getVolumeDb (*plugins.track) == Catch::Approx (-6.0f).margin (0.01f));
    REQUIRE (plugins.mixerService.getPan (*plugins.track) == Catch::Approx (0.5f).margin (0.01f));
    REQUIRE (plugins.mixerService.getMasterVolumeDb() == Catch::Approx (-3.0f).margin (0.01f));

    plugins.mixerService.setPan (*plugins.track, 7.0f); // wird begrenzt
    REQUIRE (plugins.mixerService.getPan (*plugins.track) == Catch::Approx (1.0f));

    const auto levels = plugins.mixerService.getLevels (*plugins.track);
    REQUIRE (levels.leftDb <= 0.0f);
}
