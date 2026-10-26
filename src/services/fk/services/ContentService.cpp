#include "fk/services/ContentService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto manifestName = "fk-package.json"; ///< liegt im Paketordner, wenn die Installation fertig ist
constexpr juce::int64 megabyte = 1024 * 1024;
} // namespace

ContentService::ContentService (juce::File folder, std::vector<ContentPackage> packages)
    : contentFolder (std::move (folder))
    , catalog (std::move (packages))
{
}

std::vector<ContentPackage> ContentService::defaultCatalog()
{
    return {
        { "salamander-grand-piano",
          "Salamander Grand Piano",
          NEEDS_TRANS ("Yamaha C5, 16 velocity layers, pedal noises. SFZ for the FK Sampler."),
          "https://github.com/sfzinstruments/SalamanderGrandPiano/archive/refs/heads/master.zip",
          "CC BY 3.0, Alexander Holm (SFZ: sfzinstruments)",
          750 * megabyte },
        { "vsco2-ce",
          "VSCO 2 Community Edition",
          NEEDS_TRANS ("Orchestra: strings, woodwinds, brass, percussion, keys. SFZ for the FK Sampler."),
          "https://github.com/sgossner/VSCO-2-CE/archive/refs/tags/1.1.0.zip",
          "CC0, Versilian Studios",
          2400 * megabyte },
    };
}

const ContentPackage* ContentService::find (const juce::String& packageId) const
{
    const auto found = std::ranges::find (catalog, packageId, &ContentPackage::id);
    return found != catalog.end() ? &*found : nullptr;
}

juce::File ContentService::getPackageFolder (const juce::String& packageId) const
{
    return contentFolder.getChildFile (packageId);
}

bool ContentService::isInstalled (const juce::String& packageId) const
{
    return getPackageFolder (packageId).getChildFile (manifestName).existsAsFile();
}

core::Result<void> ContentService::install (const juce::String& packageId,
                                            const std::function<void (float)>& progress,
                                            const std::function<bool()>& shouldCancel)
{
    const auto* package = find (packageId);

    if (package == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Package not found.").toStdString());

    if (isInstalled (packageId))
        return {};

    contentFolder.createDirectory();

    // Archiv und entpackter Inhalt liegen kurz gleichzeitig auf der Platte: doppelte Größe nötig.
    if (const auto free = contentFolder.getBytesFreeOnVolume(); free > 0 && free < package->sizeBytes * 2)
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Not enough disk space: {needed} needed, {free} free."),
                                                  { { "needed", juce::File::descriptionOfSizeInBytes (package->sizeBytes * 2) },
                                                    { "free", juce::File::descriptionOfSizeInBytes (free) } })
                               .toStdString());

    const auto cancelled = [&shouldCancel] { return shouldCancel && shouldCancel(); };
    const auto archive = contentFolder.getChildFile (packageId + ".download.zip");
    const auto target = getPackageFolder (packageId);
    const auto cleanUp = [&archive, &target]
    {
        archive.deleteFile();
        target.deleteRecursively();
    };

    // Herunterladen in Blöcken, damit Fortschritt und Abbrechen möglich sind.
    {
        int statusCode = 0;
        auto stream = juce::URL (package->url)
                          .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                  .withConnectionTimeoutMs (15000)
                                                  .withNumRedirectsToFollow (5)
                                                  .withStatusCode (&statusCode));

        if (stream == nullptr || (statusCode != 0 && statusCode >= 400))
            return core::fail (core::ErrorCode::ioError,
                               plugins::i18n::format (TRANS ("Download not possible ({code}): {url}"),
                                                      { { "code", juce::String (statusCode) }, { "url", package->url } })
                                   .toStdString());

        archive.deleteFile();
        juce::FileOutputStream out (archive);

        if (out.failedToOpen())
            return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("File is not writable: {path}"), { { "path", archive.getFullPathName() } }).toStdString());

        const auto total = stream->getTotalLength() > 0 ? stream->getTotalLength() : package->sizeBytes;
        juce::HeapBlock<char> buffer (64 * 1024);
        juce::int64 done = 0;

        while (! stream->isExhausted())
        {
            if (cancelled())
            {
                out.flush();
                cleanUp();
                return core::fail (core::ErrorCode::cancelled, TRANS ("Download cancelled.").toStdString());
            }

            const auto read = stream->read (buffer.getData(), 64 * 1024);

            if (read <= 0)
                break;

            if (! out.write (buffer.getData(), static_cast<size_t> (read)))
            {
                cleanUp();
                return core::fail (core::ErrorCode::ioError, TRANS ("Writing failed (disk full?).").toStdString());
            }

            done += read;

            if (progress && total > 0)
                progress (0.9f * static_cast<float> (std::min (done, total)) / static_cast<float> (total));
        }

        out.flush();
    }

    // Entpacken.
    juce::ZipFile zip (archive);

    if (zip.getNumEntries() == 0)
    {
        cleanUp();
        return core::fail (core::ErrorCode::ioError, TRANS ("The archive is empty or not a ZIP file.").toStdString());
    }

    target.createDirectory();

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        if (cancelled())
        {
            cleanUp();
            return core::fail (core::ErrorCode::cancelled, TRANS ("Installation cancelled.").toStdString());
        }

        if (const auto result = zip.uncompressEntry (i, target); result.failed())
        {
            cleanUp();
            return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Extraction failed: {error}"), { { "error", result.getErrorMessage() } }).toStdString());
        }

        if (progress)
            progress (0.9f + 0.1f * static_cast<float> (i + 1) / static_cast<float> (zip.getNumEntries()));
    }

    archive.deleteFile();

    auto manifest = std::make_unique<juce::DynamicObject>();
    manifest->setProperty ("id", packageId);
    manifest->setProperty ("installed", juce::Time::getCurrentTime().toISO8601 (true));
    target.getChildFile (manifestName).replaceWithText (juce::JSON::toString (juce::var (manifest.release())));
    return {};
}

core::Result<void> ContentService::remove (const juce::String& packageId)
{
    if (find (packageId) == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Package not found.").toStdString());

    if (! isInstalled (packageId))
        return core::fail (core::ErrorCode::notFound, TRANS ("The package is not installed.").toStdString());

    if (! getPackageFolder (packageId).deleteRecursively())
        return core::fail (core::ErrorCode::ioError, TRANS ("Could not delete the package folder.").toStdString());

    return {};
}

std::vector<ContentInstrument> ContentService::getInstruments() const
{
    std::vector<ContentInstrument> instruments;

    for (const auto& package : catalog)
    {
        if (! isInstalled (package.id))
            continue;

        const auto folder = getPackageFolder (package.id);

        for (const auto& file : folder.findChildFiles (juce::File::findFiles, true, "*.sfz"))
            instruments.push_back ({ file.getFileNameWithoutExtension(),
                                     file.getParentDirectory().getRelativePathFrom (folder).replaceCharacter ('\\', '/'),
                                     package.id,
                                     file });
    }

    std::ranges::sort (instruments,
                       [] (const ContentInstrument& a, const ContentInstrument& b)
                       {
                           if (a.packageId != b.packageId)
                               return a.packageId < b.packageId;

                           return a.name.compareNatural (b.name) < 0;
                       });

    return instruments;
}

} // namespace fk::services
