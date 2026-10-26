#include "fk/services/PreviewService.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

PreviewService::PreviewService (engine::EngineContext& context)
    : engineContext (context)
{
    formats.registerBasicFormats(); // WAV, AIFF, FLAC, OGG und MP3 (JUCE_USE_MP3AUDIOFORMAT)
    readThread.startThread();
    player.setSource (&transport);
    engineContext.getAudioDeviceManager().addAudioCallback (&player);
}

PreviewService::~PreviewService()
{
    // Reihenfolge: erst vom Gerät abmelden, dann Quelle lösen, dann Thread beenden.
    engineContext.getAudioDeviceManager().removeAudioCallback (&player);
    player.setSource (nullptr);
    transport.setSource (nullptr);
    readerSource.reset();
    readThread.stopThread (2000);
}

core::Result<void> PreviewService::play (const juce::File& audioFile)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (audioFile));

    if (reader == nullptr)
        return core::fail (core::ErrorCode::unsupported,
                           plugins::i18n::format (TRANS ("Could not read the file: {path}"), { { "path", audioFile.getFullPathName() } }).toStdString());

    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();

    const double sampleRate = reader->sampleRate;
    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    transport.setSource (readerSource.get(), 32768, &readThread, sampleRate);
    transport.setPosition (0.0);
    transport.start();
    currentFile = audioFile;
    return {};
}

void PreviewService::stop()
{
    transport.stop();
    transport.setPosition (0.0);
}

bool PreviewService::isPlaying() const
{
    return transport.isPlaying();
}

juce::File PreviewService::getCurrentFile() const
{
    return currentFile;
}

double PreviewService::getPositionSeconds() const
{
    return transport.getCurrentPosition();
}

double PreviewService::getLengthSeconds() const
{
    return transport.getLengthInSeconds();
}

double PreviewService::getFileLengthSeconds (const juce::File& audioFile) const
{
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (audioFile));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
}

} // namespace fk::services
