#pragma once

#include "fk/core/BuildInfo.h"
#include "fk/core/Result.h"

#include <juce_core/juce_core.h>

#include <optional>

namespace fk::services
{

/// Eine veröffentlichte Version, wie sie latest-<Plattform>.json im Download-Bucket beschreibt.
struct UpdateInfo
{
    juce::String version;
    juce::String url; ///< direkter Download
    juce::String date;
    juce::String channel;
};

/// Update-Hinweis: liest, was veröffentlicht ist, und meldet, ob es eine neuere Version gibt. Installiert
/// nichts; der Nutzer lädt selbst. Zwei Quellen sind möglich:
/// - die Release-Liste von fk-licensing (`/v1/releases/?product=fk-audio-suite`, Auslieferungs-Builds):
///   gilt erst, wenn dort ein Mensch das Release veröffentlicht hat, und die Adresse hängt nicht am Bucket;
/// - latest-<Plattform>.json im Download-Bucket.
class UpdateService final
{
public:
    /// settingsFile hält den Schalter "beim Start prüfen" (leer = immer an).
    /// platform wie "linux-x64"; wählt in der Release-Liste die Einträge dieser Plattform.
    UpdateService (juce::String manifestUrl,
                   juce::String currentVersion,
                   juce::File settingsFile = {},
                   juce::String platform = core::buildInfo::platform);

    [[nodiscard]] bool isAutomaticCheckEnabled() const;
    void setAutomaticCheckEnabled (bool enabled);

    /// Aus, wenn der Build keine Adresse kennt (Entwickler-Builds, solange es keinen Bucket gibt).
    [[nodiscard]] bool isAvailable() const noexcept { return manifestUrl.isNotEmpty(); }

    /// Blockiert (Netz): neuere Version oder leer, wenn aktuell. Aus einem Hintergrund-Thread rufen.
    [[nodiscard]] core::Result<std::optional<UpdateInfo>> check() const;

    /// Ein Manifest oder eine Release-Liste ({"releases": [...]}, daraus die höchste Version der
    /// Plattform; notFound, wenn es für sie noch keine gibt).
    [[nodiscard]] static core::Result<UpdateInfo> parseManifest (const juce::String& json,
                                                                const juce::String& platform = "linux-x64");

    /// Versionen wie "0.10.2" stellenweise vergleichen (0.10 ist neuer als 0.9).
    [[nodiscard]] static bool isNewer (const juce::String& candidate, const juce::String& current);

private:
    juce::String manifestUrl;
    juce::String currentVersion;
    juce::File settingsFile;
    juce::String platform;
};

} // namespace fk::services
