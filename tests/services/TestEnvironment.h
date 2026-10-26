#pragma once

#include "fk/services/ClipService.h"
#include "fk/services/PluginService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace fk::tests
{

/// Tracktion braucht eine laufende JUCE-Umgebung (MessageManager). Dieses Hilfsobjekt
/// baut sie für die Dauer eines Tests auf; die Engine öffnet dabei kein echtes Audiogerät.
/// Member-Reihenfolge = Aufbaureihenfolge: Services nach der Engine, Abbau rückwärts.
struct TestEnvironment
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    engine::EngineContext engineContext { "FKAudioSuiteTests" };
    services::ProjectService projectService { engineContext };
    services::TimelineService timelineService { projectService };
    services::TrackService trackService { projectService };
    services::ClipService clipService { projectService };

    // Eindeutig je Lauf (UUID): ctest -j startet Tests parallel, und getNonexistentChildFile
    // gab zwei Prozessen manchmal denselben Ordner; der erste löschte ihn dem zweiten weg.
    juce::File tempFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("FKAudioSuiteTests")
                                .getChildFile ("run-" + juce::Uuid().toString());

    ~TestEnvironment()
    {
        projectService.closeProject();
        tempFolder.deleteRecursively();
    }

    /// Lässt JUCEs Ereignisschleife kurz laufen. Nötig für alles, was Tracktion asynchron erledigt,
    /// z. B. den Aufbau der MIDI-Geräteliste nach einem Rescan.
    static void pumpMessages (int milliseconds = 300)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
    }

    /// Kennung des FK Synth (Klang ohne Dateien). Nie "das erste Instrument": Seit dem Sampler
    /// steht der alphabetisch vorn und schweigt ohne SFZ-Datei.
    static juce::String synthId (const services::PluginService& pluginService)
    {
        for (const auto& instrument : pluginService.getAvailableInstruments())
            if (instrument.name == "FK Synth")
                return instrument.id;

        return {};
    }

    /// Schreibt eine Standard-MIDI-Datei mit zwei Spuren: Spur 1 Noten C4 bei 0 und E4 bei 1 (je 1 Schlag),
    /// Spur 2 eine Note G3 bei 2 Schlägen; 480 Ticks je Viertel.
    juce::File createMidiFile (const juce::String& name)
    {
        const auto file = tempFolder.getChildFile (name + ".mid");
        file.getParentDirectory().createDirectory();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);

        juce::MidiMessageSequence first;
        first.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0.0);
        first.addEvent (juce::MidiMessage::noteOff (1, 60), 480.0);
        first.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 480.0);
        first.addEvent (juce::MidiMessage::noteOff (1, 64), 960.0);
        first.updateMatchedPairs();
        midi.addTrack (first);

        juce::MidiMessageSequence second;
        second.addEvent (juce::MidiMessage::noteOn (1, 55, 0.8f), 960.0);
        second.addEvent (juce::MidiMessage::noteOff (1, 55), 1440.0);
        second.updateMatchedPairs();
        midi.addTrack (second);

        juce::FileOutputStream stream (file);
        midi.writeTo (stream);
        return file;
    }

    /// Schreibt eine stille Mono-WAV-Datei mit der gewünschten Länge.
    juce::File createSilentWav (const juce::String& name, double seconds, double sampleRate = 48000.0)
    {
        const auto file = tempFolder.getChildFile (name + ".wav");
        file.getParentDirectory().createDirectory();

        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = format.createWriterFor (
            stream,
            juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16));

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> silence (1, static_cast<int> (seconds * sampleRate));
            silence.clear();
            writer->writeFromAudioSampleBuffer (silence, 0, silence.getNumSamples());
        }

        return file;
    }
};

} // namespace fk::tests
