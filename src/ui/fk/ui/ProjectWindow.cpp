#include "fk/ui/ProjectWindow.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

ProjectWindow::ProjectWindow (const juce::String& title, UiServices services)
    : juce::DocumentWindow (title, theme::backgroundBase, juce::DocumentWindow::allButtons)
    , content (services)
{
    setUsingNativeTitleBar (true);
    setContentNonOwned (&content, true);
    setResizable (true, false);
    centreWithSize (1280, 720);
    setVisible (true);
    content.grabKeyboardFocus();
}

void ProjectWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void ProjectWindow::closeProjectThen (std::function<void()> action)
{
    content.closeProjectThen (std::move (action));
}

} // namespace fk::ui
