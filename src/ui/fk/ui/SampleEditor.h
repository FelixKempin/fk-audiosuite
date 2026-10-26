#pragma once

#include "fk/services/ClipService.h"
#include "fk/services/TrackService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>

namespace fk::ui
{

/// Sample-Editor in der unteren Zone: ein Audio-Clip groß, mit Abschnittswahl (ziehen), Zoom
/// (Strg+Rad, Buttons) und Bearbeitungen des Materials (Stille, Verstärken, Normalisieren, Fades,
/// Umkehren, Phase). Ohne Auswahl wirkt eine Bearbeitung auf den ganzen Clip. Jede Bearbeitung
/// schreibt eine neue Datei und ist ein Undo-Schritt (ClipService::processAudio).
class SampleEditor final : public juce::Component, private services::TrackService::Listener
{
public:
    SampleEditor (services::ClipService& clipService,
                  services::TrackService& trackService,
                  services::WaveformService& waveformService);
    ~SampleEditor() override;

    /// Diesen Clip zeigen; Spur über ihre Kennung, damit nichts ins Leere zeigt, wenn sie verschwindet.
    void setClip (const juce::String& trackId, const juce::String& clipId);

    std::function<void (const juce::String& message)> onError;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    void tracksChanged() override;
    void contentChanged() override;
    void refresh();
    void apply (services::ClipService::AudioProcess process, float gainDb = 0.0f);
    void showGainMenu();
    void zoom (double factor, double anchorSeconds);
    void zoomToFit();
    [[nodiscard]] std::optional<services::ClipInfo> currentClip() const;
    [[nodiscard]] juce::Rectangle<int> waveformArea() const;
    [[nodiscard]] double secondsAtX (int x) const;
    [[nodiscard]] int xForSeconds (double seconds) const;

    services::ClipService& clips;
    services::TrackService& tracks;
    services::WaveformService& waveforms;

    juce::String trackId;
    juce::String clipId;
    std::optional<services::ClipInfo> info; ///< Stand des letzten refresh(); leer = kein Clip
    juce::File shownFile;
    std::unique_ptr<juce::AudioThumbnailBase> thumbnail;

    double visibleStart = 0.0;                          ///< Clip-Zeit am linken Rand
    double visibleLength = 1.0;                         ///< sichtbare Clip-Zeit in Sekunden
    std::optional<std::pair<double, double>> selection; ///< Abschnitt in Clip-Zeit
    double dragAnchor = 0.0;

    IconButton silenceButton;
    IconButton gainButton;
    IconButton normalizeButton;
    IconButton fadeInButton;
    IconButton fadeOutButton;
    IconButton reverseButton;
    IconButton phaseButton;
    IconButton zoomInButton;
    IconButton zoomOutButton;
    IconButton zoomFitButton;
    juce::Label infoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleEditor)
};

} // namespace fk::ui
