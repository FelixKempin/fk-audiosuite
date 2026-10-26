#include "fk/ui/UndoHistoryPanel.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int rowHeight = 24;
constexpr int refreshHz = 5;
} // namespace

UndoHistoryPanel::UndoHistoryPanel (services::ProjectService& projectService)
    : project (projectService)
{
    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    list.setTooltip (TRANS ("Click a step: bring the project to the state after that step. Gray = undone, can be redone."));
    addAndMakeVisible (list);

    hint.setText (TRANS ("No changes yet. Every edit appears here as a step."),
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, theme::textSecondary);
    hint.setJustificationType (juce::Justification::centred);
    addChildComponent (hint);

    refresh();
    startTimerHz (refreshHz);
    setSize (360, 480);
}

UndoHistoryPanel::~UndoHistoryPanel()
{
    list.setModel (nullptr);
}

void UndoHistoryPanel::resized()
{
    list.setBounds (getLocalBounds());
    hint.setBounds (getLocalBounds().reduced (theme::spacingLarge));
}

void UndoHistoryPanel::refresh()
{
    auto latest = project.getUndoHistory();

    if (latest.done == history.done && latest.undone == history.undone)
        return;

    history = std::move (latest);
    hint.setVisible (history.done.empty() && history.undone.empty());
    list.updateContent();
    list.scrollToEnsureRowIsOnscreen (getCurrentRow());
    list.repaint();
}

void UndoHistoryPanel::jumpTo (int row)
{
    project.jumpToUndoStep (row);
    refresh();
}

int UndoHistoryPanel::getNumRows()
{
    return 1 + static_cast<int> (history.done.size() + history.undone.size());
}

void UndoHistoryPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    const auto current = getCurrentRow();
    const auto doneCount = static_cast<int> (history.done.size());

    if (row == current)
        g.fillAll (theme::accentSoft);

    juce::String text;

    if (row == 0)
        text = TRANS ("Project start");
    else if (row <= doneCount)
        text = juce::String (row) + ". " + history.done[static_cast<std::size_t> (row - 1)];
    else
        text = juce::String (row) + ". " + history.undone[static_cast<std::size_t> (row - doneCount - 1)];

    g.setColour (row <= current ? theme::textPrimary : theme::textDisabled);
    g.setFont (juce::FontOptions (13.0f, row == current ? juce::Font::bold : juce::Font::plain));
    g.drawText (text, 10, 0, width - 20, height, juce::Justification::centredLeft, true);
}

void UndoHistoryPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    jumpTo (row);
}

void UndoHistoryPanel::timerCallback()
{
    refresh();
}

void UndoHistoryPanel::show (services::ProjectService& projectService)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("History");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (new UndoHistoryPanel (projectService)); // JUCE-API verlangt besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

} // namespace fk::ui
