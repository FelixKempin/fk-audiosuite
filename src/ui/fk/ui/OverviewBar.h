#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Übersichtsleiste über dem Arrangement: das ganze Projekt verkleinert (eine dünne Zeile je Spur,
/// Clips als Blöcke in Spurfarbe) und ein Rahmen für den sichtbaren Ausschnitt. Rahmen ziehen
/// scrollt, seine Ränder ziehen zoomt, Klick daneben setzt den Ausschnitt mittig auf die Stelle.
class OverviewBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    /// Eine Spur, wie die Leiste sie zeichnet.
    struct Row
    {
        juce::Colour colour;
        std::vector<std::pair<double, double>> clips; ///< Start und Länge in Sekunden
    };

    /// Der neue Ausschnitt nach einer Mausaktion (Sekunden).
    std::function<void (double fromSeconds, double toSeconds)> onRangeChanged;

    OverviewBar();

    void setContent (std::vector<Row> newRows, double lengthSeconds);
    void setVisibleRange (double fromSeconds, double toSeconds);
    [[nodiscard]] double getLengthSeconds() const noexcept { return totalSeconds; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

    static constexpr int height = 30;

private:
    enum class Grip
    {
        none,
        move,
        left,
        right
    };

    [[nodiscard]] double xToSeconds (float x) const;
    [[nodiscard]] float secondsToX (double seconds) const;
    [[nodiscard]] Grip gripAt (float x) const;

    std::vector<Row> rows;
    double totalSeconds = 1.0;
    double visibleFrom = 0.0;
    double visibleTo = 1.0;

    struct Drag
    {
        Grip grip = Grip::none;
        double startFrom = 0.0;
        double startTo = 0.0;
        double anchorSeconds = 0.0;
    };
    std::optional<Drag> drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverviewBar)
};

} // namespace fk::ui
