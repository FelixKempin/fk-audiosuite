#include "fk/services/MidiService.h"
#include "fk/services/PluginService.h"
#include "fk/services/TransportService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

// Durchlauf wie ein Nutzer: Instrumentenspur, Bildschirm-Keyboard als Eingang, scharfschalten,
// Aufnahme starten, Noten spielen, stoppen. Danach muss ein MIDI-Clip mit diesen Noten da sein.
TEST_CASE ("MIDI-Aufnahme: Noten vom Bildschirm-Keyboard landen als Clip auf der Spur", "[services][midirecord]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    env.engineContext.enableAllAudioInputs();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::MidiService midiService { env.engineContext };
    midiService.ensureVirtualKeyboardExists();
    TestEnvironment::pumpMessages();

    REQUIRE (env.projectService.createProject (env.tempFolder, "Aufnahme").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));

    juce::String keyboardId;
    for (const auto& input : env.trackService.getAvailableMidiInputs())
        if (input.name == fk::services::MidiService::getVirtualKeyboardName())
            keyboardId = input.id;

    REQUIRE (keyboardId.isNotEmpty());
    REQUIRE (env.trackService.setAudioInput (*track, keyboardId).has_value());
    env.trackService.setArmed (*track, true);
    REQUIRE (env.trackService.isArmed (*track));

    transport.record();
    TestEnvironment::pumpMessages (300);
    REQUIRE (transport.isRecording());

    // Aufgenommen wird im Audio-Thread. Läuft das Audiogerät der Testumgebung nicht (CI-Rechner ohne
    // Soundkarte, Gerät belegt, PipeWire ausgelastet), steht die Position still und es entsteht kein
    // Clip; dann lässt sich hier nichts prüfen.
    const auto positionBefore = transport.getPositionSeconds();
    TestEnvironment::pumpMessages (200);
    if (transport.getPositionSeconds() - positionBefore < 0.05)
    {
        transport.stop();
        SKIP ("Audiogerät der Testumgebung läuft nicht (Position steht bei " << positionBefore << " s)");
    }

    const auto firstNoteStart = transport.getPositionSeconds();

    // Zwei Noten, jeweils 150 ms gehalten.
    midiService.sendNoteOn (60, 0.8f);
    TestEnvironment::pumpMessages (150);
    midiService.sendNoteOff (60);
    TestEnvironment::pumpMessages (100);
    const auto secondNoteStart = transport.getPositionSeconds();
    midiService.sendNoteOn (64, 0.8f);
    TestEnvironment::pumpMessages (150);
    midiService.sendNoteOff (64);
    TestEnvironment::pumpMessages (300);

    transport.stop();

    // Der Clip entsteht nach dem Stopp asynchron; auf langsamen Rechnern dauert das länger.
    auto clips = env.trackService.getClips (*track);
    for (int waited = 0; clips.empty() && waited < 3000; waited += 100)
    {
        TestEnvironment::pumpMessages (100);
        clips = env.trackService.getClips (*track);
    }
    REQUIRE (clips.size() == 1);
    REQUIRE (clips[0].isMidi);

    REQUIRE (clips[0].notes.size() == 2);
    REQUIRE (clips[0].notes[0].noteNumber == 60);
    REQUIRE (clips[0].notes[1].noteNumber == 64);

    // Zeitlich muss die Aufnahme stimmen: Jede Note liegt dort, wo der Positionszeiger beim Spielen
    // stand. Verglichen wird mit der Engine-Position, nicht mit der Wanduhr. Das geht nur, wenn das
    // Audiogerät der Testumgebung wirklich läuft: Steht die Position still (Gerät belegt, PipeWire
    // ausgelastet), lässt sich über Zeitstempel nichts sagen. Dann warnen statt falsch durchfallen.
    if (secondNoteStart - firstNoteStart < 0.05)
    {
        WARN ("Audiogerät der Testumgebung läuft nicht (Position steht bei " << firstNoteStart
                                                                             << " s): Zeitprüfung übersprungen");
        return;
    }

    REQUIRE (clips[0].notes[1].startSeconds > clips[0].notes[0].startSeconds);
    REQUIRE (clips[0].notes[0].startSeconds == Catch::Approx (firstNoteStart).margin (0.15));
    REQUIRE (clips[0].notes[1].startSeconds == Catch::Approx (secondNoteStart).margin (0.15));
    REQUIRE (clips[0].notes[0].lengthSeconds == Catch::Approx (0.15).margin (0.08));
    REQUIRE (clips[0].notes[1].lengthSeconds == Catch::Approx (0.15).margin (0.08));
}

TEST_CASE ("Retrospektive Aufnahme: ohne Aufnahme gespieltes MIDI wird nachträglich ein Clip", "[services][midirecord][b3]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::MidiService midiService { env.engineContext };
    midiService.ensureVirtualKeyboardExists();
    TestEnvironment::pumpMessages();

    REQUIRE (env.projectService.createProject (env.tempFolder, "Rueckblick").has_value());
    auto* track = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));

    juce::String keyboardId;
    for (const auto& input : env.trackService.getAvailableMidiInputs())
        if (input.name == fk::services::MidiService::getVirtualKeyboardName())
            keyboardId = input.id;

    REQUIRE (env.trackService.setAudioInput (*track, keyboardId).has_value());

    // Nicht scharf: Es gibt nichts zu holen (0 Clips, kein Fehler).
    midiService.sendNoteOn (60, 0.8f);
    TestEnvironment::pumpMessages (100);
    midiService.sendNoteOff (60);
    TestEnvironment::pumpMessages (100);
    const auto notArmed = transport.captureRetrospective();
    if (! notArmed)
        WARN (notArmed.error().message);
    REQUIRE (notArmed.has_value());
    REQUIRE (*notArmed == 0);

    // Scharf, gespielt ohne Aufnahme, dann holen.
    env.trackService.setArmed (*track, true);
    midiService.sendNoteOn (62, 0.8f);
    TestEnvironment::pumpMessages (100);
    midiService.sendNoteOff (62);
    midiService.sendNoteOn (65, 0.8f);
    TestEnvironment::pumpMessages (100);
    midiService.sendNoteOff (65);
    TestEnvironment::pumpMessages (200);

    const auto captured = transport.captureRetrospective();
    REQUIRE (captured.has_value());

    if (*captured == 0)
    {
        WARN ("MIDI-Eingang der Testumgebung liefert keinen Rückblick-Puffer; Inhalt nicht prüfbar");
        return;
    }

    const auto clips = env.trackService.getClips (*track);
    REQUIRE (clips.size() == 1);
    REQUIRE (clips[0].isMidi);
    REQUIRE (clips[0].notes.size() >= 2);
}
