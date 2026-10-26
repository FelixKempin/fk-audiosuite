#pragma once

#include "fk/engine/ExpressionMap.h"
#include "fk/services/MidiClipService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Artikulations-Lane im Key Editor (Cubase: Articulations-Lane): eine Zeile je Spielweise der
/// Expression Map, darin ein Block je Note mit dieser Spielweise. Klick in eine Zeile über einer Note
/// gibt ihr diese Spielweise (ausgewählte Noten: allen), ein zweiter Klick nimmt sie wieder weg.
class ArticulationLane final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int rowHeight = 14;

    /// Spielweise für diese Noten setzen (leer = entfernen).
    std::function<void (const std::vector<int>& noteIndices, const juce::String& articulationId)> onSet;

    ArticulationLane();

    void setMap (std::optional<engine::ExpressionMap> newMap);
    void setClip (std::optional<services::MidiClipInfo> newClip);
    void setSelectedNotes (std::vector<int> noteIndices);
    void setPixelsPerBeat (double pixels);
    void setScrollX (int x);
    void setHeaderWidth (int width);

    [[nodiscard]] int getPreferredHeight() const;
    [[nodiscard]] bool hasMap() const noexcept { return map.has_value() && ! map->articulations.empty(); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    [[nodiscard]] float xForBeats (double beats) const;

    std::optional<engine::ExpressionMap> map;
    std::optional<services::MidiClipInfo> clip;
    std::vector<int> selected;
    double pixelsPerBeat = 40.0;
    int scrollX = 0;
    int headerWidth = 56;
};

} // namespace fk::ui
