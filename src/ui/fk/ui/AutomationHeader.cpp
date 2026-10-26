#include "fk/ui/AutomationHeader.h"

#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr std::array<services::AutomationMode, 4> modes { services::AutomationMode::read,
                                                          services::AutomationMode::touch,
                                                          services::AutomationMode::latch,
                                                          services::AutomationMode::write };
} // namespace

AutomationHeader::AutomationHeader (services::AutomationService& automationService,
                                    services::TrackHandle& automatedTrack,
                                    int indexOfLane)
    : automation (automationService)
    , track (automatedTrack)
    , laneIndex (indexOfLane)
    , addButton (Icon::plus, TRANS ("Another automation track for this track"))
    , removeButton (Icon::minus, TRANS ("Remove this automation track"))
{
    parameterBox.setTooltip (TRANS ("Parameter whose curve the automation track shows"));
    parameterBox.onChange = [this]
    {
        const auto selected = parameterBox.getSelectedId();
        const auto id = selected > noParameterItemId
                            ? parameters[static_cast<std::size_t> (selected - noParameterItemId - 1)].id
                            : juce::String();
        automation.setLaneParameter (track, laneIndex, id);
    };

    modeBox.setTooltip (TRANS ("Read: play back only. Touch: writes while the control is touched. Latch: writes until stop. Write: writes from play start"));
    int itemId = 1;
    for (const auto mode : modes)
        modeBox.addItem (services::AutomationService::getModeName (mode), itemId++);
    modeBox.onChange = [this]
    {
        if (const auto index = modeBox.getSelectedId() - 1; index >= 0 && index < static_cast<int> (modes.size()))
            automation.setMode (track, modes[static_cast<std::size_t> (index)]);
    };

    addButton.onClick = [this] { automation.addLane (track); };
    removeButton.onClick = [this] { automation.removeLane (track, laneIndex); };

    addAndMakeVisible (parameterBox);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (addButton);
    addAndMakeVisible (removeButton);
    modeBox.setVisible (laneIndex == 0); // Modus gilt für die ganze Spur, einmal reicht
    addMouseListener (this, true);
    automation.addListener (*this);
    refresh();
}

AutomationHeader::~AutomationHeader()
{
    automation.removeListener (*this);
    removeMouseListener (this);
}

void AutomationHeader::refresh()
{
    parameters = automation.getParameters (track);
    const auto shown = automation.getLaneParameter (track, laneIndex);

    parameterBox.clear (juce::dontSendNotification);
    parameterBox.addItem (TRANS ("No parameter"), noParameterItemId);
    int itemId = noParameterItemId + 1;
    int selectedId = noParameterItemId;

    for (const auto& parameter : parameters)
    {
        const auto text = parameter.ownerName.isEmpty() ? parameter.name : parameter.ownerName + ": " + parameter.name;
        parameterBox.addItem (parameter.hasPoints ? text + " *" : text, itemId);

        if (shown && shown->id == parameter.id)
            selectedId = itemId;

        ++itemId;
    }

    parameterBox.setSelectedId (selectedId, juce::dontSendNotification);

    const auto mode = automation.getMode (track);
    for (std::size_t i = 0; i < modes.size(); ++i)
        if (modes[i] == mode)
            modeBox.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
}

void AutomationHeader::automationChanged()
{
    refresh();
}

void AutomationHeader::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void AutomationHeader::resized()
{
    constexpr int rowHeight = 22;
    auto area = getLocalBounds().reduced (theme::spacingMedium, theme::spacingSmall);
    area.removeFromLeft (theme::spacingSmall + theme::spacingLarge); // eingerückt unter dem Spurnamen
    parameterBox.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingSmall);
    auto secondRow = area.removeFromTop (rowHeight);
    removeButton.setBounds (secondRow.removeFromRight (rowHeight));
    secondRow.removeFromRight (theme::spacingSmall);
    addButton.setBounds (secondRow.removeFromRight (rowHeight));
    modeBox.setBounds (secondRow.removeFromLeft (90));
}

void AutomationHeader::mouseDown (const juce::MouseEvent&)
{
    if (onSelected)
        onSelected();
}

} // namespace fk::ui
