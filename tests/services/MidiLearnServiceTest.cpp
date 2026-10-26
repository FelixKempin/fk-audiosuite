#include "fk/services/MidiLearnService.h"

#include "fk/services/MidiService.h"
#include "fk/services/MixerService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("MidiLearnService: Parameter anfassen, Regler bewegen, Zuordnung steht und steuert", "[services][midilearn]")
{
    TestEnvironment env;
    fk::services::MidiLearnService learn { env.engineContext, env.projectService };
    fk::services::MixerService mixer { env.projectService };
    int changes = 0;
    struct Counter final : fk::services::MidiLearnService::Listener
    {
        explicit Counter (int& c)
            : count (c)
        {
        }
        void midiLearnChanged() override { ++count; }
        int& count;
    } counter { changes };
    learn.addListener (counter);

    REQUIRE (env.projectService.createProject (env.tempFolder, "Learn").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang");
    auto* edit = env.projectService.getEdit();
    REQUIRE (learn.getMappings().empty());
    REQUIRE (learn.getLearnHint().isEmpty());

    // Lernmodus an, Fader anfassen (Parameteränderung mit Benachrichtigung), dann CC 7 auf Kanal 1 bewegen.
    learn.setLearnActive (true);
    REQUIRE (learn.isLearnActive());
    REQUIRE (learn.getLearnHint().isNotEmpty());
    mixer.setVolumeDb (*track, -6.0f);
    TestEnvironment::pumpMessages (20);
    REQUIRE (learn.getLearnHint().contains ("hardware"));

    edit->getParameterControlMappings().sendChange (0x10000 + 7, 0.5f, 1);
    TestEnvironment::pumpMessages (50);

    const auto mappings = learn.getMappings();
    REQUIRE (mappings.size() == 1);
    REQUIRE (mappings[0].controllerName.startsWith ("CC 7"));
    REQUIRE (changes > 0);
    learn.setLearnActive (false);

    // Die Zuordnung steuert den Fader: Regler ganz hoch = Maximum der Lautstärke.
    edit->getParameterControlMappings().sendChange (0x10000 + 7, 1.0f, 1);
    TestEnvironment::pumpMessages (20);
    const auto maxDb = mixer.getVolumeDb (*track);
    edit->getParameterControlMappings().sendChange (0x10000 + 7, 0.0f, 1);
    TestEnvironment::pumpMessages (20);
    REQUIRE (mixer.getVolumeDb (*track) < maxDb);

    // Die Zuordnung steht im Projekt und lässt sich entfernen.
    REQUIRE (edit->state.getChildWithName (tracktion::IDs::CONTROLLERMAPPINGS).getNumChildren() == 1);
    REQUIRE_FALSE (learn.removeMapping (5).has_value());
    REQUIRE (learn.removeMapping (0).has_value());
    REQUIRE (learn.getMappings().empty());
    learn.removeListener (counter);
}

TEST_CASE ("MidiService: Geräteliste enthält das Bildschirm-Keyboard und lässt sich schalten", "[services][midi]")
{
    TestEnvironment env;
    fk::services::MidiService midi { env.engineContext };
    midi.ensureVirtualKeyboardExists();
    TestEnvironment::pumpMessages (100);

    const auto devices = midi.getInputDevices();
    const auto keyboard = std::find_if (devices.begin(),
                                        devices.end(),
                                        [] (const fk::services::MidiDeviceInfo& device)
                                        { return device.name == fk::services::MidiService::getVirtualKeyboardName(); });
    REQUIRE (keyboard != devices.end());
    REQUIRE (keyboard->isVirtual);

    int notifications = 0;
    struct Counter final : fk::services::MidiService::Listener
    {
        explicit Counter (int& c)
            : count (c)
        {
        }
        void midiDevicesChanged() override { ++count; }
        int& count;
    } counter { notifications };
    midi.addListener (counter);

    midi.setInputDeviceEnabled (keyboard->id, false);
    REQUIRE (notifications >= 1);
    for (const auto& device : midi.getInputDevices())
        if (device.id == keyboard->id)
            REQUIRE_FALSE (device.enabled);

    midi.setInputDeviceEnabled (keyboard->id, true);
    for (const auto& device : midi.getInputDevices())
        if (device.id == keyboard->id)
            REQUIRE (device.enabled);

    midi.removeListener (counter);
}
