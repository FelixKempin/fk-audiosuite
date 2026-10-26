#include "fk/ui/LowerZone.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

LowerZone::LowerZone (MixConsole& mixConsole,
                      VirtualKeyboard& virtualKeyboard,
                      KeyEditor& keyEditor,
                      MediaBrowser& mediaBrowser,
                      SampleEditor& sampleEditor,
                      DrumEditor& drumEditor,
                      ListEditor& listEditor,
                      InstrumentBrowser& instrumentBrowser)
{
    // Die Reiter besitzen ihre Inhalte nicht (false): Mixer und Keyboard gehören dem ProjectComponent.
    tabs.addTab (TRANS ("MixConsole"), theme::backgroundPanel, &mixConsole, false);
    tabs.addTab (TRANS ("Keyboard"), theme::backgroundPanel, &virtualKeyboard, false);
    tabs.addTab (TRANS ("Key Editor"), theme::backgroundPanel, &keyEditor, false);
    tabs.addTab (TRANS ("Media"), theme::backgroundPanel, &mediaBrowser, false);
    tabs.addTab (TRANS ("Sample Editor"), theme::backgroundPanel, &sampleEditor, false);
    tabs.addTab (TRANS ("Drum Editor"), theme::backgroundPanel, &drumEditor, false);
    tabs.addTab (TRANS ("List"), theme::backgroundPanel, &listEditor, false);
    tabs.addTab (TRANS ("Instruments"), theme::backgroundPanel, &instrumentBrowser, false);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);
}

void LowerZone::showMedia()
{
    tabs.setCurrentTabIndex (3);
}

void LowerZone::showSampleEditor()
{
    tabs.setCurrentTabIndex (4);
}

void LowerZone::showDrumEditor()
{
    tabs.setCurrentTabIndex (5);
}

void LowerZone::showListEditor()
{
    tabs.setCurrentTabIndex (6);
}

void LowerZone::showInstrumentBrowser()
{
    tabs.setCurrentTabIndex (7);
}

void LowerZone::showMixer()
{
    tabs.setCurrentTabIndex (0);
}

void LowerZone::showKeyboard()
{
    tabs.setCurrentTabIndex (1);
}

void LowerZone::showKeyEditor()
{
    tabs.setCurrentTabIndex (2);
}

int LowerZone::getCurrentTab() const
{
    return tabs.getCurrentTabIndex();
}

void LowerZone::resized()
{
    tabs.setBounds (getLocalBounds());
}

} // namespace fk::ui
