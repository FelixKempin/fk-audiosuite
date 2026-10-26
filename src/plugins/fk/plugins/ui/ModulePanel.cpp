#include "fk/plugins/ui/ModulePanel.h"

#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins::ui
{

ModulePanel::ModulePanel (juce::String panelTitle)
    : title (std::move (panelTitle))
{
}

void ModulePanel::setLocked (bool shouldBeLocked, const juce::String& hint)
{
    locked = shouldBeLocked;
    lockHint = hint;
    setTooltip (locked ? hint : juce::String());

    for (auto* child : getChildren())
        child->setEnabled (! locked);

    repaint();
}

bool ModulePanel::isLocked() const noexcept
{
    return locked;
}

juce::Rectangle<int> ModulePanel::getContentArea() const
{
    return getLocalBounds()
        .withTrimmedTop (theme::panelTitleHeight)
        .reduced (theme::spacingMedium, theme::spacingSmall);
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::backgroundPanel);
    g.fillRoundedRectangle (area, theme::cornerRadius);
    g.setColour (theme::borderSubtle);
    g.drawRoundedRectangle (area, theme::cornerRadius, 1.0f);

    auto titleArea = getLocalBounds().removeFromTop (theme::panelTitleHeight).reduced (theme::spacingMedium, 0);
    g.setColour (locked ? theme::textDisabled : theme::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));

    if (locked)
    {
        // Schloss als Vektorpfad: Bügel und Körper. Daneben der Hinweis ("In Pro enthalten").
        const auto lockArea = titleArea.removeFromRight (14).toFloat().reduced (2.0f, 5.0f);
        juce::Path lock;
        lock.addRoundedRectangle (lockArea.withTrimmedTop (lockArea.getHeight() * 0.45f), 1.5f);
        lock.addCentredArc (lockArea.getCentreX(),
                            lockArea.getY() + lockArea.getHeight() * 0.45f,
                            lockArea.getWidth() * 0.3f,
                            lockArea.getHeight() * 0.4f,
                            0.0f,
                            juce::MathConstants<float>::pi * 1.5f,
                            juce::MathConstants<float>::pi * 2.5f,
                            true);
        g.fillPath (lock);
        g.drawFittedText (title + "  " + lockHint, titleArea, juce::Justification::centredLeft, 1);
        return;
    }

    g.drawFittedText (title, titleArea, juce::Justification::centredLeft, 1);
}

} // namespace fk::plugins::ui
