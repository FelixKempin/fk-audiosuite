#include "fk/services/MidiService.h"
#include "fk/services/PluginService.h"

#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("TrackService: Instrumentenspur mit FK Synth, MIDI-Eingänge wählbar", "[services][midi]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiService midiService { env.engineContext };
    midiService.ensureVirtualKeyboardExists();
    TestEnvironment::pumpMessages(); // Geräteliste entsteht asynchron, erst danach das Projekt öffnen

    REQUIRE (env.projectService.createProject (env.tempFolder, "Synth").has_value());

    const auto instruments = pluginService.getAvailableInstruments();
    REQUIRE (instruments.size() >= 2); // FK Sampler und FK Synth, eigene zuerst
    const auto synthId = TestEnvironment::synthId (pluginService);
    REQUIRE (synthId.isNotEmpty());

    const auto created = env.trackService.createInstrumentTrack ("Lead", synthId);
    REQUIRE (created.has_value());

    const auto info = env.trackService.getTrackInfo (**created);
    REQUIRE (info.isInstrument);
    REQUIRE (info.name == "Lead");

    // Das Instrument ist kein Insert, die Insert-Slots bleiben frei.
    REQUIRE (pluginService.getInserts (**created).empty());

    const auto midiInputs = env.trackService.getAvailableMidiInputs();
    bool hasVirtualKeyboard = false;
    for (const auto& input : midiInputs)
        hasVirtualKeyboard = hasVirtualKeyboard || input.name == fk::services::MidiService::getVirtualKeyboardName();
    REQUIRE (hasVirtualKeyboard);

    for (const auto& input : midiInputs)
        if (input.name == fk::services::MidiService::getVirtualKeyboardName())
            REQUIRE (env.trackService.setAudioInput (**created, input.id).has_value());

    REQUIRE (env.trackService.getTrackInfo (**created).inputId.isNotEmpty());

    const auto unknown = env.trackService.createInstrumentTrack ("X", "gibt-es-nicht");
    REQUIRE_FALSE (unknown.has_value());
}
