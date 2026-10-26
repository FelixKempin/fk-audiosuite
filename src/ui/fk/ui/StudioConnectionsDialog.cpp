#include "fk/ui/StudioConnectionsDialog.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int rowHeight = 28;
constexpr int headingHeight = 26;
constexpr int nameWidth = 140;

} // namespace

StudioConnectionsDialog::StudioConnectionsDialog (services::TrackService& trackService)
    : tracks (trackService)
{
    inputsHeading.setText (TRANS ("Inputs"), juce::dontSendNotification);
    outputsHeading.setText (TRANS ("Outputs"), juce::dontSendNotification);

    for (auto* heading : { &inputsHeading, &outputsHeading })
    {
        heading->setFont (juce::FontOptions (15.0f, juce::Font::bold));
        rowHolder.addAndMakeVisible (*heading);
    }

    for (const auto& port : tracks.getStudioPorts())
    {
        auto row = std::make_unique<Row>();
        row->portId = port.id;
        row->name.setText (port.name, juce::dontSendNotification);
        row->name.setColour (juce::Label::textColourId, theme::textSecondary);
        row->alias.setText (port.alias, juce::dontSendNotification);
        row->alias.setTextToShowWhenEmpty (TRANS ("own name, e.g. Guitar DI"), theme::textSecondary);
        row->alias.setTooltip (TRANS ("Own name for this port. Leave empty for the name from the driver. Applies to all projects."));

        auto* rawRow = row.get();
        row->alias.onReturnKey = [this, rawRow] { applyAlias (*rawRow); };
        row->alias.onFocusLost = [this, rawRow] { applyAlias (*rawRow); };

        rowHolder.addAndMakeVisible (row->name);
        rowHolder.addAndMakeVisible (row->alias);
        (port.isInput ? inputRows : outputRows).push_back (std::move (row));
    }

    emptyHint.setText (TRANS ("No audio device active. Choose an interface under Studio > Audio and MIDI Settings, then its inputs and outputs appear here."),
                       juce::dontSendNotification);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, theme::textSecondary);
    addChildComponent (emptyHint);
    emptyHint.setVisible (inputRows.empty() && outputRows.empty());

    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setSize (460, 480);
}

void StudioConnectionsDialog::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);
}

void StudioConnectionsDialog::resized()
{
    auto area = getLocalBounds().reduced (12);
    emptyHint.setBounds (area);
    viewport.setBounds (area);

    const int width = viewport.getMaximumVisibleWidth();
    int y = 0;

    const auto layoutSection = [&] (juce::Label& heading, std::vector<std::unique_ptr<Row>>& rows)
    {
        heading.setBounds (0, y, width, headingHeight);
        y += headingHeight;

        for (auto& row : rows)
        {
            row->name.setBounds (0, y + 2, nameWidth, rowHeight - 4);
            row->alias.setBounds (nameWidth + 8, y + 2, width - nameWidth - 8, rowHeight - 4);
            y += rowHeight;
        }

        y += 8;
    };

    layoutSection (inputsHeading, inputRows);
    layoutSection (outputsHeading, outputRows);
    rowHolder.setSize (width, y);
}

void StudioConnectionsDialog::applyAlias (Row& row)
{
    if (const auto result = tracks.setPortAlias (row.portId, row.alias.getText()); ! result)
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, TRANS ("Studio Connections"), services::errorText (result.error()));
}

void StudioConnectionsDialog::show (services::TrackService& trackService)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Studio Connections");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (new StudioConnectionsDialog (trackService)); // JUCE-API verlangt besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

} // namespace fk::ui
