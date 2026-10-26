#include "fk/ui/PluginWindow.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

PluginWindow::PluginWindow (const juce::String& title, services::EditorPtr editorToShow)
    : juce::DocumentWindow (title, theme::backgroundBase, juce::DocumentWindow::closeButton)
    , editor (std::move (editorToShow))
{
    setUsingNativeTitleBar (true);
    // JUCE übernimmt hier den Besitz über einen rohen Zeiger (Framework-Vertrag): release() gibt ihn ab.
    setContentNonOwned (editor.get(), true);
    setResizable (editor->isResizable(), false);
    centreAroundComponent (nullptr, getWidth(), getHeight());
    setVisible (true);
}

PluginWindow::~PluginWindow()
{
    // Erst den Inhalt vom Fenster lösen, dann den Editor freigeben: Sonst zeigt das Fenster
    // beim eigenen Abbau auf einen schon gelöschten Inhalt.
    clearContentComponent();
    editor.reset();
}

void PluginWindow::closeButtonPressed()
{
    if (onClosed)
        onClosed();
}

} // namespace fk::ui
