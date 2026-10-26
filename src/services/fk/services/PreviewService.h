#pragma once

#include "fk/core/Result.h"
#include "fk/engine/EngineContext.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

namespace fk::services
{

/// Vorhören von Audiodateien im Medien-Browser, unabhängig vom Projekt: ein eigener
/// Audio-Callback neben Tracktions Wiedergabe (JUCE mischt beide auf das Gerät). Dateien werden
/// über einen Hintergrund-Thread gepuffert, der Audio-Thread liest nur aus dem Puffer.
class PreviewService final
{
public:
    explicit PreviewService (engine::EngineContext& engineContext);
    ~PreviewService();

    /// Startet die Datei von vorn; eine laufende Vorschau wird ersetzt.
    [[nodiscard]] core::Result<void> play (const juce::File& audioFile);
    void stop();

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] juce::File getCurrentFile() const;
    [[nodiscard]] double getPositionSeconds() const;
    [[nodiscard]] double getLengthSeconds() const;

    /// Dauer einer Datei in Sekunden ohne sie abzuspielen (0, wenn unlesbar).
    [[nodiscard]] double getFileLengthSeconds (const juce::File& audioFile) const;

private:
    engine::EngineContext& engineContext;
    mutable juce::AudioFormatManager formats; // createReaderFor ist bei JUCE nicht const, ändert aber nichts
    juce::TimeSliceThread readThread { "FK Vorschau" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer player;
    juce::File currentFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewService)
};

} // namespace fk::services
