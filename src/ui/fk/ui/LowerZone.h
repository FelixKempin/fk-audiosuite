#pragma once

#include "fk/ui/DrumEditor.h"
#include "fk/ui/InstrumentBrowser.h"
#include "fk/ui/KeyEditor.h"
#include "fk/ui/ListEditor.h"
#include "fk/ui/MediaBrowser.h"
#include "fk/ui/MixConsole.h"
#include "fk/ui/SampleEditor.h"
#include "fk/ui/VirtualKeyboard.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Untere Zone (UI_CONCEPT.md §2) mit Reitern: MixConsole, Keyboard, Key Editor, Medien, Sample-Editor, Drum-Editor.
class LowerZone final : public juce::Component
{
public:
    LowerZone (MixConsole& mixConsole,
               VirtualKeyboard& virtualKeyboard,
               KeyEditor& keyEditor,
               MediaBrowser& mediaBrowser,
               SampleEditor& sampleEditor,
               DrumEditor& drumEditor,
               ListEditor& listEditor,
               InstrumentBrowser& instrumentBrowser);

    void showMixer();
    void showKeyboard();
    void showKeyEditor();
    void showMedia();
    void showSampleEditor();
    void showDrumEditor();
    void showListEditor();
    void showInstrumentBrowser();
    [[nodiscard]] int getCurrentTab() const;

    void resized() override;

private:
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};

} // namespace fk::ui
