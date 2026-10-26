#pragma once

#include "fk/core/Result.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <vector>

namespace fk::services
{

/// Ein ladbares Content-Paket (freie Sample-Bibliothek), wie es der Content-Manager anbietet.
struct ContentPackage
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::String url;       ///< ZIP-Archiv (https://, in Tests file://)
    juce::String licence;   ///< Lizenz und Urheber, steht auch in THIRD_PARTY.md
    juce::int64 sizeBytes = 0; ///< Größe des Downloads (ungefähr), für Anzeige und Platzprüfung
};

/// Ein spielbares Instrument aus einem installierten Paket: eine SFZ-Datei.
struct ContentInstrument
{
    juce::String name;      ///< Dateiname ohne Endung
    juce::String category;  ///< Unterordner im Paket (z. B. "Strings/Violin Section")
    juce::String packageId;
    juce::File sfzFile;
};

/// Content-Manager (B5): lädt Pakete herunter, entpackt sie in contentFolder/<id>, entfernt sie
/// wieder und listet die SFZ-Instrumente der installierten Pakete. install() blockiert und meldet
/// Fortschritt; die Oberfläche ruft es aus einem Hintergrund-Thread, Tests direkt.
class ContentService final
{
public:
    explicit ContentService (juce::File contentFolder, std::vector<ContentPackage> catalog = defaultCatalog());

    /// Die mitgelieferte Liste: Salamander Grand Piano und VSCO 2 CE.
    [[nodiscard]] static std::vector<ContentPackage> defaultCatalog();

    [[nodiscard]] const std::vector<ContentPackage>& getPackages() const noexcept { return catalog; }
    [[nodiscard]] bool isInstalled (const juce::String& packageId) const;
    [[nodiscard]] juce::File getPackageFolder (const juce::String& packageId) const;

    /// Herunterladen, entpacken, als installiert markieren. progress: 0..0,9 Download, dann Entpacken.
    /// shouldCancel wird regelmäßig gefragt; true bricht ab und räumt auf.
    [[nodiscard]] core::Result<void> install (const juce::String& packageId,
                                              const std::function<void (float progress)>& progress = nullptr,
                                              const std::function<bool()>& shouldCancel = nullptr);

    [[nodiscard]] core::Result<void> remove (const juce::String& packageId);

    /// Alle SFZ-Instrumente der installierten Pakete, nach Paket und Name sortiert.
    [[nodiscard]] std::vector<ContentInstrument> getInstruments() const;

private:
    [[nodiscard]] const ContentPackage* find (const juce::String& packageId) const;

    juce::File contentFolder;
    std::vector<ContentPackage> catalog;
};

} // namespace fk::services
