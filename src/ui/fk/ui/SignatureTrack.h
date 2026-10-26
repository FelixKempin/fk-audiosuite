#pragma once

#include "fk/services/SignatureService.h"
#include "fk/services/TimelineService.h"
#include "fk/ui/TrackLane.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

namespace fk::ui
{

/// Signaturspur zwischen Lineal und Spuren mit drei Lagen: Tempo (Punkte, Sprung oder Rampe),
/// Taktart (Wechsel am Taktanfang) und Tonart (Grundton und Skala). Klick in eine Lage setzt
/// einen Punkt am Raster, Klick auf einen Punkt bearbeitet ihn, Rechtsklick entfernt ihn,
/// Tempopunkte lassen sich ziehen (waagerecht Zeit, senkrecht BPM) und per Doppelklick zwischen
/// Sprung und Rampe umschalten. Links stehen die Lagennamen in Spurkopf-Breite.
class SignatureTrack final : public juce::Component,
                             public juce::SettableTooltipClient,
                             private services::SignatureService::Listener
{
public:
    static constexpr int laneHeight = 18;
    static constexpr int numLanes = 3;
    static constexpr int totalHeight = laneHeight * numLanes;

    SignatureTrack (services::SignatureService& signatureService, services::TimelineService& timelineService);
    ~SignatureTrack() override;

    void setSettings (LaneSettings newSettings);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    enum class Lane
    {
        tempo = 0,
        timeSignature = 1,
        key = 2
    };

    struct TempoDrag
    {
        int index = -1;
        double bpm = 120.0;
        double seconds = 0.0;
        bool moved = false;
    };

    void signatureChanged() override;
    void refresh();

    [[nodiscard]] Lane laneAt (int y) const;
    [[nodiscard]] juce::Rectangle<int> laneArea (Lane lane) const;
    [[nodiscard]] int xForSeconds (double seconds) const;
    [[nodiscard]] double secondsForX (int x, bool snap) const;
    [[nodiscard]] float yForBpm (double bpm) const;
    [[nodiscard]] double bpmForY (float y) const;
    [[nodiscard]] int findTempoPointAt (juce::Point<int> position) const;
    [[nodiscard]] int findMarkerAt (Lane lane, int x) const;

    void paintTempoLane (juce::Graphics& g);
    void paintMarkerLane (juce::Graphics& g, Lane lane);
    void showTimeSignatureMenu (juce::Point<int> screenPosition, double seconds, int existingIndex);
    void showKeyChooser (juce::Rectangle<int> screenArea, double seconds, int existingIndex);
    void reportError (const core::Error& error) const;

    services::SignatureService& signature;
    services::TimelineService& timeline;
    LaneSettings settings;
    std::vector<services::TempoPointInfo> tempoPoints;
    std::vector<services::TimeSignaturePointInfo> timeSignaturePoints;
    std::vector<services::KeyPointInfo> keyPoints;
    std::optional<TempoDrag> tempoDrag;
    double minBpm = 40.0;
    double maxBpm = 200.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignatureTrack)
};

} // namespace fk::ui
