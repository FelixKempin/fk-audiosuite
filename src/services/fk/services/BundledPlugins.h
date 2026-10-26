#pragma once

#include "fk/core/Result.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace fk::services
{

/// Die Lite-Plugins, die mit der DAW ausgeliefert werden (FK Rig, ADR 0008 §3), als VST3-Bundles in
/// einem Ordner neben dem Programm. Sie sind für die DAW fremde Plugins wie jedes andere VST3.
///
/// Das AppImage hängt sich bei jedem Start an einem anderen Pfad ein; ein Plugin direkt daraus
/// hätte bei jedem Start einen neuen Pfad. Deshalb kopiert install() die Bundles in einen festen
/// VST3-Ordner (Linux ~/.vst3, dort finden sie auch andere DAWs). Unter Windows legt der Installer
/// sie nach Common Files\VST3 (wie der Einzel-Installer des Plugins, nie zwei Kopien) und nennt sie in
/// bundled-plugins.txt; das Zip hat sie in <Programm>\VST3. Beide Male bleiben sie, wo sie sind.
class BundledPlugins final
{
public:
    struct Folders
    {
        juce::File source; ///< mitgelieferte Bundles; fehlt in Entwickler-Builds
        juce::File target; ///< fester VST3-Ordner; leer = Bundles an Ort und Stelle benutzen
        juce::StringArray onlyNames = {}; ///< nicht leer: nur diese Bundles aus source (geteilter Ordner)
    };

    /// versionTag kennzeichnet den Stand der Auslieferung (Version und Git-Stand). Ein Bundle im Ziel
    /// mit anderem Stand wird ersetzt, eines ohne Kennzeichen (selbst installiert) nie.
    BundledPlugins (Folders folders, juce::String versionTag);

    /// Ordner für diese Plattform, abgeleitet vom laufenden Programm.
    [[nodiscard]] static Folders defaultFolders();

    /// Kopiert neue oder geänderte Bundles ins Ziel und liefert die Pfade, unter denen die DAW sie
    /// kennen soll. Leer, wenn nichts mitgeliefert ist. Dateisystem, Message-Thread.
    [[nodiscard]] core::Result<std::vector<juce::File>> install() const;

    /// Datei im Bundle, die den Stand der Auslieferung festhält.
    static constexpr auto markerFileName = ".fk-bundled";

private:
    Folders folders;
    juce::String versionTag;
};

} // namespace fk::services
