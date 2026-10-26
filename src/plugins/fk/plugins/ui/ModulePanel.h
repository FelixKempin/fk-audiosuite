#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fk::plugins::ui
{

/// Rahmen mit Titelzeile für eine Gruppe von Bausteinen (Abschnitt eines Plugins, Modul einer
/// Kette). Gesperrt (Lite) zeigt er ein Schloss und einen Hinweis; der Inhalt bleibt sichtbar,
/// aber ausgegraut (Guideline 10a: nie unsichtbar). Den Inhalt ordnet der Besitzer über
/// getContentArea() an.
class ModulePanel final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModulePanel (juce::String title);

    void setLocked (bool shouldBeLocked, const juce::String& hint);
    [[nodiscard]] bool isLocked() const noexcept;

    [[nodiscard]] juce::Rectangle<int> getContentArea() const;

    void paint (juce::Graphics& g) override;

private:
    juce::String title;
    bool locked = false;
    juce::String lockHint;
};

} // namespace fk::plugins::ui
