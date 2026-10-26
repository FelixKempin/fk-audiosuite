#include "fk/services/AutomationService.h"

#include "fk/services/PluginService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::services::AutomationMode;
using fk::tests::TestEnvironment;

namespace
{
struct CountingListener final : fk::services::AutomationService::Listener
{
    int calls = 0;
    void automationChanged() override { ++calls; }
};

juce::String findId (const std::vector<fk::services::AutomationParameterInfo>& parameters, const juce::String& name)
{
    for (const auto& parameter : parameters)
        if (parameter.name == name)
            return parameter.id;

    return {};
}
} // namespace

TEST_CASE ("AutomationService: Spur bietet Lautstärke, Pan und Plugin-Parameter an", "[services][automation]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::AutomationService automation { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Automation").has_value());

    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    auto parameters = automation.getParameters (*track);

    REQUIRE (parameters.size() >= 2);
    REQUIRE (parameters[0].name == "Volume");
    REQUIRE (parameters[0].ownerName.isEmpty());
    REQUIRE (parameters[1].name == "Pan");
    REQUIRE (parameters[1].minValue == Catch::Approx (-1.0f));
    REQUIRE (parameters[1].maxValue == Catch::Approx (1.0f));

    // Ein Insert bringt seine Parameter mit, benannt nach dem Plugin.
    const auto effects = pluginService.getAvailableEffects();
    REQUIRE_FALSE (effects.empty());
    REQUIRE (pluginService.insertPlugin (*track, effects.front().id).has_value());

    parameters = automation.getParameters (*track);
    bool foundPluginParameter = false;

    for (const auto& parameter : parameters)
        foundPluginParameter = foundPluginParameter || parameter.ownerName == effects.front().name;

    REQUIRE (foundPluginParameter);
    REQUIRE (automation.getNumLanes (*track) == 0);
}

TEST_CASE ("AutomationService: Punkte setzen, verschieben, lesen, entfernen und rückgängig machen",
           "[services][automation]")
{
    TestEnvironment env;
    fk::services::AutomationService automation { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Punkte").has_value());

    auto* track = *env.trackService.createAudioTrack ("Keys", 2);
    const auto panId = findId (automation.getParameters (*track), "Pan");
    REQUIRE (panId.isNotEmpty());

    CountingListener listener;
    automation.addListener (listener);

    REQUIRE (automation.addPoint (*track, panId, 2.0, -1.0f).has_value());
    REQUIRE (automation.addPoint (*track, panId, 4.0, 1.0f).has_value());
    REQUIRE (automation.addPoint (*track, panId, 0.0, 5.0f).has_value()); // wird auf +1 begrenzt
    TestEnvironment::pumpMessages (50);

    auto points = automation.getPoints (*track, panId);
    REQUIRE (points.size() == 3);
    REQUIRE (points[0].seconds == Catch::Approx (0.0));
    REQUIRE (points[0].value == Catch::Approx (1.0f));
    REQUIRE (points[1].seconds == Catch::Approx (2.0));
    REQUIRE (points[2].seconds == Catch::Approx (4.0));
    REQUIRE (listener.calls >= 1);

    // Zwischen den Punkten wird linear interpoliert.
    REQUIRE (automation.getValueAt (*track, panId, 3.0) == Catch::Approx (0.0f).margin (0.01));
    REQUIRE (automation.getParameter (*track, panId)->hasPoints);

    // Punkte überholen ihre Nachbarn nicht: Ziehen hinter den letzten Punkt endet an dessen Zeit,
    // der Index bleibt. Der Wert wird übernommen.
    automation.beginChange ("Punkt verschieben");
    const auto moved = automation.movePoint (*track, panId, 1, 6.0, 0.5f);
    REQUIRE (moved.has_value());
    REQUIRE (*moved == 1);
    points = automation.getPoints (*track, panId);
    REQUIRE (points[1].seconds == Catch::Approx (4.0));
    REQUIRE (points[1].value == Catch::Approx (0.5f));

    REQUIRE (automation.removePoint (*track, panId, 0).has_value());
    REQUIRE (automation.getPoints (*track, panId).size() == 2);
    REQUIRE_FALSE (automation.removePoint (*track, panId, 7).has_value());

    env.projectService.undo();
    REQUIRE (automation.getPoints (*track, panId).size() == 3);

    REQUIRE (automation.clearPoints (*track, panId).has_value());
    REQUIRE (automation.getPoints (*track, panId).empty());

    automation.removeListener (listener);
}

TEST_CASE ("AutomationService: angezeigter Parameter, Modus je Spur und Lesen/Schreiben global",
           "[services][automation]")
{
    TestEnvironment env;
    fk::services::AutomationService automation { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Modus").has_value());

    auto* track = *env.trackService.createAudioTrack ("Bass", 1);
    const auto volumeId = findId (automation.getParameters (*track), "Volume");

    // Lagen: hinzufügen, Parameter setzen, entfernen; Undo nimmt die letzte Änderung zurück.
    automation.addLane (*track);
    automation.addLane (*track, volumeId);
    REQUIRE (automation.getNumLanes (*track) == 2);
    REQUIRE_FALSE (automation.getLaneParameter (*track, 0).has_value());
    REQUIRE (automation.getLaneParameter (*track, 1)->id == volumeId);

    automation.setLaneParameter (*track, 0, volumeId);
    REQUIRE (automation.getLaneParameter (*track, 0)->id == volumeId);
    automation.setLaneParameter (*track, 0, {});
    REQUIRE_FALSE (automation.getLaneParameter (*track, 0).has_value());

    automation.removeLane (*track, 0);
    REQUIRE (automation.getNumLanes (*track) == 1);
    REQUIRE (automation.getLaneParameter (*track, 0)->id == volumeId);
    automation.removeLane (*track, 7); // ungültig: keine Änderung
    REQUIRE (automation.getNumLanes (*track) == 1);

    env.projectService.undo();
    REQUIRE (automation.getNumLanes (*track) == 2);

    REQUIRE (automation.getMode (*track) != AutomationMode::write);
    automation.setMode (*track, AutomationMode::write);
    REQUIRE (automation.getMode (*track) == AutomationMode::write);
    REQUIRE (fk::services::AutomationService::getModeName (AutomationMode::latch) == "Latch");

    REQUIRE (automation.isReadEnabled()); // Tracktion liest Kurven standardmäßig
    automation.setWriteEnabled (true);
    REQUIRE (automation.isWriteEnabled());
    automation.setWriteEnabled (false);
    REQUIRE_FALSE (automation.isWriteEnabled());
    automation.setReadEnabled (false);
    REQUIRE_FALSE (automation.isReadEnabled());
}

TEST_CASE ("AutomationService: Automationsspuren überleben Speichern und Öffnen", "[services][automation]")
{
    TestEnvironment env;
    fk::services::AutomationService automation { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Lagen").has_value());

    auto* track = *env.trackService.createAudioTrack ("Pad", 2);
    const auto panId = findId (automation.getParameters (*track), "Pan");
    automation.addLane (*track, panId);
    automation.addLane (*track);
    REQUIRE (env.projectService.saveProject().has_value());

    const auto editFile =
        env.projectService.getProjectFolder().findChildFiles (juce::File::findFiles, false, "*.tracktionedit");
    REQUIRE (editFile.size() == 1);
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (editFile[0]).has_value());

    auto* reopened = env.trackService.getAudioTracks().front();
    REQUIRE (automation.getNumLanes (*reopened) == 2);
    REQUIRE (automation.getLaneParameter (*reopened, 0)->name == "Pan");
    REQUIRE_FALSE (automation.getLaneParameter (*reopened, 1).has_value());
}
