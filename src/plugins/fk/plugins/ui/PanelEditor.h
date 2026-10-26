#pragma once

#include "fk/plugins/edition/FeatureGate.h"
#include "fk/plugins/ui/PanelDescription.h"
#include "fk/plugins/ui/PanelView.h"
#include "fk/plugins/ui/PluginLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fk::plugins::ui
{

/// Das Fenster eines Plugins aus seiner Beschreibung: Kopfzeile mit Plugin-Namen, darunter die
/// Abschnitte (PanelView). Die Fenstergröße folgt dem Inhalt.
///
/// Tooltips zeigt der Host über sein TooltipWindow (DAW: ProjectComponent; Standalone: Produkt).
class PanelEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int maxWidth = PanelView::maxWidth;

    PanelEditor (juce::AudioProcessor& owner,
                 juce::AudioProcessorValueTreeState& parameters,
                 PanelDescription description,
                 edition::FeatureGate* featureGate = nullptr);
    ~PanelEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] int getNumControls() const noexcept;
    [[nodiscard]] int getNumUnresolvedControls() const noexcept;
    [[nodiscard]] juce::Component* findControl (const juce::String& parameterId) const;

private:
    juce::String title;
    PluginLookAndFeel lookAndFeel;
    PanelView view;
};

} // namespace fk::plugins::ui
