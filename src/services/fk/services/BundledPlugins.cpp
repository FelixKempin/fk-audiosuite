#include "fk/services/BundledPlugins.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

BundledPlugins::BundledPlugins (Folders bundleFolders, juce::String tag)
    : folders (std::move (bundleFolders))
    , versionTag (std::move (tag))
{
}

BundledPlugins::Folders BundledPlugins::defaultFolders()
{
    // JUCE meldet das Programm so, wie es gestartet wurde. Im AppImage ist das AppRun, ein Symlink im
    // Wurzelordner auf usr/bin/fk-audio-suite; erst der aufgelöste Pfad liegt neben usr/lib.
    const auto programFolder =
        juce::File::getSpecialLocation (juce::File::currentExecutableFile).getLinkedTarget().getParentDirectory();

#if JUCE_WINDOWS
    // Zip: Bundles in <Programm>\VST3, an Ort und Stelle.
    if (const auto portable = programFolder.getChildFile ("VST3"); portable.isDirectory())
        return { portable, {} };

    // Installer: Bundles im geteilten Ordner Common Files\VST3, die eigenen stehen in bundled-plugins.txt.
    juce::StringArray names;
    names.addLines (programFolder.getChildFile ("bundled-plugins.txt").loadFileAsString());
    names.trim();
    names.removeEmptyStrings();
    if (names.isEmpty())
        return {};

    const auto commonFiles =
        juce::SystemStats::getEnvironmentVariable ("CommonProgramW6432", "C:\\Program Files\\Common Files");
    return { juce::File (commonFiles).getChildFile ("VST3"), {}, names };
#else
    // AppImage: usr/bin/fk-audio-suite und usr/lib/vst3/<Name>.vst3. Die Laufzeit des AppImage nennt
    // den Einhängepunkt in APPDIR; das gilt vor dem Programmpfad.
    const auto appDir = juce::SystemStats::getEnvironmentVariable ("APPDIR", {});
    const auto usr = appDir.isNotEmpty() ? juce::File (appDir).getChildFile ("usr") : programFolder.getParentDirectory();

    return { usr.getChildFile ("lib").getChildFile ("vst3"),
             juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3") };
#endif
}

core::Result<std::vector<juce::File>> BundledPlugins::install() const
{
    std::vector<juce::File> plugins;

    if (! folders.source.isDirectory())
        return plugins;

    for (const auto& bundle : folders.source.findChildFiles (juce::File::findDirectories, false, "*.vst3"))
    {
        if (! folders.onlyNames.isEmpty() && ! folders.onlyNames.contains (bundle.getFileName()))
            continue;

        if (folders.target == juce::File())
        {
            plugins.push_back (bundle);
            continue;
        }

        const auto installed = folders.target.getChildFile (bundle.getFileName());
        const auto marker = installed.getChildFile (markerFileName);

        // Ohne Kennzeichen hat der Nutzer das Plugin selbst installiert: nie überschreiben.
        const bool isOurs = ! installed.exists() || marker.existsAsFile();

        if (isOurs && marker.loadFileAsString().trim() != versionTag)
        {
            if (installed.exists() && ! installed.deleteRecursively())
                return core::fail (core::ErrorCode::ioError,
                                   plugins::i18n::format (TRANS ("Could not replace the old plugin: {path}"), { { "path", installed.getFullPathName() } }).toStdString());

            if (! folders.target.createDirectory() || ! bundle.copyDirectoryTo (installed)
                || ! marker.replaceWithText (versionTag))
                return core::fail (core::ErrorCode::ioError,
                                   plugins::i18n::format (TRANS ("Could not install the plugin to {path}"), { { "path", installed.getFullPathName() } }).toStdString());
        }

        plugins.push_back (installed);
    }

    return plugins;
}

} // namespace fk::services
