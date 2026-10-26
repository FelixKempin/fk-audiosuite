#pragma once

#include "fk/services/PreviewService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace fk::ui
{

/// Medien-Browser (UI_CONCEPT.md, FEATURES §12): Dateibaum mit Audiodateien, Wellenform und
/// Vorhören der gewählten Datei, Einfügen am Positionszeiger per Button oder Doppelklick,
/// Ziehen ins Arrangement (die Beschreibung des Drag-Vorgangs enthält die Pfade, zeilenweise).
class MediaBrowser final : public juce::Component, private juce::FileBrowserListener, private juce::Timer
{
public:
    static constexpr auto dragDescriptionPrefix = "fk-media:";

    /// Wird mit den gewählten Dateien gerufen (Einfügen-Button, Doppelklick).
    std::function<void (const juce::StringArray& files)> onImportRequested;

    MediaBrowser (services::PreviewService& previewService, services::WaveformService& waveformService);
    ~MediaBrowser() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void selectionChanged() override;
    void fileClicked (const juce::File& file, const juce::MouseEvent& event) override;
    void fileDoubleClicked (const juce::File& file) override;
    void browserRootChanged (const juce::File& newRoot) override;
    void timerCallback() override;

    void setRoot (const juce::File& folder);
    void togglePreview();
    [[nodiscard]] juce::StringArray getSelectedAudioFiles() const;

    services::PreviewService& preview;
    services::WaveformService& waveforms;

    juce::TimeSliceThread scanThread { "FK Medien" };
    juce::WildcardFileFilter filter { "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.mid;*.midi",
                                      "*",
                                      "Audio- und MIDI-Dateien" };
    juce::DirectoryContentsList contents { &filter, scanThread };
    juce::FileTreeComponent tree { contents };
    juce::ComboBox rootBox;
    juce::Label infoLabel;
    IconButton playButton;
    IconButton importButton;
    std::unique_ptr<juce::AudioThumbnailBase> thumbnail;
    juce::File selectedFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaBrowser)
};

} // namespace fk::ui
