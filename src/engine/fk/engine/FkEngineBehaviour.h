#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace fk::engine
{

namespace te = tracktion;

/// Anpassungen am Verhalten der Tracktion Engine. Tracktion fragt hier nach, statt
/// Entscheidungen fest zu verdrahten (Strategie-Muster, offen für Erweiterung: SOLID "O").
///
/// M1: Wohin Aufnahmen geschrieben werden (Projektordner/Audio, benannt nach der Spur).
/// M4: Plugin-Scan im Unterprozess.
/// M6: Mono- und Stereo-Eingänge nebeneinander.
class FkEngineBehaviour final : public te::EngineBehaviour
{
public:
    FkEngineBehaviour() = default;

    /// Liefert die Zieldatei für eine neue Aufnahme auf dieser Spur.
    /// Beispiel: <Projekt>/Audio/Gitarre_1.wav, _2.wav, ...
    juce::File getFileForNewAudioRecording (te::Track& track, const juce::String& fileExtension) override;

    /// Plugins werden in einem Unterprozess untersucht: Stürzt ein fremdes Plugin beim Laden ab,
    /// stirbt nur der Helfer, nicht die DAW. Die Gegenseite dazu steht in Application::initialise.
    bool canScanPluginsOutOfProcess() override;

    /// Wir beschreiben die Audio-Ein-/Ausgänge selbst: Jeder Eingangskanal einzeln als Mono-Eingang
    /// UND jedes Kanalpaar als Stereo-Eingang, damit Mono- und Stereospuren beides wählen können
    /// (wie Cubase' Eingangsbusse). Ausgänge bleiben Stereo-Paare. Die "monitor_"-Rückkanäle, die
    /// PipeWire unter JACK zusätzlich als Eingänge meldet, lassen wir weg.
    bool isDescriptionOfWaveDevicesSupported() override;
    void describeWaveDevices (std::vector<te::WaveDeviceDescription>& descriptions,
                              juce::AudioIODevice& device,
                              bool isInput) override;

    /// B4: Spielweisen der Noten beim Abspielen in Kanal, Keyswitch und CC umsetzen (ExpressionMap.h).
    juce::MidiMessageSequence createPlaybackMidiSequence (const te::MidiList& list,
                                                          te::MidiClip& clip,
                                                          te::MidiList::TimeBase timeBase,
                                                          bool generateMPE) override;
};

} // namespace fk::engine
