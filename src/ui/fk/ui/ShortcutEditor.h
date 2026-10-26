#pragma once

#include "fk/ui/Commands.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace fk::ui
{

/// Tastaturbefehle (Cubase: Bearbeiten > Tastaturbefehle). Links alle Befehle mit Suche, rechts die
/// Tasten des gewählten Befehls. "Taste hinzufügen" wartet auf die nächste gedrückte Taste; war sie
/// schon belegt, wechselt sie hierher und der Hinweis sagt, wo sie vorher lag.
class ShortcutEditor final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit ShortcutEditor (KeyBindings& keyBindings);
    ~ShortcutEditor() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    /// Für Tests und Doppelklick: Befehl wählen, Tastenaufnahme starten, Text des Hinweises.
    void selectCommand (Command command);
    void startCapture();
    void removeSelectedKey();
    void setSearchText (const juce::String& text);
    [[nodiscard]] bool isCapturing() const noexcept { return capturing; }
    [[nodiscard]] juce::String getMessage() const { return message.getText(); }
    [[nodiscard]] int getNumRows() override;

    static void show (KeyBindings& keyBindings);

private:
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override;
    void applyFilter();
    void refreshDetails();
    [[nodiscard]] std::optional<Command> selectedCommand() const;

    /// Liste der Tasten des gewählten Befehls (eigenes Modell, damit beide Listen getrennt bleiben).
    struct KeyListModel final : juce::ListBoxModel
    {
        std::vector<juce::KeyPress> keys;
        int getNumRows() override { return static_cast<int> (keys.size()); }
        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    };

    KeyBindings& bindings;
    std::vector<Command> visible; ///< Befehle nach der Suche, in Tabellenreihenfolge
    bool capturing = false;

    juce::TextEditor search;
    juce::ListBox commandList;
    juce::Label commandTitle;
    KeyListModel keyModel;
    juce::ListBox keyList;
    juce::TextButton addKeyButton;
    juce::TextButton removeKeyButton;
    juce::TextButton resetCommandButton;
    juce::TextButton resetAllButton;
    juce::Label message;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutEditor)
};

} // namespace fk::ui
