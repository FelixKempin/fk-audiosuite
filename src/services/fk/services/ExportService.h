#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <functional>
#include <vector>

namespace fk::services
{

/// Dateiformat des Exports. MP3 braucht die LAME-Bibliothek des Systems (libmp3lame), siehe isFormatAvailable().
enum class ExportFormat
{
    wav,
    flac,
    aiff,
    ogg,
    mp3
};

/// Was exportiert wird.
enum class ExportRange
{
    wholeProject, ///< vom Anfang bis zum Ende des letzten Clips
    loopRange     ///< der Loop-Bereich aus dem Lineal
};

/// Anheben oder Absenken des fertigen Exports.
enum class NormaliseMode
{
    off,
    peak,    ///< Spitzenpegel auf targetDb (dBFS)
    loudness ///< integrierte Lautheit auf targetDb (LUFS, EBU R128), True Peak bleibt unter −1 dBTP
};

struct ExportOptions
{
    juce::File file; ///< Zieldatei; beim Stem-Export der Zielordner
    ExportFormat format = ExportFormat::wav;
    ExportRange range = ExportRange::wholeProject;
    int bitDepth = 24;        ///< 16, 24 oder 32 (Float); nur WAV, FLAC, AIFF
    double sampleRate = 0.0;  ///< Ziel-Samplerate; 0 = Rate des Audiogeräts
    bool dither = false;      ///< Rauschformung beim Kürzen auf 16 oder 24 Bit
    int quality = -1;         ///< Index in getQualityChoices(); −1 = Vorgabe des Formats
    bool includeTails = true; ///< Hall und Echo nach dem Ende ausklingen lassen (höchstens 10 s)
    NormaliseMode normalise = NormaliseMode::off;
    float normaliseTargetDb = 0.0f;
};

/// Eine Qualitätsstufe für verlustbehaftete Formate, z. B. "320 kbit/s CBR".
struct QualityChoice
{
    juce::String name;
    int index = 0; ///< Index für ExportOptions::quality
};

/// Export des Projekts: Mixdown (Master mit allen Plugins), Stems je Spur oder Gruppe, MIDI-Datei.
/// Alle Funktionen blockieren; die Oberfläche ruft sie aus einem Hintergrund-Thread mit
/// Fortschrittsfenster, Tests direkt.
class ExportService final
{
public:
    explicit ExportService (ProjectService& projectService);

    /// progress bekommt 0..1. Liefert die geschriebene Datei.
    [[nodiscard]] core::Result<juce::File> exportMixdown (const ExportOptions& options,
                                                          const std::function<void (float)>& progress = nullptr);

    /// Eine Datei je Spur (Kennungen aus TrackService) in den Ordner options.file, ohne Master-Plugins.
    /// Eine Gruppe enthält ihre Spuren; Sidechain- und Send-Quellen laufen stumm mit, damit der
    /// Stem so klingt wie im Mix.
    [[nodiscard]] core::Result<std::vector<juce::File>> exportStems (const ExportOptions& options,
                                                                     const std::vector<juce::String>& trackIds,
                                                                     const std::function<void (float)>& progress = nullptr);

    /// Standard-MIDI-Datei (Typ 1): Tempo und Taktart in der ersten Spur, danach eine Spur je
    /// Projektspur mit MIDI-Clips. Stumme Clips fehlen.
    [[nodiscard]] core::Result<juce::File> exportMidi (const juce::File& file);

    /// MP3 nur, wenn libmp3lame installiert ist; alle anderen immer.
    [[nodiscard]] bool isFormatAvailable (ExportFormat format) const;

    /// Qualitätsstufen für OGG und MP3 (leer für verlustfreie Formate) und die Vorgabe daraus.
    [[nodiscard]] std::vector<QualityChoice> getQualityChoices (ExportFormat format) const;
    [[nodiscard]] static int getDefaultQuality (ExportFormat format);

    /// Dateiendung für ein Format, z. B. ".wav".
    [[nodiscard]] static juce::String extensionFor (ExportFormat format);

    /// Verlustfreie Formate haben eine Bittiefe, OGG und MP3 eine Qualitätsstufe.
    [[nodiscard]] static bool isLossless (ExportFormat format);

private:
    [[nodiscard]] core::Result<te::TimeRange> rangeFor (te::Edit& edit, ExportRange range) const;
    [[nodiscard]] juce::AudioFormat* findFormat (ExportFormat format) const;
    [[nodiscard]] core::Result<juce::File> render (te::PlannedRenderJob job,
                                                   const std::function<void (float)>& progress);

    ProjectService& projectService;
};

} // namespace fk::services
