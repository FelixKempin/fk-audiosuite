#include "fk/plugins/SamplerProcessor.h"
#include "fk/services/BakeService.h"
#include "fk/services/ExportService.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"
#include "fk/services/RackService.h"
#include "fk/services/TransportService.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

#include <cmath>
#include <map>
#include <numbers>

using fk::tests::TestEnvironment;

namespace
{
/// Ein SFZ-Instrument mit drei Regionen: C3 und E3 auf Ausgang 1+2, D3 auf Ausgang 3+4.
juce::File writeMultiOutSfz (const juce::File& folder)
{
    folder.createDirectory();
    constexpr double sampleRate = 48000.0;
    juce::AudioBuffer<float> sine (1, static_cast<int> (sampleRate * 0.3));

    for (int i = 0; i < sine.getNumSamples(); ++i)
        sine.setSample (0, i, 0.8f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 440.0 * i / sampleRate)));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (folder.getChildFile ("sine.wav"));
    auto writer = format.createWriterFor (
        stream, juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16));
    writer->writeFromAudioSampleBuffer (sine, 0, sine.getNumSamples());
    writer.reset();

    const auto sfz = folder.getChildFile ("multi.sfz");
    sfz.replaceWithText ("<region> sample=sine.wav key=60 loop_mode=no_loop ampeg_release=0.01 output=0\n"
                         "<region> sample=sine.wav key=62 loop_mode=no_loop ampeg_release=0.01 output=1\n"
                         "<region> sample=sine.wav key=64 loop_mode=no_loop ampeg_release=0.01\n");
    return sfz;
}

/// Spitzenpegel einer Datei im Zeitfenster [from, to) in Sekunden.
float peakBetween (const juce::File& file, double from, double to)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    REQUIRE (reader != nullptr);

    const auto start = static_cast<juce::int64> (from * reader->sampleRate);
    const auto end = std::min (reader->lengthInSamples, static_cast<juce::int64> (to * reader->sampleRate));

    if (end <= start)
        return 0.0f;

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (end - start));
    reader->read (&buffer, 0, buffer.getNumSamples(), start, true, true);
    return buffer.getMagnitude (0, buffer.getNumSamples());
}

juce::String samplerId (const fk::services::PluginService& plugins)
{
    for (const auto& plugin : plugins.getAllPlugins())
        if (plugin.name == "FK Sampler")
            return plugin.id;

    return {};
}
} // namespace

TEST_CASE ("Rack-Instrument: MIDI-Spuren auf Kanälen, Ausgänge als eigene Kanäle", "[services][rack][b1]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::RackService racks { env.projectService, env.trackService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Rack").has_value());

    REQUIRE_FALSE (racks.createRackInstrument ("unbekannt", "X").has_value());

    const auto created = racks.createRackInstrument (samplerId (plugins), "Drums");
    REQUIRE (created.has_value());
    auto* midiOne = *created;

    // Ein Undo-Schritt für Rack, MIDI-Spur und Ausgangskanal.
    REQUIRE (env.projectService.getUndoHistory().done.back() == "Add rack instrument");

    auto rackList = racks.getRacks();
    REQUIRE (rackList.size() == 1);
    const auto rack = rackList.front();
    REQUIRE (rack.numOutputPairs == 8);
    REQUIRE (rack.activeOutputs[0]);
    REQUIRE_FALSE (rack.activeOutputs[1]);
    REQUIRE (rack.numMidiTracks == 1);
    REQUIRE (env.trackService.getAudioTracks().size() == 2); // MIDI-Spur + Ausgang 1+2

    // Rollen und Spurarten.
    REQUIRE (env.trackService.getTrackInfo (*midiOne).kind == fk::services::TrackKind::instrument);
    REQUIRE (racks.getRole (*midiOne)->midiChannel == 1);
    auto* output12 = env.trackService.getAudioTracks()[1];
    REQUIRE (env.trackService.getTrackInfo (*output12).kind == fk::services::TrackKind::rackOutput);
    REQUIRE (plugins.getInstrument (*midiOne)->id == rack.instrumentId); // Editor über die MIDI-Spur erreichbar

    // Ausgang 3+4 aktivieren; zweimal ist kein Fehler, ein Ausgang 9+10 gibt es nicht.
    REQUIRE (racks.setOutputActive (rack.id, 1, true).has_value());
    REQUIRE (racks.setOutputActive (rack.id, 1, true).has_value());
    REQUIRE_FALSE (racks.setOutputActive (rack.id, 8, true).has_value());
    REQUIRE (racks.getRack (rack.id)->activeOutputs[1]);
    auto* output34 = env.trackService.getAudioTracks().back();

    // Zweite MIDI-Spur bekommt automatisch Kanal 2.
    auto* midiTwo = *racks.addMidiTrack (rack.id);
    REQUIRE (racks.getRole (*midiTwo)->midiChannel == 2);
    REQUIRE_FALSE (racks.setMidiChannel (*midiTwo, 17).has_value());
    REQUIRE_FALSE (racks.setMidiChannel (*output34, 3).has_value());

    // Instrument laden: SFZ mit Regionen auf zwei Ausgängen.
    fk::plugins::SamplerProcessor* sampler = nullptr;

    for (auto* type : env.projectService.getEdit()->getRackList().getTypes())
        for (auto* plugin : type->getPlugins())
            if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
                sampler = dynamic_cast<fk::plugins::SamplerProcessor*> (external->getAudioPluginInstance());

    REQUIRE (sampler != nullptr);
    REQUIRE (sampler->loadSfz (writeMultiOutSfz (env.tempFolder.getChildFile ("sfz"))).has_value());

    // MIDI-Spur 1: C3 bei 0 s (Ausgang 1+2), D3 bei 1 s (Ausgang 3+4). MIDI-Spur 2 spielt dieselbe
    // Instanz: E3 bei 2 s. Bei 120 BPM ist ein Schlag 0,5 s.
    const auto clipOne = *midiClips.createEmptyClip (*midiOne, 0.0, 4.0);
    REQUIRE (midiClips.addNote (clipOne, 60, 0.0, 0.5, 100).has_value());
    REQUIRE (midiClips.addNote (clipOne, 62, 2.0, 0.5, 100).has_value());
    const auto clipTwo = *midiClips.createEmptyClip (*midiTwo, 0.0, 4.0);
    REQUIRE (midiClips.addNote (clipTwo, 64, 4.0, 0.5, 100).has_value());

    fk::services::ExportOptions options;
    options.file = env.tempFolder.getChildFile ("Stems");
    options.includeTails = false;
    const auto stems = exporter.exportStems (
        options, { env.trackService.getTrackId (*output12), env.trackService.getTrackId (*output34) });
    REQUIRE (stems.has_value());
    REQUIRE (stems->size() == 2);
    const auto& out12 = (*stems)[0];
    const auto& out34 = (*stems)[1];

    // Ausgang 1+2: C3 (0 s) und E3 von der zweiten MIDI-Spur (2 s), kein D3.
    REQUIRE (peakBetween (out12, 0.0, 0.3) > 0.05f);
    REQUIRE (peakBetween (out12, 1.0, 1.3) < 0.001f);
    REQUIRE (peakBetween (out12, 2.0, 2.3) > 0.05f);

    // Ausgang 3+4: nur D3 bei 1 s.
    REQUIRE (peakBetween (out34, 0.0, 0.3) < 0.001f);
    REQUIRE (peakBetween (out34, 1.0, 1.3) > 0.05f);
    REQUIRE (peakBetween (out34, 2.0, 2.3) < 0.001f);

    // Der Kanal steckt in der Patch Bay vor der Rack-Instanz: jeder eingehende Kanal → Kanal der Spur.
    // (sfizz wertet Kanäle nicht aus; Kontakt und Co. tun es, deshalb hier die Zuordnung selbst.)
    const auto channelOf = [] (fk::services::TrackHandle& track, int incoming)
    {
        for (auto* plugin : track.pluginList.getPlugins())
            if (auto* bay = dynamic_cast<tracktion::MidiPatchBayPlugin*> (plugin))
                return static_cast<int> (bay->getMappings().map[incoming]);

        return -1;
    };

    REQUIRE (channelOf (*midiTwo, 1) == 2);
    REQUIRE (channelOf (*midiTwo, 16) == 2);
    REQUIRE (channelOf (*midiOne, 5) == 1);
    REQUIRE (racks.setMidiChannel (*midiTwo, 10).has_value());
    REQUIRE (channelOf (*midiTwo, 1) == 10);
    REQUIRE (racks.getRole (*midiTwo)->midiChannel == 10);

    // Backen geht auf Rack-Spuren nicht (geteiltes Instrument).
    fk::services::BakeService bake { env.projectService, env.trackService };
    REQUIRE (env.trackService.getTrackInfo (*midiOne).inRack);
    REQUIRE_FALSE (bake.bakeTrack (*midiOne).has_value());
    REQUIRE_FALSE (bake.bakeInserts (*output12).has_value());

    // Ausgang 3+4 aus: Kanal verschwindet.
    REQUIRE (racks.setOutputActive (rack.id, 1, false).has_value());
    REQUIRE_FALSE (racks.getRack (rack.id)->activeOutputs[1]);
    REQUIRE (env.trackService.getAudioTracks().size() == 3);
}

TEST_CASE ("Normale Instrumentenspur: nur der Hauptausgang, Rack überlebt Speichern", "[services][rack][b1]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::RackService racks { env.projectService, env.trackService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Normal").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Sampler", samplerId (plugins));
    REQUIRE_FALSE (racks.getRole (*track).has_value());

    fk::plugins::SamplerProcessor* sampler = nullptr;

    for (auto* plugin : track->pluginList.getPlugins())
        if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
            sampler = dynamic_cast<fk::plugins::SamplerProcessor*> (external->getAudioPluginInstance());

    REQUIRE (sampler != nullptr);
    REQUIRE (sampler->getTotalNumOutputChannels() == 2); // Zusatzausgänge abgeschaltet
    REQUIRE (sampler->loadSfz (writeMultiOutSfz (env.tempFolder.getChildFile ("sfz"))).has_value());

    const auto clip = *midiClips.createEmptyClip (*track, 0.0, 2.0);
    REQUIRE (midiClips.addNote (clip, 60, 0.0, 0.5, 100).has_value()); // Ausgang 1+2
    REQUIRE (midiClips.addNote (clip, 62, 2.0, 0.5, 100).has_value()); // Ausgang 3+4: landet auf 1+2

    fk::services::ExportOptions options;
    options.file = env.tempFolder.getChildFile ("mix.wav");
    options.includeTails = false;
    const auto mix = exporter.exportMixdown (options);
    REQUIRE (mix.has_value());
    // Die Spur hat zwei Kanäle; der FK Sampler legt Regionen anderer Ausgänge dann auf den
    // Hauptausgang, damit eine Multi-Out-SFZ auf einer normalen Spur nicht stumm bleibt.
    REQUIRE (peakBetween (*mix, 0.0, 0.3) > 0.05f);
    REQUIRE (peakBetween (*mix, 1.0, 1.3) > 0.05f);

    // Instrument tauschen geht auf normalen Spuren, auf Rack-Spuren nicht.
    auto* rackMidi = *racks.createRackInstrument (TestEnvironment::synthId (plugins), "Keys");
    REQUIRE_FALSE (plugins.setInstrument (*rackMidi, TestEnvironment::synthId (plugins)).has_value());

    // Speichern und neu öffnen: Rack, Rollen und Kanäle sind wieder da.
    REQUIRE (racks.setMidiChannel (*rackMidi, 7).has_value());
    REQUIRE (env.projectService.saveProject().has_value());
    const auto file = env.tempFolder.getChildFile ("Normal/Normal.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());

    const auto reopened = racks.getRacks();
    REQUIRE (reopened.size() == 1);
    REQUIRE (reopened.front().name == "Keys");
    REQUIRE (reopened.front().numMidiTracks == 1);
    REQUIRE (reopened.front().activeOutputs[0]);

    int channel = 0;
    for (auto* reopenedTrack : env.trackService.getAudioTracks())
        if (const auto role = racks.getRole (*reopenedTrack); role && role->isMidi)
            channel = role->midiChannel;

    REQUIRE (channel == 7);
}

TEST_CASE ("MIDI-Spur an externem Gerät: Kanal, Programm und Bank", "[services][midi][b1]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::TransportService transport { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExternalMidiService externalMidi { env.projectService, env.trackService, transport };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Hardware").has_value());

    auto* track = *externalMidi.createMidiTrack ("Synth-Modul");
    REQUIRE (env.projectService.getUndoHistory().done.back() == "Add MIDI track");
    REQUIRE (env.trackService.getTrackInfo (*track).kind == fk::services::TrackKind::midi);
    REQUIRE (fk::services::playsMidi (fk::services::TrackKind::midi));

    // Ausgänge der MIDI-Spur sind nur MIDI-Ports (welche es gibt, hängt vom Testrechner ab).
    for (const auto& output : env.trackService.getAvailableOutputs (*track))
        REQUIRE (output.id.startsWith ("device:"));

    // MIDI-Clips gehen auf die Spur.
    const auto clip = midiClips.createEmptyClip (*track, 0.0, 2.0);
    REQUIRE (clip.has_value());
    REQUIRE (midiClips.addNote (*clip, 60, 0.0, 1.0, 100).has_value());

    auto settings = *externalMidi.getSettings (*track);
    REQUIRE (settings.channel == 1);
    REQUIRE (settings.program == -1);
    REQUIRE (fk::services::ExternalMidiService::programMessages (settings).empty());

    // Programm 5 (Anzeige 6) aus Bank 300: CC 0 = 2, CC 32 = 44, dann Program Change, auf Kanal 3.
    REQUIRE (externalMidi.setChannel (*track, 3).has_value());
    const auto before = externalMidi.getNumMessagesSent();
    REQUIRE (externalMidi.setProgram (*track, 5, 300).has_value());
    REQUIRE (externalMidi.getNumMessagesSent() == before + 3);

    settings = *externalMidi.getSettings (*track);
    const auto messages = fk::services::ExternalMidiService::programMessages (settings);
    REQUIRE (messages.size() == 3);
    REQUIRE (messages[0].isControllerOfType (0));
    REQUIRE (messages[0].getControllerValue() == 2);
    REQUIRE (messages[1].isControllerOfType (32));
    REQUIRE (messages[1].getControllerValue() == 44);
    REQUIRE (messages[2].isProgramChange());
    REQUIRE (messages[2].getProgramChangeNumber() == 5);
    REQUIRE (messages[2].getChannel() == 3);

    REQUIRE_FALSE (externalMidi.setProgram (*track, 128, -1).has_value());
    REQUIRE_FALSE (externalMidi.setChannel (*track, 0).has_value());

    // Ohne Bank nur der Programmwechsel.
    REQUIRE (externalMidi.setProgram (*track, 7, -1).has_value());
    REQUIRE (fk::services::ExternalMidiService::programMessages (*externalMidi.getSettings (*track)).size() == 1);

    // Normale Spuren sind keine MIDI-Spuren an Geräten.
    auto* audio = *env.trackService.createAudioTrack ("Gesang", 1);
    REQUIRE_FALSE (externalMidi.getSettings (*audio).has_value());
    REQUIRE_FALSE (externalMidi.setProgram (*audio, 1, -1).has_value());

    // Speichern und öffnen: Spurart, Kanal und Programm bleiben.
    REQUIRE (env.projectService.saveProject().has_value());
    const auto file = env.tempFolder.getChildFile ("Hardware/Hardware.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());

    auto* reopened = env.trackService.getAudioTracks().front();
    REQUIRE (env.trackService.getTrackInfo (*reopened).kind == fk::services::TrackKind::midi);
    REQUIRE (externalMidi.getSettings (*reopened)->channel == 3);
    REQUIRE (externalMidi.getSettings (*reopened)->program == 7);

    // Start der Wiedergabe schickt die Programme erneut.
    const auto beforePlay = externalMidi.getNumMessagesSent();
    externalMidi.sendAllPrograms();
    REQUIRE (externalMidi.getNumMessagesSent() == beforePlay + 1);
}

TEST_CASE ("MIDI-Insert vor dem Instrument: FK Transpose macht ein C2 zum C3", "[services][midi][b3]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "MidiInsert").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Sampler", samplerId (plugins));
    fk::plugins::SamplerProcessor* sampler = nullptr;

    for (auto* plugin : track->pluginList.getPlugins())
        if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
            sampler = dynamic_cast<fk::plugins::SamplerProcessor*> (external->getAudioPluginInstance());

    REQUIRE (sampler->loadSfz (writeMultiOutSfz (env.tempFolder.getChildFile ("sfz"))).has_value()); // klingt nur auf 60, 62, 64

    const auto clip = *midiClips.createEmptyClip (*track, 0.0, 2.0);
    REQUIRE (midiClips.addNote (clip, 48, 0.0, 0.5, 100).has_value()); // C2: keine Region

    const auto mix = [&] (const juce::String& name)
    {
        fk::services::ExportOptions options;
        options.file = env.tempFolder.getChildFile (name + ".wav");
        options.includeTails = false;
        return *exporter.exportMixdown (options);
    };

    REQUIRE (peakBetween (mix ("ohne"), 0.0, 0.3) < 0.001f);

    // FK Transpose kommt vor das Instrument, auch wenn es nach einem Audio-Insert eingesetzt wird.
    std::map<juce::String, juce::String> ids;
    for (const auto& plugin : plugins.getAllPlugins())
        ids[plugin.name] = plugin.id;

    REQUIRE (plugins.insertPlugin (*track, ids["FK EQ"]).has_value());
    const auto transposeId = *plugins.insertPlugin (*track, ids["FK Transpose"]);

    int transposeIndex = -1, synthIndex = -1, index = 0;
    for (auto* plugin : track->pluginList.getPlugins())
    {
        if (plugin->itemID.toString() == transposeId)
            transposeIndex = index;
        if (plugin->isSynth() && synthIndex < 0)
            synthIndex = index;
        ++index;
    }
    REQUIRE (transposeIndex >= 0);
    REQUIRE (transposeIndex < synthIndex);

    // +12 Halbtöne: C2 → C3, die Region klingt.
    for (auto* plugin : track->pluginList.getPlugins())
        if (plugin->itemID.toString() == transposeId)
            if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
                for (auto* parameter : external->getAudioPluginInstance()->getParameters())
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged->getParameterID() == "semitones")
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 (12.0f));

    REQUIRE (peakBetween (mix ("mit"), 0.0, 0.3) > 0.05f);

    // Kategorie im Menü.
    for (const auto& plugin : plugins.getAllPlugins())
        if (plugin.name == "FK Arpeggiator")
            REQUIRE (plugin.group == "MIDI-Effekte");
}
