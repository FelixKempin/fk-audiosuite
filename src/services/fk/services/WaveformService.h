#pragma once

#include "fk/services/ProjectService.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

namespace fk::services
{

/// Liefert Wellenform-Vorschauen für Audiodateien. Dahinter steckt Tracktions SmartThumbnail
/// (Berechnung im Hintergrund, Cache auf Platte); die Oberfläche sieht nur die JUCE-Basisklasse
/// juce::AudioThumbnailBase und bleibt damit frei von Tracktion-Aufrufen (ADR 0004).
class WaveformService final
{
public:
    explicit WaveformService (ProjectService& projectService);

    /// Erzeugt eine Vorschau, die componentToRepaint neu zeichnet, sobald mehr Daten da sind.
    /// Der Rückgabewert gehört dem Aufrufer; er muss vor dem Projektwechsel freigegeben werden.
    [[nodiscard]] std::unique_ptr<juce::AudioThumbnailBase> createThumbnail (const juce::File& audioFile,
                                                                             juce::Component& componentToRepaint);

private:
    ProjectService& projectService;
};

} // namespace fk::services
