#pragma once

#include "fk/services/ShortcutService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Alle Befehle des Projektfensters, die eine Taste haben können. Buttons und Menüs rufen dieselben
/// Funktionen; die Tabelle (allCommands) sagt, wie ein Befehl heißt und welche Taste er in Cubase hat.
enum class Command
{
    play,
    record,
    retrospectiveRecord,
    toggleLoop,
    rewindBar,
    forwardBar,
    returnToStart,
    jumpLeftLocator,
    jumpRightLocator,
    setLeftLocator,
    setRightLocator,
    togglePunch,
    addMarker,
    previousMarker,
    nextMarker,
    toggleMetronome,

    save,
    saveAs,
    projectSettings,
    exportProject,

    undo,
    redo,
    history,
    cut,
    copy,
    paste,
    duplicate,
    glue,
    selectAll,
    deleteSelection,
    deleteAndClose,
    toggleClipMute,
    nudgeLeft,
    nudgeRight,

    toolSelect,
    toolRange,
    toolSplit,
    toolGlue,
    toolErase,
    toolZoom,
    toolMute,
    toolDraw,

    zoomIn,
    zoomOut,
    zoomProject,
    zoomSelection,

    inspector,
    mixer,
    keyboard,
    keyEditor,
    mediaBrowser,
    sampleEditor,
    drumEditor,
    listEditor,
    instrumentBrowser,
    overview,
    shortcutEditor
};

/// Ein Befehl in der Tabelle. key ist der stabile Name für die Einstellungen (nie umbenennen),
/// defaultKeys sind JUCE-Tastenbeschreibungen ("ctrl + Z", "numpad 1").
struct CommandInfo
{
    Command id;
    const char* key;
    const char* name;     ///< UTF-8
    const char* category; ///< UTF-8
    std::vector<const char*> defaultKeys;
};

[[nodiscard]] const std::vector<CommandInfo>& allCommands();
[[nodiscard]] const CommandInfo& infoFor (Command command);
[[nodiscard]] juce::String commandName (Command command);
[[nodiscard]] juce::String commandCategory (Command command);

/// Taste auf Deutsch, wie sie auf der Tastatur steht: "Strg+Z", "Leertaste", "Num 1", "Entf".
[[nodiscard]] juce::String describeKey (const juce::KeyPress& key);

/// Die gültige Belegung: Cubase-Standard aus der Tabelle, überschrieben durch die eigenen Änderungen
/// aus dem ShortcutService. Eine Taste gehört höchstens einem Befehl.
class KeyBindings final
{
public:
    explicit KeyBindings (services::ShortcutService& store);

    /// Gerufen nach jeder Änderung (Menüs und Tooltips neu beschriften).
    std::function<void()> onChanged;

    [[nodiscard]] std::vector<juce::KeyPress> keysFor (Command command) const;

    /// Befehl zu einer gedrückten Taste. Erst genaue Übereinstimmung, dann das getippte Zeichen
    /// ("*" kommt auf deutschen Tastaturen als Shift+Plus).
    [[nodiscard]] std::optional<Command> find (const juce::KeyPress& key) const;

    /// Erste Taste als Text, leer ohne Taste. Für Menüs ("\t" + Text) und Tooltips.
    [[nodiscard]] juce::String shortcutText (Command command) const;

    /// Menütext mit Taste rechts: "Rückgängig\tStrg+Z".
    [[nodiscard]] juce::String menuText (const juce::String& text, Command command) const;

    /// Tasten eines Befehls ersetzen. Belegt eine davon schon einen anderen Befehl, wird sie dort
    /// entfernt; die Rückgabe nennt diese Befehle.
    std::vector<Command> setKeys (Command command, const std::vector<juce::KeyPress>& keys);

    void resetToDefault (Command command);
    void resetAll();

    [[nodiscard]] bool isDefault (Command command) const;

private:
    void reload();
    void store (Command command);

    services::ShortcutService& shortcuts;
    std::map<Command, std::vector<juce::KeyPress>> bindings;
};

} // namespace fk::ui
