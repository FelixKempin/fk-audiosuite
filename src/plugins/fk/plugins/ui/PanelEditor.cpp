#include "fk/plugins/ui/PanelEditor.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

PanelEditor::PanelEditor (juce::AudioProcessor& owner,
                          juce::AudioProcessorValueTreeState& parameterState,
                          PanelDescription panelDescription,
                          edition::FeatureGate* featureGate)
    : juce::AudioProcessorEditor (owner)
    , title (panelDescription.title)
    , view (parameterState, std::move (panelDescription), featureGate)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    setSize (view.getPreferredWidth(), theme::editorHeaderHeight + view.getPreferredHeight());
}

PanelEditor::~PanelEditor()
{
    setLookAndFeel (nullptr);
}

int PanelEditor::getNumControls() const noexcept
{
    return view.getNumControls();
}

int PanelEditor::getNumUnresolvedControls() const noexcept
{
    return view.getNumUnresolvedControls();
}

juce::Component* PanelEditor::findControl (const juce::String& parameterId) const
{
    return view.findControl (parameterId);
}

void PanelEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);

    const auto header = getLocalBounds().removeFromTop (theme::editorHeaderHeight);
    g.setColour (theme::backgroundPanel);
    g.fillRect (header);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, static_cast<float> (getWidth()));

    g.setColour (theme::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawFittedText (title, header.reduced (theme::spacingLarge, 0), juce::Justification::centredLeft, 1);
}

void PanelEditor::resized()
{
    view.setBounds (getLocalBounds().withTrimmedTop (theme::editorHeaderHeight));
}

} // namespace fk::plugins::ui
