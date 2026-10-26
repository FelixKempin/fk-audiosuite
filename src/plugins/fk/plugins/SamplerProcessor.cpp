#include "fk/plugins/SamplerProcessor.h"

#include "fk/plugins/i18n/Text.h"

#include <sfizz.hpp>

#include <algorithm>
#include <array>

namespace fk::plugins
{

namespace
{
constexpr auto volumeId = "volume";
const juce::Identifier sfzFileProperty ("sfzFile");
constexpr int numVoices = 64;
constexpr int numOutputPairs = 8; // SFZ: output=0..7 wählt den Stereo-Ausgang einer Region
constexpr int numOutputChannels = numOutputPairs * 2;
} // namespace

SamplerProcessor::SamplerProcessor()
    : InternalPlugin (info(), createLayout())
    , synth (std::make_unique<sfz::Sfizz>())
    , volumeDb (parameter (volumeId))
{
    synth->setNumVoices (numVoices);
}

SamplerProcessor::~SamplerProcessor() = default;

InternalPlugin::Info SamplerProcessor::info()
{
    return { .name = "FK Sampler",
             .category = "Instrument",
             .uniqueId = 0x464b0102,
             .isInstrument = true,
             .outputPairs = numOutputPairs };
}

juce::AudioProcessorValueTreeState::ParameterLayout SamplerProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { volumeId, 1 },
                                                             TRANS ("Volume"),
                                                             juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f),
                                                             0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

void SamplerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock lock (renderLock);
    synth->setSampleRate (static_cast<float> (sampleRate));
    synth->setSamplesPerBlock (samplesPerBlock);

    outputGain.prepare ({ sampleRate, static_cast<juce::uint32> (samplesPerBlock), numOutputChannels });
    outputGain.setRampDurationSeconds (0.02);
}

void SamplerProcessor::releaseResources() {}

void SamplerProcessor::sendMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int delay = metadata.samplePosition;

        if (message.isNoteOn())
            synth->noteOn (delay, message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff())
            synth->noteOff (delay, message.getNoteNumber(), message.getVelocity());
        else if (message.isController())
            synth->cc (delay, message.getControllerNumber(), message.getControllerValue());
        else if (message.isPitchWheel())
            synth->pitchWheel (delay, message.getPitchWheelValue() - 8192); // JUCE 0..16383, sfizz -8192..8191
        else if (message.isChannelPressure())
            synth->channelAftertouch (delay, message.getChannelPressureValue());
        else if (message.isAftertouch())
            synth->polyAftertouch (delay, message.getNoteNumber(), message.getAfterTouchValue());
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            synth->allSoundOff();
    }
}

void SamplerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Während loadSfz() den Lock hält, bleibt der Block still: Warten wäre ein Aussetzer, der
    // nie wieder gut wird; ein stiller Block beim Laden fällt nicht auf.
    const juce::ScopedTryLock tryLock (renderLock);

    // So viele Stereo-Paare, wie der Host gerade freigeschaltet hat: auf einer normalen Spur eins,
    // im Rack alle acht. Regionen mit output= jenseits davon legt sfizz auf den Hauptausgang.
    const int pairs = std::min (buffer.getNumChannels() / 2, numOutputPairs);

    if (! tryLock.isLocked() || pairs < 1)
        return;

    sendMidi (midi);

    std::array<float*, numOutputChannels> outputs {};

    for (int channel = 0; channel < pairs * 2; ++channel)
        outputs[static_cast<std::size_t> (channel)] = buffer.getWritePointer (channel);

    synth->renderBlock (outputs.data(), static_cast<std::size_t> (buffer.getNumSamples()), pairs);

    outputGain.setGainDecibels (volumeDb.load());
    juce::dsp::AudioBlock<float> block (buffer);
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

core::Result<void> SamplerProcessor::loadSfz (const juce::File& file)
{
    if (! file.existsAsFile())
        return core::fail (core::ErrorCode::notFound,
                           i18n::format (TRANS ("SFZ file not found: {file}"), { { "file", file.getFullPathName() } })
                               .toStdString());

    bool loaded = false;

    {
        const juce::ScopedLock lock (renderLock);
        loaded = synth->loadSfzFile (file.getFullPathName().toStdString());
        numRegions.store (loaded ? synth->getNumRegions() : 0);
    }

    if (! loaded)
        return core::fail (core::ErrorCode::invalidArgument,
                           i18n::format (TRANS ("SFZ file contains no playable regions: {file}"),
                                         { { "file", file.getFileName() } })
                               .toStdString());

    parameters.state.setProperty (sfzFileProperty, file.getFullPathName(), nullptr);
    return {};
}

void SamplerProcessor::unload()
{
    {
        const juce::ScopedLock lock (renderLock);
        synth->loadSfzString ("", "");
        numRegions.store (0);
    }

    parameters.state.removeProperty (sfzFileProperty, nullptr);
}

void SamplerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    InternalPlugin::setStateInformation (data, sizeInBytes);

    // Die Datei steht als Eigenschaft im Zustand; nach dem Laden des Projekts wieder öffnen.
    const auto path = parameters.state.getProperty (sfzFileProperty).toString();

    if (path.isNotEmpty())
        if (const auto result = loadSfz (juce::File (path)); ! result)
            juce::Logger::writeToLog ("FK Sampler: " + juce::String::fromUTF8 (result.error().message.c_str()));
}

juce::File SamplerProcessor::getSfzFile() const
{
    const auto path = parameters.state.getProperty (sfzFileProperty).toString();
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

int SamplerProcessor::getNumRegions() const noexcept
{
    return numRegions.load();
}

int SamplerProcessor::getNumActiveVoices() const noexcept
{
    return synth->getNumActiveVoices();
}

juce::String SamplerProcessor::getStatusText() const
{
    const auto file = getSfzFile();

    if (file == juce::File())
        return TRANS ("No SFZ file loaded. Load... chooses a file.");

    const auto regions = getNumRegions();
    const auto voices = getNumActiveVoices();

    return i18n::format (TRANS ("{file}  |  {regions}  |  {voices}"),
                         { { "file", file.getFileName() },
                           { "regions", i18n::plural (regions, "{count} region", "{count} regions") },
                           { "voices", i18n::plural (voices, "{count} voice", "{count} voices") } });
}

void SamplerProcessor::chooseFile()
{
    // Asynchron: Der Dialog blockiert weder Oberfläche noch Audio; das Ergebnis kommt per Callback.
    fileChooser = std::make_unique<juce::FileChooser> (
        TRANS ("Choose SFZ instrument"), getSfzFile().getParentDirectory(), "*.sfz");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file == juce::File())
                                      return;

                                  if (const auto result = loadSfz (file); ! result)
                                      juce::Logger::writeToLog (
                                          "FK Sampler: " + juce::String::fromUTF8 (result.error().message.c_str()));
                              });
}

ui::PanelDescription SamplerProcessor::describePanel()
{
    return { .title = info().name,
             .sections = { { .title = TRANS ("Instrument"),
                             .controls = { ui::action (TRANS ("Load..."), [this] { chooseFile(); }),
                                           ui::status ([this] { return getStatusText(); }) } },
                           { .title = TRANS ("Output"), .controls = { ui::knob (volumeId) } } } };
}

} // namespace fk::plugins
