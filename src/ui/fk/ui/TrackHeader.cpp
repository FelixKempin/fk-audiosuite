#include "fk/ui/TrackHeader.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/services/TrackColours.h"
#include "fk/ui/Icons.h"
#include "fk/ui/Theme.h"

#include <algorithm>

namespace fk::ui
{

namespace
{
constexpr int noInputItemId = 1; // ComboBox-IDs müssen > 0 sein; 1 = "kein Eingang", Eingänge ab 2
} // namespace

TrackHeader::TrackHeader (services::TrackService& trackService,
                          services::MixerService& mixerService,
                          services::TrackHandle& audioTrack)
    : tracks (trackService)
    , mixer (mixerService)
    , track (audioTrack)
    , armButton ("R")
    , muteButton ("M")
    , soloButton ("S")
    , automationButton (Icon::automation, TRANS ("Automation track on/off"))
    , instrumentButton (Icon::openPlugin,
                        TRANS ("Open instrument: plugin window of the track instrument"))
    , bakeButton (Icon::bake, TRANS ("Bake: render the track to audio and save CPU"))
{
    nameLabel.setEditable (false, true, false);
    nameLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    nameLabel.onTextChange = [this] { tracks.setTrackName (track, nameLabel.getText()); };

    inputBox.onChange = [this]
    {
        const auto selectedId = inputBox.getSelectedId();
        const auto inputId = selectedId > noInputItemId
                                 ? inputs[static_cast<std::size_t> (selectedId - noInputItemId - 1)].id
                                 : juce::String();

        if (const auto result = tracks.setAudioInput (track, inputId); ! result)
            juce::Logger::writeToLog ("Eingang konnte nicht gesetzt werden: " + services::errorText (result.error()));
    };

    armButton.setClickingTogglesState (true);
    armButton.setColour (juce::TextButton::buttonOnColourId, theme::record);
    armButton.onClick = [this] { tracks.setArmed (track, armButton.getToggleState()); };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, theme::mute);
    muteButton.onClick = [this] { tracks.setMuted (track, muteButton.getToggleState()); };

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, theme::solo);
    soloButton.onClick = [this] { tracks.setSolo (track, soloButton.getToggleState()); };

    automationButton.setClickingTogglesState (true);
    automationButton.setOnColour (theme::accent);
    automationButton.onClick = [this]
    {
        if (onAutomationToggled)
            onAutomationToggled (automationButton.getToggleState());
    };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (inputBox);
    addAndMakeVisible (armButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);
    addAndMakeVisible (automationButton);
    instrumentButton.onClick = [this]
    {
        if (onOpenInstrument)
            onOpenInstrument();
    };
    addChildComponent (instrumentButton);
    bakeButton.setOnColour (theme::accent);
    bakeButton.onClick = [this] { showBakeMenu(); };
    addChildComponent (bakeButton);
    addAndMakeVisible (meter);

    // Auch Klicks auf Buttons und Felder im Kopf wählen die Spur aus (sonst nur die Lücken dazwischen).
    addMouseListener (this, true);

    refresh();
}

TrackHeader::~TrackHeader()
{
    removeMouseListener (this);
}

void TrackHeader::setSelected (bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void TrackHeader::setAutomationShown (bool shown)
{
    automationButton.setToggleState (shown, juce::dontSendNotification);
}

void TrackHeader::refresh()
{
    const auto info = tracks.getTrackInfo (track);

    nameLabel.setText (info.name, juce::dontSendNotification);
    nameLabel.setTooltip (TRANS ("Double-click: rename. Drag: move track. Right-click: color, duplicate, hide, delete"));
    colour = info.colour;
    armButton.setToggleState (info.armed, juce::dontSendNotification);
    armButton.setVisible (info.kind == services::TrackKind::audio || services::playsMidi (info.kind));
    muteButton.setToggleState (info.muted, juce::dontSendNotification);
    soloButton.setToggleState (info.solo, juce::dontSendNotification);
    instrumentButton.setVisible (info.isInstrument);
    instrumentButton.setTooltip (plugins::i18n::format (TRANS ("Open instrument: {name}"), { { "name", info.instrumentName } }));

    // Bake: an, solange gebacken; in einer gebackenen Gruppe gesperrt und erklärt.
    lastInfo = info;
    bakeButton.setVisible (info.kind != services::TrackKind::fx && info.kind != services::TrackKind::midi && ! info.inRack);
    bakeButton.setToggleState (info.baked, juce::dontSendNotification);
    bakeButton.setEnabled (info.bakedIntoGroup.isEmpty());
    bakeButton.setTooltip (
        info.bakedIntoGroup.isNotEmpty()
            ? plugins::i18n::format (TRANS ("Baked into group \"{group}\": unbake it there to edit this track"),
                                     { { "group", info.bakedIntoGroup } })
        : info.baked ? TRANS ("Baked: instrument/inserts run as audio. Click: unbake")
                     : TRANS ("Bake: render the track to audio and save CPU"));
    rebuildInputChoices (info);
    resized();
}

void TrackHeader::paint (juce::Graphics& g)
{
    g.fillAll (selected ? theme::backgroundElevated : theme::backgroundPanel);

    // Farbbalken links wie in Cubase: Spurfarbe auf einen Blick.
    g.setColour (colour);
    g.fillRect (0, 0, theme::spacingSmall, getHeight());

    // Gebacken oder in gebackener Gruppe: Schloss oben rechts, wie in den Insert-Slots.
    if (lastInfo.baked || lastInfo.bakedIntoGroup.isNotEmpty())
        drawIcon (g,
                  Icon::locked,
                  juce::Rectangle<float> (static_cast<float> (getWidth() - 34), 6.0f, 14.0f, 14.0f),
                  theme::accent);

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TrackHeader::showBakeMenu()
{
    bakeButton.setToggleState (lastInfo.baked, juce::dontSendNotification);
    juce::PopupMenu menu;

    const auto addItem = [&menu] (BakeAction action, const juce::String& text, bool enabled)
    {
        juce::PopupMenu::Item item (text);
        item.itemID = static_cast<int> (action) + 1;
        item.isEnabled = enabled;
        item.image = makeIconDrawable (action == BakeAction::unbake ? Icon::locked : Icon::bake, theme::textPrimary);
        menu.addItem (std::move (item));
    };

    if (lastInfo.kind == services::TrackKind::instrument)
    {
        addItem (BakeAction::bakeTrack,
                 TRANS ("Bake Track: instrument to audio (inserts stay live)"),
                 true);
        addItem (BakeAction::bakeInserts, TRANS ("Bake Inserts: instrument and all inserts"), true);
    }
    else if (lastInfo.kind == services::TrackKind::group)
    {
        addItem (
            BakeAction::bakeInserts, TRANS ("Bake group (with all feeding tracks)"), true);
    }
    else
    {
        addItem (BakeAction::bakeInserts, TRANS ("Bake Inserts: clips through all inserts"), true);
    }

    menu.addSeparator();
    addItem (BakeAction::unbake, TRANS ("Unbake: live and editable again"), lastInfo.baked);
    menu.addSeparator();
    menu.addItem (
        -1, TRANS ("Choose individual insert slots: in the Inspector (mark at the slot)"), false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bakeButton),
                        [safeThis = juce::Component::SafePointer<TrackHeader> (this)] (int result)
                        {
                            if (safeThis != nullptr && result > 0 && safeThis->onBakeRequested)
                                safeThis->onBakeRequested (static_cast<BakeAction> (result - 1));
                        });
}

void TrackHeader::resized()
{
    constexpr int rowHeight = 22;
    constexpr int smallButtonWidth = 26;

    auto area = getLocalBounds().reduced (theme::spacingMedium, theme::spacingSmall);
    area.removeFromLeft (theme::spacingSmall);

    area.removeFromRight (12); // Platz für den Pegel
    nameLabel.setBounds (area.removeFromTop (rowHeight));
    area.removeFromTop (theme::spacingSmall);

    auto buttonRow = area.removeFromTop (rowHeight);
    armButton.setBounds (buttonRow.removeFromLeft (smallButtonWidth));
    buttonRow.removeFromLeft (theme::spacingSmall);
    muteButton.setBounds (buttonRow.removeFromLeft (smallButtonWidth));
    buttonRow.removeFromLeft (theme::spacingSmall);
    soloButton.setBounds (buttonRow.removeFromLeft (smallButtonWidth));
    buttonRow.removeFromLeft (theme::spacingSmall);
    automationButton.setBounds (buttonRow.removeFromLeft (smallButtonWidth));
    buttonRow.removeFromLeft (theme::spacingSmall);

    for (auto* button : { &instrumentButton, &bakeButton })
    {
        if (! button->isVisible())
            continue;

        button->setBounds (buttonRow.removeFromLeft (smallButtonWidth));
        buttonRow.removeFromLeft (theme::spacingSmall);
    }

    buttonRow.removeFromLeft (theme::spacingSmall);
    inputBox.setBounds (buttonRow);
    // Kompakte Spuren oder volle Knopfreihe: Eingangswahl bleibt im Inspector erreichbar, hier
    // fehlt der Platz (unter 70 Pixeln wäre nur noch der Pfeil zu sehen).
    constexpr int minInputBoxWidth = 70;
    inputBox.setVisible (getHeight() >= 64 && buttonRow.getWidth() >= minInputBoxWidth &&
                         inputs.size() + 1 == static_cast<std::size_t> (inputBox.getNumItems()));

    // Pegel am rechten Rand über die ganze Höhe.
    meter.setBounds (getLocalBounds().removeFromRight (10).reduced (0, theme::spacingSmall));
}

void TrackHeader::mouseDown (const juce::MouseEvent& event)
{
    if (onSelected)
        onSelected();

    if (event.mods.isPopupMenu() && isDragHandle (event))
        showContextMenu();
}

bool TrackHeader::isDragHandle (const juce::MouseEvent& event) const
{
    // Ziehen und Kontextmenü nur am Kopf selbst und am Namen; Buttons und Eingangswahl behalten ihre Maus.
    return event.originalComponent == this || event.originalComponent == &nameLabel;
}

void TrackHeader::mouseDrag (const juce::MouseEvent& event)
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

void TrackHeader::mouseUp (const juce::MouseEvent& event)
{
    if (! reordering)
        return;

    reordering = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);

    if (onReorderDrop)
        onReorderDrop (event.getScreenY());
}

void TrackHeader::showContextMenu()
{
    enum
    {
        rename = 1,
        duplicate,
        moveUp,
        moveDown,
        hide,
        remove,
        newFolder,
        outOfFolder,
        colourBase = 100,
        folderBase = 200
    };

    const auto folders = tracks.getFolders();
    auto* parentFolder = tracks.getParentFolder (track);

    juce::PopupMenu colours;

    for (std::size_t i = 0; i < services::trackPalette().size(); ++i)
    {
        juce::PopupMenu::Item item (plugins::i18n::format (TRANS ("Color {number}"), { { "number", juce::String (i + 1) } }));
        item.itemID = colourBase + static_cast<int> (i);
        auto swatch = std::make_unique<juce::DrawableRectangle>();
        swatch->setRectangle (juce::Parallelogram<float> (juce::Rectangle<float> (0.0f, 0.0f, 16.0f, 16.0f)));
        swatch->setFill (services::trackPalette()[i]);
        item.image = std::move (swatch);
        item.isTicked = services::trackPalette()[i] == colour;
        colours.addItem (std::move (item));
    }

    juce::PopupMenu menu;
    menu.addItem (rename, TRANS ("Rename"));
    menu.addSubMenu (TRANS ("Color"), colours);
    menu.addItem (duplicate, TRANS ("Duplicate"));
    menu.addSeparator();
    menu.addItem (moveUp, TRANS ("Move up"));
    menu.addItem (moveDown, TRANS ("Move down"));

    juce::PopupMenu folderMenu;

    for (std::size_t i = 0; i < folders.size(); ++i)
        folderMenu.addItem (folderBase + static_cast<int> (i),
                            tracks.getFolderInfo (*folders[i]).name,
                            folders[i] != parentFolder,
                            folders[i] == parentFolder);

    if (! folders.empty())
        folderMenu.addSeparator();

    folderMenu.addItem (newFolder, TRANS ("New folder with this track"));
    menu.addSubMenu (TRANS ("Move to folder"), folderMenu);
    menu.addItem (outOfFolder, TRANS ("Take out of folder"), parentFolder != nullptr);
    menu.addItem (hide, TRANS ("Hide (toolbar: eye to show again)"));
    menu.addSeparator();
    menu.addItem (remove, TRANS ("Delete track"));

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&nameLabel),
        [safeThis = juce::Component::SafePointer (this), folders] (int chosen)
        {
            // Jede Aktion kann diesen Kopf zerstören (Neuaufbau); danach nichts mehr an ihm anfassen.
            if (safeThis == nullptr || chosen == 0)
                return;

            auto& trackService = safeThis->tracks;
            auto& handle = safeThis->track;
            const auto reportError = safeThis->onError;

            const auto report = [&reportError] (const core::Error& error)
            {
                if (reportError)
                    reportError (services::errorText (error));
            };

            if (chosen >= folderBase)
            {
                const auto index = static_cast<std::size_t> (chosen - folderBase);

                if (index < folders.size())
                    if (const auto result = trackService.moveIntoFolder (handle, folders[index]); ! result)
                        report (result.error());

                return;
            }

            if (chosen >= colourBase)
            {
                trackService.setTrackColour (handle, services::paletteColour (chosen - colourBase));
                return;
            }

            switch (chosen)
            {
                case rename:
                    safeThis->nameLabel.showEditor();
                    break;
                case duplicate:
                    if (const auto copy = trackService.duplicateTrack (handle); ! copy)
                        report (copy.error());
                    break;
                case moveUp:
                    if (const auto result = trackService.moveWithinFolder (handle, -1); ! result)
                        report (result.error());
                    break;
                case moveDown:
                    if (const auto result = trackService.moveWithinFolder (handle, 1); ! result)
                        report (result.error());
                    break;
                case newFolder:
                    if (const auto folder = trackService.createFolderTrack (TRANS ("Folder")); ! folder)
                        report (folder.error());
                    else if (const auto before = trackService.moveTrackBefore (**folder, &handle); ! before)
                        report (before.error());
                    else if (const auto into = trackService.moveIntoFolder (handle, *folder); ! into)
                        report (into.error());
                    break;
                case outOfFolder:
                    if (const auto result = trackService.moveIntoFolder (handle, nullptr); ! result)
                        report (result.error());
                    break;
                case hide:
                    trackService.setHidden (handle, true);
                    break;
                case remove:
                    trackService.deleteTrack (handle);
                    break;
                default:
                    break;
            }
        });
}

void TrackHeader::updateMeter()
{
    const auto levels = mixer.getLevels (track);
    meter.setLevels (levels.leftDb, levels.rightDb);
}

void TrackHeader::rebuildInputChoices (const services::TrackInfo& info)
{
    inputBox.setVisible (info.kind == services::TrackKind::audio || services::playsMidi (info.kind));

    // Instrumentenspuren bieten MIDI-Eingänge an, Audiospuren die zur Kanalzahl passenden Interface-Eingänge.
    inputs = services::playsMidi (info.kind) ? tracks.getAvailableMidiInputs()
                                                          : tracks.getAvailableAudioInputs (track);

    inputBox.clear (juce::dontSendNotification);
    inputBox.addItem (TRANS ("No input"), noInputItemId);

    int itemId = noInputItemId + 1;
    int selectedItemId = noInputItemId;

    for (const auto& input : inputs)
    {
        inputBox.addItem (input.name, itemId);

        if (input.id == info.inputId)
            selectedItemId = itemId;

        ++itemId;
    }

    inputBox.setSelectedId (selectedItemId, juce::dontSendNotification);
}

} // namespace fk::ui
