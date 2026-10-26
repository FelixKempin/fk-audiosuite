#include "fk/ui/FolderHeader.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/services/TrackColours.h"
#include "fk/ui/Icons.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
} // namespace

FolderHeader::FolderHeader (services::TrackService& trackService, services::FolderHandle& folderTrack)
    : tracks (trackService)
    , folder (folderTrack)
    , collapseButton (Icon::expanded, TRANS ("Collapse folder"))
    , muteButton ("M")
    , soloButton ("S")
{
    collapseButton.onClick = [this] { tracks.setFolderCollapsed (folder, ! info.collapsed); };

    nameLabel.setEditable (false, true, false);
    nameLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    nameLabel.setTooltip (TRANS ("Folder. Double-click: rename. Drag: move. Right-click: color, dissolve, delete. Tracks go in via right-click on the track header."));
    nameLabel.onTextChange = [this] { tracks.setFolderName (folder, nameLabel.getText()); };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, theme::mute);
    muteButton.setTooltip (TRANS ("Mute all tracks in the folder"));
    muteButton.onClick = [this] { tracks.setFolderMuted (folder, muteButton.getToggleState()); };

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    soloButton.setTooltip (TRANS ("Solo all tracks in the folder"));
    soloButton.onClick = [this] { tracks.setFolderSolo (folder, soloButton.getToggleState()); };

    for (auto* component :
         std::initializer_list<juce::Component*> { &collapseButton, &nameLabel, &muteButton, &soloButton })
        addAndMakeVisible (component);

    addMouseListener (this, true);
    refresh();
}

FolderHeader::~FolderHeader()
{
    removeMouseListener (this);
}

void FolderHeader::refresh()
{
    info = tracks.getFolderInfo (folder);
    nameLabel.setText (info.name, juce::dontSendNotification);
    muteButton.setToggleState (info.muted, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);
    collapseButton.setIcon (info.collapsed ? Icon::collapsed : Icon::expanded);
    collapseButton.setTooltip (info.collapsed
                                   ? plugins::i18n::format (
                                         TRANS ("Expand folder ({tracks})"),
                                         { { "tracks", plugins::i18n::plural (info.numTracks, "{count} track", "{count} tracks") } })
                                   : TRANS ("Collapse folder"));
    repaint();
}

void FolderHeader::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundElevated);
    g.setColour (info.colour);
    g.fillRect (0, 0, theme::spacingSmall, getHeight());
    drawIcon (g, Icon::folder, juce::Rectangle<float> (34.0f, 8.0f, 18.0f, 18.0f), info.colour);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void FolderHeader::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingSmall);
    area.removeFromLeft (theme::spacingSmall);
    collapseButton.setBounds (area.removeFromLeft (22));
    area.removeFromLeft (26); // Ordnersymbol
    soloButton.setBounds (area.removeFromRight (26).reduced (0, 2));
    area.removeFromRight (theme::spacingSmall);
    muteButton.setBounds (area.removeFromRight (26).reduced (0, 2));
    nameLabel.setBounds (area);
}

bool FolderHeader::isDragHandle (const juce::MouseEvent& event) const
{
    return event.originalComponent == this || event.originalComponent == &nameLabel;
}

void FolderHeader::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && isDragHandle (event))
        showContextMenu();
}

void FolderHeader::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Doppelklick auf die freie Fläche klappt auf und zu; auf dem Namen benennt das Label um.
    if (event.originalComponent == this)
        tracks.setFolderCollapsed (folder, ! info.collapsed);
}

void FolderHeader::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragHandle (event) || event.mods.isPopupMenu())
        return;

    if (! reordering && event.getDistanceFromDragStart() < 6)
        return;

    reordering = true;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);

    if (onReorderDrag)
        onReorderDrag (event.getScreenY());
}

void FolderHeader::mouseUp (const juce::MouseEvent& event)
{
    if (! reordering)
        return;

    reordering = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);

    if (onReorderDrop)
        onReorderDrop (event.getScreenY());
}

void FolderHeader::showContextMenu()
{
    enum
    {
        rename = 1,
        toggle,
        moveUp,
        moveDown,
        dissolve,
        removeAll,
        colourBase = 100
    };

    juce::PopupMenu colours;

    for (std::size_t i = 0; i < services::trackPalette().size(); ++i)
    {
        juce::PopupMenu::Item item (plugins::i18n::format (TRANS ("Color {number}"), { { "number", juce::String (i + 1) } }));
        item.itemID = colourBase + static_cast<int> (i);
        auto swatch = std::make_unique<juce::DrawableRectangle>();
        swatch->setRectangle (juce::Parallelogram<float> (juce::Rectangle<float> (0.0f, 0.0f, 16.0f, 16.0f)));
        swatch->setFill (services::trackPalette()[i]);
        item.image = std::move (swatch);
        colours.addItem (std::move (item));
    }

    juce::PopupMenu menu;
    menu.addItem (rename, TRANS ("Rename"));
    menu.addSubMenu (TRANS ("Color"), colours);
    menu.addItem (toggle, info.collapsed ? TRANS ("Expand") : TRANS ("Collapse"));
    menu.addSeparator();
    menu.addItem (moveUp, TRANS ("Move up"));
    menu.addItem (moveDown, TRANS ("Move down"));
    menu.addSeparator();
    menu.addItem (dissolve, TRANS ("Dissolve folder (tracks stay)"));
    menu.addItem (removeAll, TRANS ("Delete folder with all tracks"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameLabel),
                        [safeThis = juce::Component::SafePointer (this)] (int chosen)
                        {
                            if (safeThis == nullptr || chosen == 0)
                                return;

                            auto& trackService = safeThis->tracks;
                            auto& handle = safeThis->folder;
                            const auto reportError = safeThis->onError;
                            const auto report = [&reportError] (const core::Result<void>& result)
                            {
                                if (! result && reportError)
                                    reportError (services::errorText (result.error()));
                            };

                            if (chosen >= colourBase)
                            {
                                trackService.setFolderColour (handle, services::paletteColour (chosen - colourBase));
                                return;
                            }

                            switch (chosen)
                            {
                                case rename:
                                    safeThis->nameLabel.showEditor();
                                    break;
                                case toggle:
                                    trackService.setFolderCollapsed (handle, ! safeThis->info.collapsed);
                                    break;
                                case moveUp:
                                    report (trackService.moveWithinFolder (handle, -1));
                                    break;
                                case moveDown:
                                    report (trackService.moveWithinFolder (handle, 1));
                                    break;
                                case dissolve:
                                    report (trackService.deleteFolder (handle, true));
                                    break;
                                case removeAll:
                                    report (trackService.deleteFolder (handle, false));
                                    break;
                                default:
                                    break;
                            }
                        });
}

FolderLane::FolderLane (services::TrackService& trackService, services::FolderHandle& folderTrack)
    : tracks (trackService)
    , folder (folderTrack)
{
    setInterceptsMouseClicks (false, false);
    refresh();
}

void FolderLane::setMapping (TimeMapping newMapping)
{
    mapping = newMapping;
    repaint();
}

void FolderLane::refresh()
{
    blocks.clear();
    folderColour = tracks.getFolderInfo (folder).colour;

    for (auto* track : tracks.getTracksInFolder (folder))
    {
        const auto colour = tracks.getTrackInfo (*track).colour;

        for (const auto& clip : tracks.getClips (*track))
            blocks.push_back ({ clip.startSeconds, clip.startSeconds + clip.lengthSeconds, colour });
    }

    repaint();
}

void FolderLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    const auto area = getLocalBounds().reduced (0, 6).toFloat();

    for (const auto& block : blocks)
    {
        const auto x1 = static_cast<float> (mapping.secondsToX (block.start));
        const auto x2 = static_cast<float> (mapping.secondsToX (block.end));

        if (x2 < 0.0f || x1 > static_cast<float> (getWidth()))
            continue;

        g.setColour (block.colour.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (x1, area.getY(), juce::jmax (1.0f, x2 - x1), area.getHeight()));
    }

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

} // namespace fk::ui
