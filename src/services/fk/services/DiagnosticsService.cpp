#include "fk/services/DiagnosticsService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <vector>

namespace fk::services
{

namespace
{
/// Die neuesten Dateien eines Ordners, neueste zuerst.
std::vector<juce::File> newestFiles (const juce::File& folder, int maxCount)
{
    std::vector<juce::File> files;

    for (const auto& file : folder.findChildFiles (juce::File::findFiles, false))
        files.push_back (file);

    std::ranges::sort (files,
                       [] (const juce::File& a, const juce::File& b)
                       { return a.getLastModificationTime() > b.getLastModificationTime(); });

    if (files.size() > static_cast<std::size_t> (maxCount))
        files.resize (static_cast<std::size_t> (maxCount));

    return files;
}
} // namespace

DiagnosticsService::DiagnosticsService (engine::EngineContext& context,
                                        ProjectService& service,
                                        DiagnosticsFolders diagnosticsFolders)
    : engineContext (context)
    , projectService (service)
    , folders (std::move (diagnosticsFolders))
{
}

juce::String DiagnosticsService::describeSystem (const juce::String& appVersion) const
{
    juce::StringArray lines;
    lines.add ("FK Audio Suite " + appVersion);
    lines.add ("Erstellt: " + juce::Time::getCurrentTime().toISO8601 (true));
    lines.add (juce::SystemStats::getJUCEVersion());
    lines.add ({});
    lines.add ("Betriebssystem: " + juce::SystemStats::getOperatingSystemName());
    lines.add ("CPU: " + juce::SystemStats::getCpuModel() + ", " + juce::String (juce::SystemStats::getNumCpus()) +
               " Kerne");
    lines.add ("RAM: " + juce::String (juce::SystemStats::getMemorySizeInMegabytes()) + " MB");
    lines.add ({});

    auto& deviceManager = engineContext.getAudioDeviceManager();

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        lines.add (juce::String::fromUTF8 ("Audiogerät: ") + device->getName() + " (" +
                   deviceManager.getCurrentAudioDeviceType() + ")");
        lines.add ("Samplerate: " + juce::String (device->getCurrentSampleRate()) + " Hz, Puffer " +
                   juce::String (device->getCurrentBufferSizeSamples()) + " Samples");
        lines.add (juce::String::fromUTF8 ("Latenz: Eingang ") + juce::String (device->getInputLatencyInSamples()) +
                   juce::String::fromUTF8 (", Ausgang ") + juce::String (device->getOutputLatencyInSamples()) +
                   " Samples");
        lines.add (juce::String::fromUTF8 ("Aktive Kanäle: ") +
                   juce::String (device->getActiveInputChannels().countNumberOfSetBits()) + " ein, " +
                   juce::String (device->getActiveOutputChannels().countNumberOfSetBits()) + " aus");
        lines.add ("DSP-Last: " + juce::String (juce::roundToInt (deviceManager.getCpuUsage() * 100.0)) + " %");
    }
    else
    {
        lines.add (juce::String::fromUTF8 ("Audiogerät: keins aktiv"));
    }

    lines.add ("Bekannte Plugins: " +
               juce::String (engineContext.getEngine().getPluginManager().knownPluginList.getNumTypes()));
    lines.add ({});

    if (const auto* edit = projectService.getEdit())
    {
        lines.add ("Projekt: " + projectService.getEditFile().getFullPathName());
        lines.add ("Spuren: " + juce::String (te::getAllTracks (*edit).size()));
        lines.add (juce::String::fromUTF8 ("Ungespeicherte Änderungen: ") +
                   (projectService.isModified() ? "ja" : "nein"));
    }
    else
    {
        lines.add (juce::String::fromUTF8 ("Projekt: keins geöffnet"));
    }

    return lines.joinIntoString ("\n") + "\n";
}

core::Result<void> DiagnosticsService::exportReport (const juce::File& zipFile, const juce::String& appVersion) const
{
    juce::ZipFile::Builder builder;
    const auto systemText = describeSystem (appVersion).toStdString();
    builder.addEntry (std::make_unique<juce::MemoryInputStream> (systemText.data(), systemText.size(), true),
                      9,
                      "System.txt",
                      juce::Time::getCurrentTime());

    const std::pair<const juce::File*, const char*> sources[] = { { &folders.logs, "Logs" },
                                                                  { &folders.hangReports, "Haenger" },
                                                                  { &folders.crashReports, "Absturz" } };

    for (const auto& [folder, zipFolder] : sources)
        for (const auto& file : newestFiles (*folder, maxFilesPerFolder))
            builder.addFile (file, 9, juce::String (zipFolder) + "/" + file.getFileName());

    juce::TemporaryFile temp (zipFile);

    {
        juce::FileOutputStream stream (temp.getFile());

        if (stream.failedToOpen() || ! builder.writeToStream (stream, nullptr))
            return core::fail (core::ErrorCode::ioError,
                               plugins::i18n::format (TRANS ("Could not write the diagnostics report: {path}"), { { "path", zipFile.getFullPathName() } }).toStdString());
    }

    if (! temp.overwriteTargetFileWithTemporary())
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not save the diagnostics report: {path}"), { { "path", zipFile.getFullPathName() } }).toStdString());

    return {};
}

const DiagnosticsFolders& DiagnosticsService::getFolders() const noexcept
{
    return folders;
}

} // namespace fk::services
