#include "fk/services/UpdateService.h"

namespace fk::services
{

namespace
{
/// Die Adresse kommt vom Server und wird im Browser geöffnet: nur https, damit ein verändertes
/// Manifest keine lokale Datei (file://) und kein fremdes URL-Schema auslöst.
bool isDownloadUrl (const juce::String& url)
{
    return url.startsWithIgnoreCase ("https://") && ! url.containsAnyOf (" \t\r\n");
}
} // namespace

UpdateService::UpdateService (juce::String url, juce::String version, juce::File file, juce::String platformName)
    : manifestUrl (std::move (url))
    , currentVersion (std::move (version))
    , settingsFile (std::move (file))
    , platform (std::move (platformName))
{
}

bool UpdateService::isAutomaticCheckEnabled() const
{
    return settingsFile == juce::File() || settingsFile.loadFileAsString().trim() != "aus";
}

void UpdateService::setAutomaticCheckEnabled (bool enabled)
{
    if (settingsFile == juce::File())
        return;

    settingsFile.getParentDirectory().createDirectory();
    settingsFile.replaceWithText (enabled ? "an" : "aus");
}

core::Result<UpdateInfo> UpdateService::parseManifest (const juce::String& json, const juce::String& platform)
{
    const auto parsed = juce::JSON::parse (json);

    // fk-licensing: {"releases": [{"version", "platform": "linux"|"windows", "file", "url", ...}]}.
    // Je Plattform kann es Installer und Zip geben; der Installer (nicht .zip) gewinnt.
    if (const auto* releases = parsed.getProperty ("releases", {}).getArray())
    {
        const auto kind = platform.upToFirstOccurrenceOf ("-", false, false);
        std::optional<UpdateInfo> best;
        bool bestIsZip = false;

        for (const auto& release : *releases)
        {
            if (release.getProperty ("platform", {}).toString() != kind)
                continue;

            UpdateInfo info { release.getProperty ("version", {}).toString(),
                              release.getProperty ("url", {}).toString(),
                              release.getProperty ("published_at", {}).toString().substring (0, 10),
                              {} };
            const bool isZip = release.getProperty ("file", {}).toString().endsWithIgnoreCase (".zip");

            if (info.version.isEmpty() || ! isDownloadUrl (info.url))
                continue;

            const bool better = ! best || isNewer (info.version, best->version)
                                || (info.version == best->version && bestIsZip && ! isZip);
            if (better)
            {
                best = info;
                bestIsZip = isZip;
            }
        }

        if (! best)
            return core::fail (core::ErrorCode::notFound, TRANS ("No version has been released for this platform yet.").toStdString());

        return *best;
    }
    UpdateInfo info { parsed.getProperty ("version", {}).toString(),
                      parsed.getProperty ("url", {}).toString(),
                      parsed.getProperty ("date", {}).toString(),
                      parsed.getProperty ("channel", {}).toString() };

    if (! parsed.isObject() || info.version.isEmpty() || info.url.isEmpty())
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Update manifest incomplete (version and url expected).").toStdString());

    if (! isDownloadUrl (info.url))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Update manifest names no https download address.").toStdString());

    return info;
}

bool UpdateService::isNewer (const juce::String& candidate, const juce::String& current)
{
    juce::StringArray a, b;
    a.addTokens (candidate.trim(), ".", {});
    b.addTokens (current.upToFirstOccurrenceOf (" ", false, false).trim(), ".", {});

    for (int i = 0; i < std::max (a.size(), b.size()); ++i)
    {
        const int left = i < a.size() ? a[i].getIntValue() : 0;
        const int right = i < b.size() ? b[i].getIntValue() : 0;

        if (left != right)
            return left > right;
    }

    return false;
}

core::Result<std::optional<UpdateInfo>> UpdateService::check() const
{
    if (! isAvailable())
        return core::fail (core::ErrorCode::unsupported, TRANS ("This build does not check for updates.").toStdString());

    int statusCode = 0;
    auto stream = juce::URL (manifestUrl)
                      .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                              .withConnectionTimeoutMs (8000)
                                              .withNumRedirectsToFollow (3)
                                              .withStatusCode (&statusCode));

    if (stream == nullptr || statusCode >= 400)
        return core::fail (core::ErrorCode::ioError, TRANS ("Update server not reachable.").toStdString());

    const auto info = parseManifest (stream->readEntireStreamAsString(), platform);

    if (! info && info.error().code == core::ErrorCode::notFound)
        return std::optional<UpdateInfo>(); // nichts veröffentlicht: kein Update

    if (! info)
        return std::unexpected (info.error());

    if (isNewer (info->version, currentVersion))
        return std::optional<UpdateInfo> (*info);

    return std::optional<UpdateInfo>();
}

} // namespace fk::services
