#include "fk/engine/FkEngineBehaviour.h"

#include "fk/engine/ExpressionMap.h"

namespace fk::engine
{

juce::File FkEngineBehaviour::getFileForNewAudioRecording (te::Track& track, const juce::String& fileExtension)
{
    // ProjectService setzt das Temp-Verzeichnis des Edits auf <Projekt>/Audio.
    const auto audioFolder = track.edit.getTempDirectory (true);

    // Spurname als Dateiname, aber nur Zeichen, die auf jedem Dateisystem erlaubt sind.
    const auto baseName = juce::File::createLegalFileName (track.getName().trim());
    const auto safeName = baseName.isEmpty() ? juce::String ("Aufnahme") : baseName;

    // getNonexistentChildFile hängt bei Bedarf _2, _3 ... an, damit nichts überschrieben wird.
    return audioFolder.getNonexistentChildFile (safeName, fileExtension, true);
}

bool FkEngineBehaviour::canScanPluginsOutOfProcess()
{
    return true;
}

bool FkEngineBehaviour::isDescriptionOfWaveDevicesSupported()
{
    return true;
}

void FkEngineBehaviour::describeWaveDevices (std::vector<te::WaveDeviceDescription>& descriptions,
                                             juce::AudioIODevice& device,
                                             bool isInput)
{
    // Die Namen vergibt Tracktion danach selbst aus den Kanalnamen; angezeigt wird "Eingang 1" usw.
    // (TrackService), daher bleiben sie hier leer.
    if (isInput)
    {
        // PipeWire spiegelt über JACK jeden Ausgang als "monitor_..."-Eingang zurück. Daran hängt kein
        // Instrument, die Liste würde nur doppelt so lang (und jeder Eingang kostet Speicher).
        const auto channelNames = device.getInputChannelNames();
        std::vector<uint32_t> inputs;

        for (int channel = 0; channel < channelNames.size(); ++channel)
            if (! channelNames[channel].startsWith ("monitor_"))
                inputs.push_back (static_cast<uint32_t> (channel));

        // Mono: jeder Kanal einzeln. Stereo: je zwei benachbarte Kanäle, 1+2, 3+4, ...
        for (const auto channel : inputs)
            descriptions.push_back (te::WaveDeviceDescription::withNumChannels ({}, channel, 1, true));

        for (size_t i = 0; i + 1 < inputs.size();)
        {
            if (inputs[i + 1] != inputs[i] + 1)
            {
                ++i;
                continue;
            }

            descriptions.push_back (te::WaveDeviceDescription::withNumChannels ({}, inputs[i], 2, true));
            i += 2;
        }

        return;
    }

    const auto numChannels = static_cast<uint32_t> (device.getOutputChannelNames().size());

    for (uint32_t channel = 0; channel + 1 < numChannels; channel += 2)
        descriptions.push_back (te::WaveDeviceDescription::withNumChannels ({}, channel, 2, true));
}

juce::MidiMessageSequence FkEngineBehaviour::createPlaybackMidiSequence (const te::MidiList& list,
                                                                        te::MidiClip& clip,
                                                                        te::MidiList::TimeBase timeBase,
                                                                        bool generateMPE)
{
    return createSequenceWithArticulations (list, clip, timeBase, generateMPE);
}

} // namespace fk::engine
