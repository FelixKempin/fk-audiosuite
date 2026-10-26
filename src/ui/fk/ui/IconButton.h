#pragma once

#include "fk/ui/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::ui
{

/// Schaltfläche mit Symbol, optionaler Beschriftung und Tooltip. Der Standard für alle
/// Werkzeug- und Transportleisten (Regel: keine Funktion nur per Tastenkürzel).
/// Umschaltbar wie jeder juce::Button (setClickingTogglesState, Radiogruppen).
class IconButton final : public juce::Button
{
public:
    /// tooltip beschreibt die Funktion und nennt das Tastenkürzel, z. B. "Inspector ein/aus (F2)".
    IconButton (Icon icon, const juce::String& tooltip, const juce::String& label = {});

    /// Farbe im eingeschalteten Zustand, z. B. Rot für Aufnahme.
    void setOnColour (juce::Colour colour);

    /// Symbol tauschen, z. B. auf- und zugeklappt.
    void setIcon (Icon newIcon);

    /// Kompakt: nur das Symbol zeigen, die Beschriftung bleibt gespeichert (schmale Fenster).
    void setCompact (bool shouldBeCompact);

    /// Beschriftung neben dem Symbol ändern (leer = nur Symbol).
    void setLabel (const juce::String& newLabel);

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

private:
    Icon icon;
    juce::String label;
    juce::Colour onColour;
    bool compact = false;
};

} // namespace fk::ui
