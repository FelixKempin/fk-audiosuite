#include "fk/services/SamplerService.h"

#include "fk/plugins/SamplerProcessor.h"
#include "fk/services/DrumMapService.h"
#include "fk/services/TrackProperties.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto samplerName = "FK Sampler";

fk::plugins::SamplerProcessor* findSampler (te::AudioTrack& track, te::ExternalPlugin** owner = nullptr)
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin))
            if (auto* sampler = dynamic_cast<fk::plugins::SamplerProcessor*> (external->getAudioPluginInstance()))
            {
                if (owner != nullptr)
                    *owner = external;

                return sampler;
            }

    return nullptr;
}
} // namespace

SamplerService::SamplerService (ProjectService& project, TrackService& tracks, PluginService& plugins)
    : projectService (project)
    , trackService (tracks)
    , pluginService (plugins)
{
}

bool SamplerService::isSamplerTrack (te::AudioTrack& track) const
{
    return track.state.hasProperty (trackProperties::samplerRegions);
}

std::vector<SamplerRegion> SamplerService::getRegions (te::AudioTrack& track) const
{
    std::vector<SamplerRegion> regions;
    const auto stored = juce::JSON::parse (track.state.getProperty (trackProperties::samplerRegions).toString());

    if (const auto* list = stored.getArray())
        for (const auto& entry : *list)
            regions.push_back ({ juce::File (entry.getProperty ("file", {}).toString()),
                                 entry.getProperty ("note", 60),
                                 entry.getProperty ("chromatic", false) });

    return regions;
}

void SamplerService::storeRegions (te::AudioTrack& track, const std::vector<SamplerRegion>& regions)
{
    juce::Array<juce::var> list;

    for (const auto& region : regions)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("file", region.sample.getFullPathName());
        object->setProperty ("note", region.note);
        object->setProperty ("chromatic", region.chromatic);
        list.add (juce::var (object.release()));
    }

    track.state.setProperty (trackProperties::samplerRegions,
                             juce::JSON::toString (juce::var (list), true),
                             &projectService.getEdit()->getUndoManager());
}

juce::String SamplerService::createSfz (const std::vector<SamplerRegion>& regions, const juce::File& sfzFolder)
{
    juce::String sfz ("// Von FK Audio Suite erzeugt (Sampler-Spur). Änderungen hier gehen beim nächsten Ablegen verloren.\n");

    for (const auto& region : regions)
    {
        const auto path = region.sample.getRelativePathFrom (sfzFolder).replaceCharacter ('\\', '/');

        if (region.chromatic)
            sfz << "<region> sample=" << path << " lokey=0 hikey=127 pitch_keycenter=" << region.note
                << " ampeg_release=0.3\n";
        else
            sfz << "<region> sample=" << path << " key=" << region.note
                << " loop_mode=one_shot ampeg_release=0.05\n";
    }

    return sfz;
}

core::Result<juce::File> SamplerService::copyIntoProject (const juce::File& sample) const
{
    if (! sample.existsAsFile())
        return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("Sample not found: {path}"), { { "path", sample.getFullPathName() } }).toStdString());

    const auto audioFolder = projectService.getProjectFolder().getChildFile ("Audio");

    if (sample.isAChildOf (audioFolder))
        return sample;

    audioFolder.createDirectory();
    const auto target = audioFolder.getNonexistentChildFile (sample.getFileNameWithoutExtension(), sample.getFileExtension(), false);

    if (! sample.copyFileTo (target))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not copy the sample: {path}"), { { "path", target.getFullPathName() } }).toStdString());

    return target;
}

core::Result<void> SamplerService::writeAndLoad (te::AudioTrack& track, const std::vector<SamplerRegion>& regions)
{
    te::ExternalPlugin* owner = nullptr;
    auto* sampler = findSampler (track, &owner);

    if (sampler == nullptr)
        return core::fail (core::ErrorCode::unsupported, TRANS ("The track has no FK Sampler.").toStdString());

    storeRegions (track, regions);

    if (regions.empty())
    {
        sampler->unload();
        owner->flushPluginStateToValueTree();
        return {};
    }

    const auto folder = projectService.getProjectFolder().getChildFile ("Sampler");
    folder.createDirectory();
    const auto sfz = folder.getChildFile (juce::File::createLegalFileName (track.getName()) + "-" + track.itemID.toString() + ".sfz");

    if (! sfz.replaceWithText (createSfz (regions, folder)))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not write the SFZ file: {path}"), { { "path", sfz.getFullPathName() } }).toStdString());

    if (auto loaded = sampler->loadSfz (sfz); ! loaded)
        return loaded;

    owner->flushPluginStateToValueTree(); // die SFZ-Datei steht im Plugin-Zustand → Projekt
    return {};
}

core::Result<te::AudioTrack*> SamplerService::createSamplerTrack (const juce::File& sample)
{
    if (projectService.getEdit() == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    juce::String samplerId;

    for (const auto& plugin : pluginService.getAllPlugins())
        if (plugin.name == samplerName && plugin.format == "FK")
            samplerId = plugin.id;

    if (samplerId.isEmpty())
        return core::fail (core::ErrorCode::notFound, TRANS ("FK Sampler is not registered.").toStdString());

    std::optional<juce::File> copied;

    if (sample != juce::File())
    {
        auto copy = copyIntoProject (sample);

        if (! copy)
            return std::unexpected (copy.error());

        copied = *copy;
    }

    const auto name = copied ? copied->getFileNameWithoutExtension() : TRANS ("Drum kit");
    auto created = trackService.createInstrumentTrack (name, samplerId);

    if (! created)
        return created;

    auto* track = *created; // alles Weitere gehört zum Undo-Schritt "Spur anlegen"

    if (! copied)
    {
        // Leeres Kit: GM-Map, damit der Drum-Editor gleich die Zeilen zum Ablegen zeigt.
        track->state.setProperty (trackProperties::drumMap, DrumMapService::gmMapId, &projectService.getEdit()->getUndoManager());
        storeRegions (*track, {});
        return track;
    }

    if (auto written = writeAndLoad (*track, { { *copied, 60, true } }); ! written)
        return std::unexpected (written.error());

    return track;
}

core::Result<te::AudioTrack*> SamplerService::createTrackFromSfz (const juce::File& sfzFile, const juce::String& name)
{
    if (! sfzFile.existsAsFile())
        return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("SFZ file not found: {path}"), { { "path", sfzFile.getFullPathName() } }).toStdString());

    juce::String samplerId;

    for (const auto& plugin : pluginService.getAllPlugins())
        if (plugin.name == samplerName && plugin.format == "FK")
            samplerId = plugin.id;

    if (samplerId.isEmpty())
        return core::fail (core::ErrorCode::notFound, TRANS ("FK Sampler is not registered.").toStdString());

    auto created = trackService.createInstrumentTrack (name.isNotEmpty() ? name : sfzFile.getFileNameWithoutExtension(), samplerId);

    if (! created)
        return created;

    te::ExternalPlugin* owner = nullptr;
    auto* sampler = findSampler (**created, &owner);

    if (sampler == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not load FK Sampler.").toStdString());

    if (auto loaded = sampler->loadSfz (sfzFile); ! loaded)
        return std::unexpected (loaded.error());

    owner->flushPluginStateToValueTree();
    return *created;
}

core::Result<void> SamplerService::assignSample (te::AudioTrack& track, const juce::File& sample, int note)
{
    if (note < 0 || note > 127)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The note must be between 0 and 127.").toStdString());

    if (findSampler (track) == nullptr)
        return core::fail (core::ErrorCode::unsupported,
                           TRANS ("Samples can be dropped on tracks with FK Sampler. Track > Sampler track creates one.").toStdString());

    auto copy = copyIntoProject (sample);

    if (! copy)
        return std::unexpected (copy.error());

    projectService.beginUndoStep (TRANS ("Assign sample"));

    // Ein chromatisches Sample und ein Pad auf derselben Spur schließen sich aus: Ab dem ersten Pad ist es ein Kit.
    auto regions = getRegions (track);
    std::erase_if (regions, [note] (const SamplerRegion& region) { return region.chromatic || region.note == note; });
    regions.push_back ({ *copy, note, false });
    std::ranges::sort (regions, {}, &SamplerRegion::note);
    return writeAndLoad (track, regions);
}

core::Result<void> SamplerService::removeSample (te::AudioTrack& track, int note)
{
    auto regions = getRegions (track);

    if (std::erase_if (regions, [note] (const SamplerRegion& region) { return region.note == note; }) == 0)
        return core::fail (core::ErrorCode::notFound, TRANS ("There is no sample on this note.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove sample"));
    return writeAndLoad (track, regions);
}

} // namespace fk::services
