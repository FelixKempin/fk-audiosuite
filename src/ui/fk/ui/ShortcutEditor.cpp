#include "fk/ui/ShortcutEditor.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

ShortcutEditor::ShortcutEditor (KeyBindings& keyBindings)
    : bindings (keyBindings)
    , addKeyButton (TRANS ("Add Key..."))
    , removeKeyButton (TRANS ("Remove Key"))
    , resetCommandButton (TRANS ("Default"))
    , resetAllButton (TRANS ("All to Cubase Defaults"))
{
    setWantsKeyboardFocus (true);

    search.setTextToShowWhenEmpty (TRANS ("Search command or key, e.g. Zoom or Ctrl+D"), theme::textSecondary);
    search.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible (search);

    commandList.setModel (this);
    commandList.setRowHeight (24);
    commandList.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    commandList.setTooltip (TRANS ("Double-click: record a new key for this command"));
    addAndMakeVisible (commandList);

    commandTitle.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (commandTitle);

    keyList.setModel (&keyModel);
    keyList.setRowHeight (24);
    keyList.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    addAndMakeVisible (keyList);

    addKeyButton.setTooltip (TRANS ("Then press the key you want (with Ctrl, Alt, Shift); Esc cancels"));
    addKeyButton.onClick = [this] { startCapture(); };
    removeKeyButton.setTooltip (TRANS ("Remove the selected key from this command"));
    removeKeyButton.onClick = [this] { removeSelectedKey(); };
    resetCommandButton.setTooltip (TRANS ("Reset this command to its Cubase key"));
    resetCommandButton.onClick = [this]
    {
        if (const auto command = selectedCommand())
        {
            bindings.resetToDefault (*command);
            message.setText (plugins::i18n::format (TRANS ("{command}: default restored."), { { "command", commandName (*command) } }), juce::dontSendNotification);
            refreshDetails();
        }
    };
    resetAllButton.setTooltip (TRANS ("Discard all custom changes and load the Cubase layout"));
    resetAllButton.onClick = [this]
    {
        bindings.resetAll();
        message.setText (TRANS ("All commands are back to the Cubase layout."), juce::dontSendNotification);
        refreshDetails();
    };

    for (auto* button : { &addKeyButton, &removeKeyButton, &resetCommandButton, &resetAllButton })
        addAndMakeVisible (button);

    message.setColour (juce::Label::textColourId, theme::textSecondary);
    message.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (message);

    applyFilter();
    commandList.selectRow (0);
    setSize (720, 520);
}

ShortcutEditor::~ShortcutEditor()
{
    commandList.setModel (nullptr);
    keyList.setModel (nullptr);
}

void ShortcutEditor::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingLarge);
    auto bottom = area.removeFromBottom (28);
    resetAllButton.setBounds (bottom.removeFromRight (200));

    auto left = area.removeFromLeft (area.getWidth() * 3 / 5);
    search.setBounds (left.removeFromTop (28));
    left.removeFromTop (theme::spacingSmall);
    commandList.setBounds (left);

    area.removeFromLeft (theme::spacingLarge);
    commandTitle.setBounds (area.removeFromTop (28));
    keyList.setBounds (area.removeFromTop (110));
    area.removeFromTop (theme::spacingSmall);

    for (auto* button : { &addKeyButton, &removeKeyButton, &resetCommandButton })
    {
        button->setBounds (area.removeFromTop (28));
        area.removeFromTop (theme::spacingSmall);
    }

    message.setBounds (area);
}

void ShortcutEditor::applyFilter()
{
    const auto text = search.getText().trim();
    const auto previous = selectedCommand();
    visible.clear();

    for (const auto& info : allCommands())
    {
        juce::String keys;

        for (const auto& key : bindings.keysFor (info.id))
            keys << describeKey (key) << ' ';

        if (text.isEmpty() || commandName (info.id).containsIgnoreCase (text) ||
            commandCategory (info.id).containsIgnoreCase (text) || keys.containsIgnoreCase (text))
            visible.push_back (info.id);
    }

    commandList.updateContent();
    commandList.repaint();

    if (previous)
        if (const auto found = std::ranges::find (visible, *previous); found != visible.end())
            commandList.selectRow (static_cast<int> (found - visible.begin()));

    refreshDetails();
}

void ShortcutEditor::setSearchText (const juce::String& text)
{
    search.setText (text, true);
    applyFilter();
}

std::optional<Command> ShortcutEditor::selectedCommand() const
{
    const auto row = commandList.getSelectedRow();

    if (row < 0 || row >= static_cast<int> (visible.size()))
        return std::nullopt;

    return visible[static_cast<std::size_t> (row)];
}

void ShortcutEditor::selectCommand (Command command)
{
    if (const auto found = std::ranges::find (visible, command); found != visible.end())
        commandList.selectRow (static_cast<int> (found - visible.begin()));

    refreshDetails();
}

void ShortcutEditor::refreshDetails()
{
    const auto command = selectedCommand();
    keyModel.keys = command ? bindings.keysFor (*command) : std::vector<juce::KeyPress>();
    keyList.updateContent();
    keyList.repaint();
    commandList.repaint();

    commandTitle.setText (command ? commandName (*command) : TRANS ("No command selected"), juce::dontSendNotification);

    for (auto* button : { &addKeyButton, &resetCommandButton })
        button->setEnabled (command.has_value());

    removeKeyButton.setEnabled (command.has_value() && ! keyModel.keys.empty());
    resetCommandButton.setEnabled (command.has_value() && ! bindings.isDefault (*command));
}

void ShortcutEditor::startCapture()
{
    if (! selectedCommand())
        return;

    capturing = true;
    addKeyButton.setButtonText (TRANS ("Press a key... (Esc cancels)"));
    message.setText (TRANS ("Now press the key, with Ctrl, Alt or Shift if you like."), juce::dontSendNotification);
    grabKeyboardFocus();
}

void ShortcutEditor::removeSelectedKey()
{
    const auto command = selectedCommand();
    const auto row = keyList.getSelectedRow();

    if (! command || keyModel.keys.empty())
        return;

    auto keys = keyModel.keys;
    const auto index = static_cast<std::size_t> (juce::jlimit (0, static_cast<int> (keys.size()) - 1, row));
    const auto removed = keys[index];
    keys.erase (keys.begin() + static_cast<std::ptrdiff_t> (index));
    bindings.setKeys (*command, keys);
    message.setText (plugins::i18n::format (TRANS ("{key} is now free."), { { "key", describeKey (removed) } }), juce::dontSendNotification);
    refreshDetails();
}

bool ShortcutEditor::keyPressed (const juce::KeyPress& key)
{
    if (! capturing)
        return false;

    capturing = false;
    addKeyButton.setButtonText (TRANS ("Add Key..."));

    if (key == juce::KeyPress::escapeKey)
    {
        message.setText (TRANS ("Cancelled."), juce::dontSendNotification);
        return true;
    }

    const auto command = selectedCommand();

    if (! command)
        return true;

    // Zeichen und Tastencode bleiben, Maustasten-Flags nicht: Die Taste soll unabhängig davon passen.
    const juce::KeyPress clean (key.getKeyCode(), key.getModifiers().withoutMouseButtons(), 0);
    auto keys = bindings.keysFor (*command);

    if (std::ranges::find (keys, clean) != keys.end())
    {
        message.setText (plugins::i18n::format (TRANS ("{key} already belongs to this command."), { { "key", describeKey (clean) } }), juce::dontSendNotification);
        return true;
    }

    keys.push_back (clean);
    const auto robbed = bindings.setKeys (*command, keys);
    auto text = plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{key} → {command}.")), { { "key", describeKey (clean) }, { "command", commandName (*command) } });

    for (const auto other : robbed)
        text << "\n" << plugins::i18n::format (TRANS ("Previously on \"{command}\", removed there."), { { "command", commandName (other) } });

    message.setText (text, juce::dontSendNotification);
    refreshDetails();
    return true;
}

int ShortcutEditor::getNumRows()
{
    return static_cast<int> (visible.size());
}

void ShortcutEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (visible.size()))
        return;

    const auto command = visible[static_cast<std::size_t> (row)];

    if (selected)
        g.fillAll (theme::accentSoft);

    g.setColour (theme::textSecondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (commandCategory (command), 8, 0, 90, height, juce::Justification::centredLeft, true);

    g.setColour (theme::textPrimary);
    g.setFont (juce::FontOptions (13.0f, bindings.isDefault (command) ? juce::Font::plain : juce::Font::bold));
    g.drawText (commandName (command), 100, 0, width - 230, height, juce::Justification::centredLeft, true);

    g.setColour (theme::textSecondary);
    g.drawText (bindings.shortcutText (command), width - 125, 0, 117, height, juce::Justification::centredRight, true);
}

void ShortcutEditor::selectedRowsChanged (int)
{
    capturing = false;
    addKeyButton.setButtonText (TRANS ("Add Key..."));
    refreshDetails();
}

void ShortcutEditor::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    startCapture();
}

void ShortcutEditor::KeyListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (keys.size()))
        return;

    if (selected)
        g.fillAll (theme::accentSoft);

    g.setColour (theme::textPrimary);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (describeKey (keys[static_cast<std::size_t> (row)]), 8, 0, width - 16, height,
                juce::Justification::centredLeft, true);
}

void ShortcutEditor::show (KeyBindings& keyBindings)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Keyboard Commands");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (new ShortcutEditor (keyBindings)); // JUCE-API verlangt besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

} // namespace fk::ui
