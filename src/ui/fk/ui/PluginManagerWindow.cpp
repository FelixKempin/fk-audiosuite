#include "fk/ui/PluginManagerWindow.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

PluginManagerWindow::PluginManagerWindow (services::PluginService& pluginService)
    : juce::DocumentWindow (TRANS ("Plugin Manager"), theme::backgroundBase, juce::DocumentWindow::closeButton)
    , libraryPanel (pluginService)
    , listComponent (pluginService.getFormatManager(),
                     pluginService.getKnownPluginList(),
                     pluginService.getDeadMansPedalFile(),
                     nullptr)
{
    setUsingNativeTitleBar (true);
    tabs.addTab (TRANS ("My Plugins"), theme::backgroundBase, &libraryPanel, false);
    tabs.addTab (TRANS ("Search and Scan"), theme::backgroundBase, &listComponent, false);
    tabs.setSize (820, 520);
    setContentNonOwned (&tabs, true);
    setResizable (true, false);
    listComponent.setNumberOfThreadsForScanning (1); // ein Scan-Unterprozess reicht und hält die Logausgabe lesbar
    centreWithSize (820, 520);
    setVisible (true);
}

void PluginManagerWindow::closeButtonPressed()
{
    if (onClosed)
        onClosed();
}

} // namespace fk::ui
