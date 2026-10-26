#include "fk/ui/Commands.h"

#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <array>

namespace fk::ui
{

namespace
{
constexpr auto transport = NEEDS_TRANS ("Transport");
constexpr auto file = NEEDS_TRANS ("File");
constexpr auto edit = NEEDS_TRANS ("Edit");
constexpr auto tools = NEEDS_TRANS ("Tools");
constexpr auto zoom = NEEDS_TRANS ("Zoom");
constexpr auto view = NEEDS_TRANS ("View");

/// Die Tabelle aller Befehle mit den Tasten aus Cubase (soweit es den Befehl dort gibt).
const std::vector<CommandInfo> commandTable {
    { Command::play, "play", NEEDS_TRANS ("Play / Stop"), transport, { "spacebar" } },
    { Command::record, "record", NEEDS_TRANS ("Record"), transport, { "numpad *", "*" } },
    { Command::retrospectiveRecord, "retrospectiveRecord", NEEDS_TRANS ("Retrospective Record"), transport, { "shift + numpad *" } },
    { Command::toggleLoop, "toggleLoop", NEEDS_TRANS ("Loop On/Off"), transport, { "numpad /", "/" } },
    { Command::rewindBar, "rewindBar", NEEDS_TRANS ("Rewind (One Bar)"), transport, { "numpad -" } },
    { Command::forwardBar, "forwardBar", NEEDS_TRANS ("Forward (One Bar)"), transport, { "numpad +" } },
    { Command::returnToStart, "returnToStart", NEEDS_TRANS ("Go to Start"), transport, { "home" } },
    { Command::jumpLeftLocator, "jumpLeftLocator", NEEDS_TRANS ("Go to Left Locator"), transport, { "numpad 1" } },
    { Command::jumpRightLocator, "jumpRightLocator", NEEDS_TRANS ("Go to Right Locator"), transport, { "numpad 2" } },
    { Command::setLeftLocator, "setLeftLocator", NEEDS_TRANS ("Left Locator to Cursor"), transport, { "ctrl + numpad 1" } },
    { Command::setRightLocator, "setRightLocator", NEEDS_TRANS ("Right Locator to Cursor"), transport, { "ctrl + numpad 2" } },
    { Command::togglePunch, "togglePunch", NEEDS_TRANS ("Punch In/Out"), transport, { "i" } },
    { Command::addMarker, "addMarker", NEEDS_TRANS ("Add Marker"), transport, { "insert" } },
    { Command::previousMarker, "previousMarker", NEEDS_TRANS ("Previous Marker"), transport, { "shift + B" } },
    { Command::nextMarker, "nextMarker", NEEDS_TRANS ("Next Marker"), transport, { "shift + N" } },
    { Command::toggleMetronome, "toggleMetronome", NEEDS_TRANS ("Metronome On/Off"), transport, { "c" } },

    { Command::save, "save", NEEDS_TRANS ("Save"), file, { "ctrl + S" } },
    { Command::saveAs, "saveAs", NEEDS_TRANS ("Save As"), file, { "ctrl + shift + S" } },
    { Command::projectSettings, "projectSettings", NEEDS_TRANS ("Project Settings"), file, { "shift + S" } },
    { Command::exportProject, "exportProject", NEEDS_TRANS ("Export Project"), file, {} },

    { Command::undo, "undo", NEEDS_TRANS ("Undo"), edit, { "ctrl + Z" } },
    { Command::redo, "redo", NEEDS_TRANS ("Redo"), edit, { "ctrl + Y", "ctrl + shift + Z" } },
    { Command::history, "history", NEEDS_TRANS ("History"), edit, {} },
    { Command::cut, "cut", NEEDS_TRANS ("Cut"), edit, { "ctrl + X" } },
    { Command::copy, "copy", NEEDS_TRANS ("Copy"), edit, { "ctrl + C" } },
    { Command::paste, "paste", NEEDS_TRANS ("Paste at Cursor"), edit, { "ctrl + V" } },
    { Command::duplicate, "duplicate", NEEDS_TRANS ("Duplicate"), edit, { "ctrl + D" } },
    { Command::glue, "glue", NEEDS_TRANS ("Glue"), edit, { "ctrl + J" } },
    { Command::selectAll, "selectAll", NEEDS_TRANS ("Select All Clips"), edit, { "ctrl + A" } },
    { Command::deleteSelection, "deleteSelection", NEEDS_TRANS ("Delete"), edit, { "delete", "backspace" } },
    { Command::deleteAndClose,
      "deleteAndClose",
      NEEDS_TRANS ("Delete Range and Close Gap"),
      edit,
      { "shift + delete", "shift + backspace" } },
    { Command::toggleClipMute, "toggleClipMute", NEEDS_TRANS ("Mute / Unmute Clips"), edit, { "shift + M" } },
    { Command::nudgeLeft, "nudgeLeft", NEEDS_TRANS ("Nudge Selection Left"), edit, { "ctrl + cursor left" } },
    { Command::nudgeRight, "nudgeRight", NEEDS_TRANS ("Nudge Selection Right"), edit, { "ctrl + cursor right" } },

    { Command::toolSelect, "toolSelect", NEEDS_TRANS ("Select Tool"), tools, { "1" } },
    { Command::toolRange, "toolRange", NEEDS_TRANS ("Range Tool"), tools, { "2" } },
    { Command::toolSplit, "toolSplit", NEEDS_TRANS ("Split Tool"), tools, { "3" } },
    { Command::toolGlue, "toolGlue", NEEDS_TRANS ("Glue Tool"), tools, { "4" } },
    { Command::toolErase, "toolErase", NEEDS_TRANS ("Erase Tool"), tools, { "5" } },
    { Command::toolZoom, "toolZoom", NEEDS_TRANS ("Zoom Tool"), tools, { "6" } },
    { Command::toolMute, "toolMute", NEEDS_TRANS ("Mute Tool"), tools, { "7" } },
    { Command::toolDraw, "toolDraw", NEEDS_TRANS ("Draw Tool"), tools, { "8" } },

    { Command::zoomIn, "zoomIn", NEEDS_TRANS ("Zoom In"), zoom, { "h" } },
    { Command::zoomOut, "zoomOut", NEEDS_TRANS ("Zoom Out"), zoom, { "g" } },
    { Command::zoomProject, "zoomProject", NEEDS_TRANS ("Show Entire Project"), zoom, { "shift + F" } },
    { Command::zoomSelection, "zoomSelection", NEEDS_TRANS ("Zoom to Selection"), zoom, { "alt + S" } },

    { Command::inspector, "inspector", NEEDS_TRANS ("Inspector On/Off"), view, { "F2" } },
    { Command::mixer, "mixer", NEEDS_TRANS ("MixConsole"), view, { "F3" } },
    { Command::keyboard, "keyboard", NEEDS_TRANS ("On-Screen Keyboard"), view, { "F4" } },
    { Command::keyEditor, "keyEditor", NEEDS_TRANS ("Key Editor"), view, { "F5" } },
    { Command::mediaBrowser, "mediaBrowser", NEEDS_TRANS ("Media Browser"), view, { "F6" } },
    { Command::sampleEditor, "sampleEditor", NEEDS_TRANS ("Sample Editor"), view, { "F7" } },
    { Command::drumEditor, "drumEditor", NEEDS_TRANS ("Drum Editor"), view, { "F8" } },
    { Command::listEditor, "listEditor", NEEDS_TRANS ("List Editor"), view, { "F9" } },
    { Command::instrumentBrowser, "instrumentBrowser", NEEDS_TRANS ("Instrument Browser"), view, { "F10" } },
    { Command::overview, "overview", NEEDS_TRANS ("Overview Bar On/Off"), view, {} },
    { Command::shortcutEditor, "shortcutEditor", NEEDS_TRANS ("Keyboard Commands"), view, {} },
};

std::vector<juce::KeyPress> defaultKeys (const CommandInfo& info)
{
    std::vector<juce::KeyPress> keys;

    for (const auto* description : info.defaultKeys)
        keys.push_back (juce::KeyPress::createFromDescription (description));

    return keys;
}

bool isLetterOrDigit (int keyCode)
{
    return keyCode < 128 && juce::CharacterFunctions::isLetterOrDigit (static_cast<juce::juce_wchar> (keyCode));
}
} // namespace

const std::vector<CommandInfo>& allCommands()
{
    return commandTable;
}

const CommandInfo& infoFor (Command command)
{
    const auto found = std::ranges::find (commandTable, command, &CommandInfo::id);
    jassert (found != commandTable.end()); // jeder Befehl steht in der Tabelle
    return *found;
}

juce::String commandName (Command command)
{
    return juce::translate (infoFor (command).name);
}

juce::String commandCategory (Command command)
{
    return juce::translate (infoFor (command).category);
}

juce::String describeKey (const juce::KeyPress& key)
{
    juce::String text;
    const auto mods = key.getModifiers();

    if (mods.isCtrlDown())
        text << TRANS ("Ctrl+");

    if (mods.isAltDown())
        text << TRANS ("Alt+");

    if (mods.isShiftDown())
        text << TRANS ("Shift+");

    const auto code = key.getKeyCode();
    const std::array<std::pair<int, const char*>, 20> names { {
        { juce::KeyPress::spaceKey, NEEDS_TRANS ("Space") },
        { juce::KeyPress::returnKey, NEEDS_TRANS ("Enter") },
        { juce::KeyPress::escapeKey, NEEDS_TRANS ("Esc") },
        { juce::KeyPress::backspaceKey, NEEDS_TRANS ("Backspace") },
        { juce::KeyPress::deleteKey, NEEDS_TRANS ("Del") },
        { juce::KeyPress::insertKey, NEEDS_TRANS ("Insert") },
        { juce::KeyPress::homeKey, NEEDS_TRANS ("Home") },
        { juce::KeyPress::endKey, NEEDS_TRANS ("End") },
        { juce::KeyPress::pageUpKey, NEEDS_TRANS ("Page Up") },
        { juce::KeyPress::pageDownKey, NEEDS_TRANS ("Page Down") },
        { juce::KeyPress::leftKey, NEEDS_TRANS ("Left Arrow") },
        { juce::KeyPress::rightKey, NEEDS_TRANS ("Right Arrow") },
        { juce::KeyPress::upKey, NEEDS_TRANS ("Up Arrow") },
        { juce::KeyPress::downKey, NEEDS_TRANS ("Down Arrow") },
        { juce::KeyPress::tabKey, NEEDS_TRANS ("Tab") },
        { juce::KeyPress::numberPadAdd, NEEDS_TRANS ("Num +") },
        { juce::KeyPress::numberPadSubtract, NEEDS_TRANS ("Num -") },
        { juce::KeyPress::numberPadMultiply, NEEDS_TRANS ("Num *") },
        { juce::KeyPress::numberPadDivide, NEEDS_TRANS ("Num /") },
        { juce::KeyPress::numberPadDecimalPoint, NEEDS_TRANS ("Num .") },
    } };

    for (const auto& [keyCode, name] : names)
        if (code == keyCode)
            return text + juce::translate (name);

    if (code >= juce::KeyPress::numberPad0 && code <= juce::KeyPress::numberPad9)
        return text + plugins::i18n::format (TRANS ("Num {key}"), { { "key", juce::String (code - juce::KeyPress::numberPad0) } });

    const std::array<int, 12> functionKeys { juce::KeyPress::F1Key, juce::KeyPress::F2Key,  juce::KeyPress::F3Key,
                                             juce::KeyPress::F4Key, juce::KeyPress::F5Key,  juce::KeyPress::F6Key,
                                             juce::KeyPress::F7Key, juce::KeyPress::F8Key,  juce::KeyPress::F9Key,
                                             juce::KeyPress::F10Key, juce::KeyPress::F11Key, juce::KeyPress::F12Key };

    for (std::size_t i = 0; i < functionKeys.size(); ++i)
        if (code == functionKeys[i])
            return text + "F" + juce::String (static_cast<int> (i) + 1);

    if (code > 32 && code < 128)
        return text + juce::String::charToString (juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (code)));

    return text + key.getTextDescription();
}

KeyBindings::KeyBindings (services::ShortcutService& store)
    : shortcuts (store)
{
    reload();
}

void KeyBindings::reload()
{
    bindings.clear();
    const auto overrides = shortcuts.getOverrides();

    for (const auto& info : commandTable)
    {
        if (const auto found = overrides.find (info.key); found != overrides.end())
        {
            std::vector<juce::KeyPress> keys;

            for (const auto& description : found->second)
                if (const auto key = juce::KeyPress::createFromDescription (description); key.isValid())
                    keys.push_back (key);

            bindings[info.id] = keys;
        }
        else
        {
            bindings[info.id] = defaultKeys (info);
        }
    }
}

std::vector<juce::KeyPress> KeyBindings::keysFor (Command command) const
{
    if (const auto found = bindings.find (command); found != bindings.end())
        return found->second;

    return {};
}

std::optional<Command> KeyBindings::find (const juce::KeyPress& key) const
{
    for (const auto& [command, keys] : bindings)
        for (const auto& bound : keys)
            if (bound == key)
                return command;

    // Satzzeichen wie "*" und "/" liegen je nach Tastatur auf Shift-Kombinationen: dann zählt das Zeichen.
    const auto typed = key.getTextCharacter();

    if (typed > 32 && typed < 128 && ! isLetterOrDigit (static_cast<int> (typed)))
        for (const auto& [command, keys] : bindings)
            for (const auto& bound : keys)
                if (! bound.getModifiers().isAnyModifierKeyDown() && bound.getKeyCode() == static_cast<int> (typed))
                    return command;

    return std::nullopt;
}

juce::String KeyBindings::shortcutText (Command command) const
{
    const auto keys = keysFor (command);
    return keys.empty() ? juce::String() : describeKey (keys.front());
}

juce::String KeyBindings::menuText (const juce::String& text, Command command) const
{
    const auto shortcut = shortcutText (command);
    return shortcut.isEmpty() ? text : text + "\t" + shortcut;
}

std::vector<Command> KeyBindings::setKeys (Command command, const std::vector<juce::KeyPress>& keys)
{
    std::vector<Command> robbed;

    for (auto& [other, otherKeys] : bindings)
    {
        if (other == command)
            continue;

        const auto before = otherKeys.size();
        std::erase_if (otherKeys,
                       [&keys] (const juce::KeyPress& existing) { return std::ranges::find (keys, existing) != keys.end(); });

        if (otherKeys.size() != before)
        {
            robbed.push_back (other);
            store (other);
        }
    }

    bindings[command] = keys;
    store (command);

    if (onChanged)
        onChanged();

    return robbed;
}

void KeyBindings::resetToDefault (Command command)
{
    shortcuts.clearOverride (infoFor (command).key);
    reload();

    if (onChanged)
        onChanged();
}

void KeyBindings::resetAll()
{
    shortcuts.clearAll();
    reload();

    if (onChanged)
        onChanged();
}

bool KeyBindings::isDefault (Command command) const
{
    const auto defaults = defaultKeys (infoFor (command));
    const auto current = keysFor (command);
    return std::ranges::equal (defaults, current, [] (const auto& a, const auto& b) { return a == b; });
}

void KeyBindings::store (Command command)
{
    const auto& info = infoFor (command);

    if (isDefault (command))
    {
        shortcuts.clearOverride (info.key);
        return;
    }

    juce::StringArray descriptions;

    for (const auto& key : keysFor (command))
        descriptions.add (key.getTextDescription());

    shortcuts.setOverride (info.key, descriptions);
}

} // namespace fk::ui
