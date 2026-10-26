#pragma once

#include "fk/services/ExportService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace fk::ui
{

/// Export-Dialog: Mixdown, Stems (eine Datei je Spur oder Gruppe) oder MIDI-Datei. Format, Qualität,
/// Bittiefe, Samplerate, Dither, Normalisieren und Ausklingen. Der Export läuft in einem
/// Hintergrund-Thread mit Fortschrittsfenster, damit die Oberfläche bedienbar bleibt.
class ExportDialog final : public juce::Component
{
public:
    /// Dienste, die der Dialog braucht; alle leben länger als der Dialog.
    struct Services
    {
        services::ExportService& exporter;
        services::ProjectService& project;
        services::ProjectSettingsService& projectSettings;
        services::TrackService& tracks;
    };

    explicit ExportDialog (Services services);

    void resized() override;

    /// Öffnet den Dialog als eigenes Fenster.
    static void show (Services services);

    /// Was exportiert wird (Auswahl oben im Dialog).
    enum class Target
    {
        mixdown = 1,
        stems,
        midi
    };

    /// Für Tests: Ziel umstellen und die zusammengestellten Optionen lesen.
    void setTarget (Target target);
    [[nodiscard]] services::ExportOptions collectOptions() const;
    [[nodiscard]] std::vector<juce::String> getSelectedStemTrackIds() const;

private:
    struct StemRow
    {
        juce::String trackId;
        juce::ToggleButton toggle;
    };

    void chooseFile();
    void startExport();
    void updateVisibility();
    void fillQualityChoices();
    void updateTargetFile();
    [[nodiscard]] Target currentTarget() const;
    [[nodiscard]] services::ExportFormat currentFormat() const;

    services::ExportService& exporter;
    services::ProjectService& project;
    services::ProjectSettingsService& projectSettings;
    services::TrackService& tracks;

    juce::Label targetLabel;
    juce::ComboBox targetBox;
    juce::Label fileLabel;
    juce::Label fileName;
    juce::TextButton chooseFileButton;
    juce::Label formatLabel;
    juce::ComboBox formatBox;
    juce::Label rangeLabel;
    juce::ComboBox rangeBox;
    juce::Label bitDepthLabel;
    juce::ComboBox bitDepthBox;
    juce::Label qualityLabel;
    juce::ComboBox qualityBox;
    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateBox;
    juce::Label normaliseLabel;
    juce::ComboBox normaliseBox;
    juce::ToggleButton ditherToggle;
    juce::ToggleButton tailsToggle;
    juce::Label stemsLabel;
    juce::TextButton allStemsButton;
    juce::TextButton noStemsButton;
    juce::Component stemHolder;
    juce::Viewport stemViewport;
    std::vector<std::unique_ptr<StemRow>> stemRows;
    juce::Label hint;
    IconButton exportButton;
    juce::File targetFile; ///< Datei; bei Stems der Ordner

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportDialog)
};

} // namespace fk::ui
