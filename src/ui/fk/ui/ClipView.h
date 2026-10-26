#pragma once

#include "fk/services/TrackService.h"
#include "fk/services/WaveformService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace fk::ui
{

/// Ein Audio-Clip in der Spur: farbiger Block mit Name und Wellenform.
/// Die Wellenform liefert der WaveformService als juce::AudioThumbnailBase; sie zeichnet
/// sich selbst nach, sobald im Hintergrund mehr Daten berechnet sind.
class ClipView final : public juce::Component
{
public:
    ClipView (services::WaveformService& waveformService, services::ClipInfo clipInfo, juce::Colour trackColour);

    [[nodiscard]] const services::ClipInfo& getClipInfo() const noexcept;

    /// Neuer Stand desselben Clips (verschoben, getrimmt, Noten geändert). Die Wellenform bleibt,
    /// solange die Quelldatei dieselbe ist; nur ein Dateiwechsel (Take) erzeugt sie neu.
    void setClipInfo (services::ClipInfo newInfo);

    /// Griffe zum Ziehen der Fades (lokale Koordinaten), leer bei MIDI-Clips.
    [[nodiscard]] juce::Rectangle<int> getFadeInHandle() const;
    [[nodiscard]] juce::Rectangle<int> getFadeOutHandle() const;

    /// Zeigt Fades während des Ziehens, bevor das Modell geändert wird (ein Undo-Schritt beim Loslassen).
    void setPreviewFades (double fadeInSeconds, double fadeOutSeconds);

    /// Zeigt Anfang, Länge und Quellversatz während des Trimmens, bevor das Modell geändert wird.
    void setPreviewPosition (double startSeconds, double lengthSeconds, double offsetSeconds);

    /// Ausgewählte Clips tragen einen hellen Rahmen (Auswahlmodell im ArrangeView).
    void setSelected (bool shouldBeSelected);
    [[nodiscard]] bool isSelected() const noexcept;

    void paint (juce::Graphics& g) override;

private:
    static constexpr int handleSize = 8;

    [[nodiscard]] int xForSeconds (double secondsFromClipStart) const;

    services::WaveformService& waveforms;
    services::ClipInfo info;
    juce::Colour colour;
    std::unique_ptr<juce::AudioThumbnailBase> thumbnail;
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipView)
};

} // namespace fk::ui
