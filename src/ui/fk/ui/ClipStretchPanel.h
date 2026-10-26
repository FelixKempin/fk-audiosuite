#pragma once

#include "fk/services/ClipService.h"
#include "fk/services/TrackService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace fk::ui
{

/// Sprechblase "Tempo und Tonhöhe" für einen Audio-Clip: folgt dem Projekttempo (mit Tempo des
/// Materials, erkennbar per Klick), Tonhöhe in Halbtönen, dazu die aktuelle Geschwindigkeit.
/// Jede Änderung wirkt sofort und ist ein Undo-Schritt.
class ClipStretchPanel final : public juce::Component
{
public:
    ClipStretchPanel (services::ClipService& clipService,
                      services::TrackService& trackService,
                      services::TrackHandle& track,
                      juce::String clipId);

    std::function<void (const juce::String& message)> onError;

    void resized() override;

private:
    void refresh();
    void applyFollow();
    void detect();
    void report (const core::Error& error) const;
    [[nodiscard]] services::ClipInfo currentClip() const;

    services::ClipService& clips;
    services::TrackService& tracks;
    services::TrackHandle& track; // gehört dem Edit; die Sprechblase schließt, bevor die Spur verschwindet
    juce::String clipId;

    juce::ToggleButton followToggle;
    juce::Label bpmLabel;
    juce::Label bpmField;
    juce::TextButton detectButton;
    juce::Label pitchLabel;
    juce::Slider pitchSlider;
    juce::Label speedInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipStretchPanel)
};

} // namespace fk::ui
