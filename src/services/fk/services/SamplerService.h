#pragma once

#include "fk/core/Result.h"
#include "fk/services/PluginService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <optional>
#include <vector>

namespace fk::services
{

/// Ein Sample auf einer Sampler-Spur: entweder über die ganze Tastatur gespielt (chromatisch, Grundton
/// C3) oder genau auf einer Note (Drum-Pad).
struct SamplerRegion
{
    juce::File sample;
    int note = 60;
    bool chromatic = false;
};

/// Sampler-Spuren (Cubase: Sampler-Spur): eine Instrumentenspur mit FK Sampler, deren SFZ aus eigenen
/// Samples entsteht. Die Samples werden in den Projektordner (Audio/) kopiert; die Liste der Samples
/// steht in der Spur, die SFZ-Datei (Sampler/) wird daraus bei jeder Änderung neu geschrieben.
class SamplerService final
{
public:
    SamplerService (ProjectService& projectService, TrackService& trackService, PluginService& pluginService);

    /// Neue Sampler-Spur. Mit Sample: chromatisch spielbar; ohne: leeres Drum-Kit mit GM-Drum-Map,
    /// Samples kommen per Drag-and-drop im Drum-Editor auf die Noten.
    [[nodiscard]] core::Result<te::AudioTrack*> createSamplerTrack (const juce::File& sample = {});

    /// Instrumentenspur mit FK Sampler, der diese SFZ-Datei spielt (Instrumente aus dem Content-Manager).
    [[nodiscard]] core::Result<te::AudioTrack*> createTrackFromSfz (const juce::File& sfzFile, const juce::String& name);

    /// Sample auf genau eine Note legen (ersetzt, was dort lag). Die Spur muss eine Sampler-Spur sein.
    [[nodiscard]] core::Result<void> assignSample (te::AudioTrack& track, const juce::File& sample, int note);

    [[nodiscard]] core::Result<void> removeSample (te::AudioTrack& track, int note);

    [[nodiscard]] bool isSamplerTrack (te::AudioTrack& track) const;
    [[nodiscard]] std::vector<SamplerRegion> getRegions (te::AudioTrack& track) const;

    /// Die SFZ-Datei zu Regionen (ein Text; Pfade relativ zum Ordner der SFZ-Datei).
    [[nodiscard]] static juce::String createSfz (const std::vector<SamplerRegion>& regions, const juce::File& sfzFolder);

private:
    [[nodiscard]] core::Result<void> writeAndLoad (te::AudioTrack& track, const std::vector<SamplerRegion>& regions);
    [[nodiscard]] core::Result<juce::File> copyIntoProject (const juce::File& sample) const;
    void storeRegions (te::AudioTrack& track, const std::vector<SamplerRegion>& regions);

    ProjectService& projectService;
    TrackService& trackService;
    PluginService& pluginService;
};

} // namespace fk::services
