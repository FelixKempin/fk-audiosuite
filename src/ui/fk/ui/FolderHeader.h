#pragma once

#include "fk/services/TrackService.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/TimeMapping.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace fk::ui
{

/// Kopf einer Ordnerspur: Auf-/Zuklappen, Name, Mute/Solo für den ganzen Inhalt. Ziehen am Namen
/// verschiebt den Ordner samt Inhalt, Rechtsklick: Umbenennen, Farbe, Auflösen, mit Inhalt löschen.
class FolderHeader final : public juce::Component
{
public:
    FolderHeader (services::TrackService& trackService, services::FolderHandle& folder);
    ~FolderHeader() override;

    /// Wie beim Spurkopf: Zeiger bewegt und losgelassen (Bildschirm-y).
    std::function<void (int screenY)> onReorderDrag;
    std::function<void (int screenY)> onReorderDrop;
    std::function<void (const juce::String& message)> onError;

    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    void showContextMenu();
    [[nodiscard]] bool isDragHandle (const juce::MouseEvent& event) const;

    services::TrackService& tracks;
    services::FolderHandle& folder; // Handle: gehört dem Edit; das Arrangement baut bei Änderungen neu
    services::FolderInfo info;
    bool reordering = false;

    IconButton collapseButton;
    juce::Label nameLabel;
    juce::TextButton muteButton;
    juce::TextButton soloButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderHeader)
};

/// Zeitachse einer Ordnerspur: zeigt die Clips aller Spuren darin als flache Blöcke in deren Farbe,
/// damit man auch zugeklappt sieht, wo im Song etwas passiert.
class FolderLane final : public juce::Component
{
public:
    FolderLane (services::TrackService& trackService, services::FolderHandle& folder);

    void setMapping (TimeMapping newMapping);
    void refresh();
    void paint (juce::Graphics& g) override;

private:
    struct Block
    {
        double start = 0.0;
        double end = 0.0;
        juce::Colour colour;
    };

    services::TrackService& tracks;
    services::FolderHandle& folder;
    std::vector<Block> blocks;
    juce::Colour folderColour;
    TimeMapping mapping;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderLane)
};

} // namespace fk::ui
