#include "fk/services/WaveformService.h"

namespace fk::services
{

WaveformService::WaveformService (ProjectService& service)
    : projectService (service)
{
}

std::unique_ptr<juce::AudioThumbnailBase> WaveformService::createThumbnail (const juce::File& audioFile,
                                                                            juce::Component& componentToRepaint)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return nullptr;

    return std::make_unique<te::SmartThumbnail> (
        edit->engine, te::AudioFile (edit->engine, audioFile), componentToRepaint, edit);
}

} // namespace fk::services
