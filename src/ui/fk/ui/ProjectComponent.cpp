#include "fk/ui/ProjectComponent.h"

#include "fk/core/BuildInfo.h"
#include "fk/plugins/i18n/LanguageSetting.h"
#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/AudioSettingsComponent.h"
#include "fk/ui/ContentManagerDialog.h"
#include "fk/ui/ExportDialog.h"
#include "fk/ui/PluginMenu.h"
#include "fk/ui/ProjectSettingsDialog.h"
#include "fk/ui/ShortcutEditor.h"
#include "fk/ui/StudioConnectionsDialog.h"
#include "fk/ui/Theme.h"
#include "fk/ui/UndoHistoryPanel.h"

#include <array>
#include <set>

namespace fk::ui
{

namespace
{
/// Markenhinweise für das About-Fenster. Die ASIO-Zeile verlangt der Steinberg-ASIO-Lizenzvertrag
/// (§ 3) in jedem Build mit ASIO; Wortlaut aus dem Vertrag, deshalb nicht übersetzt.
juce::String trademarkNotice()
{
    juce::String text ("VST is a registered trademark of Steinberg Media Technologies GmbH.");
#if JUCE_ASIO
    text << "\nASIO is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.";
#endif
    return text;
}

constexpr auto editFileWildcard = "*.tracktionedit";
constexpr int toolRadioGroup = 1;

/// Gemeinsame Sprachwahl aller FK-Programme (~/.config/FKAudioSuite/common.json, ADR 0010).
plugins::i18n::LanguageSetting languageSetting()
{
    return plugins::i18n::LanguageSetting { plugins::i18n::SharedSettings (
        plugins::i18n::SharedSettings::defaultFile()) };
}

/// Website-Pfad in der Oberflächensprache: "pricing/" wird "/de/pricing/" bzw. "/en/pricing/".
juce::String localisedPath (const juce::String& path)
{
    return "/" + plugins::i18n::toCode (languageSetting().resolve()) + "/" + path;
}

/// Menüeintrag mit Symbol (Regel: jede Funktion sichtbar mit Icon).
void addIconItem (
    juce::PopupMenu& menu, int id, Icon icon, const juce::String& text, bool enabled = true, bool ticked = false)
{
    juce::PopupMenu::Item item (text);
    item.itemID = id;
    item.isEnabled = enabled;
    item.isTicked = ticked;
    item.image = makeIconDrawable (icon, theme::textPrimary);
    menu.addItem (std::move (item));
}

juce::String utf8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

// Menüs und ihre Einträge. Instrumente bekommen dynamische IDs ab instrumentMenuBase.
enum MenuIndex
{
    fileMenu = 0,
    editMenu,
    trackMenu,
    viewMenu,
    transportMenu,
    studioMenu,
    helpMenu
};

enum MenuItem
{
    itemNew = 1,
    itemOpen,
    itemSave,
    itemSaveAs,
    itemRestoreBackup,
    itemProjectSettings,
    itemSaveTemplate,
    itemOpenTemplateFolder,
    itemEmptyProject,
    itemExportDiagnostics,
    itemPlay,
    itemStop,
    itemRecord,
    itemReturnToStart,
    itemRewind,
    itemForward,
    itemJumpLeft,
    itemJumpRight,
    itemSetLeft,
    itemSetRight,
    itemPunch,
    itemPreviousMarker,
    itemNextMarker,
    itemAddMarker,
    itemDuplicateTrack,
    itemDeleteTrack,
    itemHideTrack,
    itemShowAllTracks,
    itemOpenLogFolder,
    itemExport,
    itemImportAudio,
    itemQuit,
    itemUndo,
    itemRedo,
    itemHistory,
    itemExternalMidi,
    itemSamplerFromFile,
    itemSamplerKit,
    itemShortcuts,
    itemZoomProject,
    itemZoomSelection,
    itemZoomIn,
    itemZoomOut,
    itemOverview,
    itemCut,
    itemCopy,
    itemPaste,
    itemDuplicate,
    itemGlue,
    itemDeleteClips,
    itemSelectAllClips,
    itemToggleClipMute,
    itemRangeRemove,
    itemRangeRemoveClose,
    itemRangeSilence,
    itemRangeSplit,
    itemRangeLoop,
    itemNormalize,
    itemReverse,
    itemInvertPhase,
    itemNudgeLeft,
    itemNudgeRight,
    itemMidiLearn,
    itemMidiMappings,
    itemAudioMono,
    itemAudioStereo,
    itemGroup,
    itemFx,
    itemFolder,
    itemMidiClip,
    itemAutomationRead,
    itemAutomationWrite,
    itemInspector,
    itemLowerZone,
    itemMixer,
    itemKeyboard,
    itemKeyEditor,
    itemMediaBrowser,
    itemSampleEditor,
    itemDrumEditor,
    itemListEditor,
    itemInstrumentBrowser,
    itemAudioSettings,
    itemStudioConnections,
    itemContentManager,
    itemCheckUpdates,
    itemAutoUpdateCheck,
    itemPluginManager,
    itemAbout,
    itemBuyPro,
    itemActivatePro,
    itemDownloadPage,
    instrumentMenuBase = 1000,
    rackMenuBase = 1500,       ///< Rack-Instrument anlegen, Index = ID - rackMenuBase in getAvailableInstruments
    recentMenuBase = 2000,     ///< Einträge "Zuletzt geöffnet", Index = ID - recentMenuBase
    templateMenuBase = 3000,   ///< Vorlagen für "Neu", Index = ID - templateMenuBase
    visibilityMenuBase = 4000, ///< Spuren ein/aus, Index = Position unter allen Spuren
    languageMenuBase = 9000,   ///< Oberflächensprache, Index = Position in i18n::allLanguages
    contentMenuBase = 10000    ///< Instrument aus Bibliothek, Index = ID - contentMenuBase in getInstruments
};
} // namespace

ProjectComponent::ProjectComponent (UiServices uiServices)
    : services (uiServices)
    , newButton (Icon::newProject, TRANS ("New project: empty or from a template"), TRANS ("New"))
    , openButton (Icon::open, TRANS ("Open project"), TRANS ("Open"))
    , saveButton (Icon::save, TRANS ("Save project (Ctrl+S)"), TRANS ("Save"))
    , saveAsButton (Icon::saveAs,
                    TRANS ("Save as: save the project to a new folder, audio is copied along (Ctrl+Shift+S)"))
    , recentButton (Icon::recent, TRANS ("Recent projects and restore backups"))
    , settingsButton (
          Icon::settings,
          TRANS ("Project settings: sample rate, record format, bit depth, auto-save (Shift+S)"))
    , exportButton (
          Icon::exportFile, TRANS ("Export: mixdown (WAV, FLAC, AIFF, OGG, MP3), stems or MIDI file"), TRANS ("Export"))
    , addTrackButton (Icon::addAudioTrack, TRANS ("Add audio track (mono or stereo)"), TRANS ("Audio"))
    , addInstrumentButton (Icon::addInstrumentTrack, TRANS ("Add instrument track"), TRANS ("Instrument"))
    , addGroupButton (Icon::addGroupTrack, TRANS ("Add group track (target for other tracks)"), TRANS ("Group"))
    , addFxButton (Icon::addFxTrack, TRANS ("Add FX track (reverb, delay via sends)"), TRANS ("FX"))
    , addFolderButton (
          Icon::folder,
          TRANS ("Add folder track: drag or right-click tracks into it, collapse, mute together"),
          TRANS ("Folder"))
    , undoButton (Icon::undo, TRANS ("Undo (Ctrl+Z)"))
    , redoButton (Icon::redo, TRANS ("Redo (Ctrl+Y)"))
    , historyButton (Icon::history, TRANS ("History: all editing steps, click to jump to a step"))
    , zoomMenuButton (Icon::zoomFit,
                      TRANS ("Zoom: whole project (Shift+F), selection (Alt+S), bars and custom zoom presets"))
    , overviewButton (Icon::overview, TRANS ("Overview bar on/off: whole project with the visible section"))
    , cutButton (Icon::cut, TRANS ("Cut selected clips (Ctrl+X)"))
    , copyButton (Icon::copy, TRANS ("Copy selected clips (Ctrl+C)"))
    , pasteButton (Icon::paste, TRANS ("Paste clips at the playhead on the selected track (Ctrl+V)"))
    , duplicateButton (Icon::duplicate, TRANS ("Duplicate selected clips right after themselves (Ctrl+D)"))
    , glueButton (Icon::glue, TRANS ("Glue selected clips of a track into one (Ctrl+J)"))
    , deleteClipsButton (Icon::trash, TRANS ("Delete selected clips (Del)"))
    , midiLearnButton (Icon::midiLearn,
                       TRANS ("MIDI Learn: switch on, then touch a control in the DAW and move the hardware control"))
    , audioSettingsButton (Icon::audioSettings, TRANS ("Audio settings: device, buffer, test tone"))
    , pluginManagerButton (Icon::plugins, TRANS ("Plugin manager: scan and manage plugins"))
    , selectToolButton (Icon::toolSelect, TRANS ("Select tool: drag clips (1)"))
    , drawToolButton (Icon::toolDraw,
                      TRANS ("Pencil: draw a MIDI clip on an instrument track, click = one bar (8)"))
    , splitToolButton (Icon::toolSplit, TRANS ("Split tool: cut a clip at the click (3)"))
    , eraseToolButton (Icon::toolErase, TRANS ("Eraser: remove a clip (5)"))
    , glueToolButton (Icon::toolGlue, TRANS ("Glue: join a clip with the one following on the right (4)"))
    , zoomToolButton (Icon::toolZoom, TRANS ("Zoom: click zooms in, Alt+click zooms out (6)"))
    , muteToolButton (Icon::toolMute, TRANS ("Mute tool: mute a clip and make it audible again (7)"))
    , rangeToolButton (
          Icon::toolRange,
          TRANS ("Range tool: drag a time range across tracks; Del deletes, Shift+Del closes the gap, right-click: insert silence, split, loop (2)"))
    , nudgeLeftButton (Icon::nudgeLeft, TRANS ("Move selected clips one grid step left (Ctrl+Left)"))
    , nudgeRightButton (Icon::nudgeRight, TRANS ("Move selected clips one grid step right (Ctrl+Right)"))
    , snapButton (Icon::snap, TRANS ("Snap: lock to the grid"))
    , tracksSmallerButton (Icon::tracksSmaller, TRANS ("Tracks smaller"))
    , tracksLargerButton (Icon::tracksLarger, TRANS ("Tracks larger"))
    , trackVisibilityButton (Icon::eye, TRANS ("Show and hide tracks (hidden ones keep playing)"))
    , inspectorButton (Icon::inspector, TRANS ("Inspector on/off: details of the selected track (F2)"))
    , lowerZoneButton (Icon::lowerZone,
                       TRANS ("Lower zone on/off: MixConsole (F3), Keyboard (F4), Key Editor (F5), Media (F6), Sample Editor (F7), Drum Editor (F8), List Editor (F9), Instruments (F10)"))
    , inspector (uiServices.tracks,
                 uiServices.mixer,
                 uiServices.plugins,
                 uiServices.sends,
                 uiServices.racks,
                 uiServices.externalMidi)
    , arrangeView ({ uiServices.tracks,
                     uiServices.clips,
                     uiServices.midiClips,
                     uiServices.transport,
                     uiServices.timeline,
                     uiServices.waveforms,
                     uiServices.mixer,
                     uiServices.automation,
                     uiServices.signature,
                     uiServices.markers })
    , mixConsole (uiServices.project, uiServices.tracks, uiServices.mixer, uiServices.plugins, uiServices.sends)
    , virtualKeyboard (uiServices.midi)
    , keyEditor (KeyEditor::Services { uiServices.midiClips,
                                       uiServices.midiControllers,
                                       uiServices.notePreview,
                                       uiServices.transport,
                                       uiServices.timeline,
                                       uiServices.signature,
                                       &stepInput,
                                       &uiServices.expressionMaps })
    , mediaBrowser (uiServices.preview, uiServices.waveforms)
    , sampleEditor (uiServices.clips, uiServices.tracks, uiServices.waveforms)
    , drumEditor (DrumEditor::Services { uiServices.midiClips,
                                         uiServices.notePreview,
                                         uiServices.transport,
                                         uiServices.timeline,
                                         uiServices.drumMaps,
                                         uiServices.tracks,
                                         uiServices.sampler })
    , listEditor (uiServices.midiClips, uiServices.timeline)
    , instrumentBrowser (uiServices.library)
    , lowerZone (
          mixConsole, virtualKeyboard, keyEditor, mediaBrowser, sampleEditor, drumEditor, listEditor, instrumentBrowser)
    , transportBar ({ uiServices.transport,
                      uiServices.timeline,
                      uiServices.automation,
                      uiServices.markers,
                      uiServices.projectSettings })
{
    newButton.onClick = [this] { showNewMenu(); };
    settingsButton.onClick = [this] { showProjectSettings(); };
    openButton.onClick = [this] { openProject(); };
    saveButton.onClick = [this] { saveProject(); };
    saveAsButton.onClick = [this] { saveProjectAs(); };
    recentButton.onClick = [this] { showRecentMenu(); };
    exportButton.onClick = [this] { exportMixdown(); };
    addTrackButton.onClick = [this] { addTrack(); };
    addInstrumentButton.onClick = [this] { addInstrumentTrack(); };
    addGroupButton.onClick = [this] { addGroupTrack(); };
    addFxButton.onClick = [this] { addFxTrack(); };
    addFolderButton.onClick = [this] { addFolderTrack(); };
    undoButton.onClick = [this] { services.project.undo(); };
    redoButton.onClick = [this] { services.project.redo(); };
    historyButton.onClick = [this] { UndoHistoryPanel::show (services.project); };
    zoomMenuButton.onClick = [this] { showZoomMenu(); };
    overviewButton.setClickingTogglesState (false);
    overviewButton.onClick = [this] { toggleOverview(); };
    cutButton.onClick = [this] { performClipAction (ClipAction::cut); };
    copyButton.onClick = [this] { performClipAction (ClipAction::copy); };
    pasteButton.onClick = [this] { performClipAction (ClipAction::paste); };
    duplicateButton.onClick = [this] { performClipAction (ClipAction::duplicate); };
    glueButton.onClick = [this] { performClipAction (ClipAction::merge); };
    deleteClipsButton.onClick = [this] { performClipAction (ClipAction::remove); };
    midiLearnButton.setClickingTogglesState (true);
    midiLearnButton.setOnColour (theme::record);
    midiLearnButton.onClick = [this] { services.midiLearn.setLearnActive (midiLearnButton.getToggleState()); };
    midiLearnHint.setFont (juce::FontOptions (12.0f));
    midiLearnHint.setColour (juce::Label::textColourId, theme::record);
    services.midiLearn.addListener (*this);
    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    pluginManagerButton.onClick = [this] { showPluginManager(); };

    // Werkzeuge als Radiogruppe: genau eines ist aktiv.
    for (auto* button : { &selectToolButton,
                          &drawToolButton,
                          &splitToolButton,
                          &eraseToolButton,
                          &glueToolButton,
                          &zoomToolButton,
                          &muteToolButton,
                          &rangeToolButton })
    {
        button->setRadioGroupId (toolRadioGroup);
        button->setClickingTogglesState (true);
    }

    selectToolButton.onClick = [this] { selectTool (Tool::select); };
    drawToolButton.onClick = [this] { selectTool (Tool::draw); };
    splitToolButton.onClick = [this] { selectTool (Tool::split); };
    eraseToolButton.onClick = [this] { selectTool (Tool::erase); };
    glueToolButton.onClick = [this] { selectTool (Tool::glue); };
    zoomToolButton.onClick = [this] { selectTool (Tool::zoom); };
    muteToolButton.onClick = [this] { selectTool (Tool::mute); };
    rangeToolButton.onClick = [this] { selectTool (Tool::range); };
    nudgeLeftButton.onClick = [this] { arrangeView.nudgeSelectedClips (-1); };
    nudgeRightButton.onClick = [this] { arrangeView.nudgeSelectedClips (1); };
    selectToolButton.setToggleState (true, juce::dontSendNotification);

    // Raster wie in Cubase: Takt, Zählzeit, alle Notenwerte (gerade, punktiert, triolisch), Sekunden, Samples.
    using services::GridResolution;
    gridChoices = { GridResolution::bar(), GridResolution::beat() };

    for (const auto& value : services::NoteValue::all())
        gridChoices.push_back (GridResolution::of (value));

    gridChoices.push_back (GridResolution::seconds());
    gridChoices.push_back (GridResolution::samples());

    for (std::size_t i = 0; i < gridChoices.size(); ++i)
    {
        if (i == 2 || gridChoices[i].kind == GridResolution::Kind::seconds)
            gridBox.addSeparator();

        gridBox.addItem (gridChoices[i].shortName(), static_cast<int> (i) + 1);
    }

    gridBox.setSelectedId (2, juce::dontSendNotification); // Zählzeit
    gridBox.setTooltip (TRANS ("Grid for snap, nudge and display: bar, beat, note values (. = dotted, T = triplet), seconds, samples"));
    gridBox.onChange = [this]
    {
        const auto index = static_cast<std::size_t> (gridBox.getSelectedId() - 1);

        if (index < gridChoices.size())
            arrangeView.setGridResolution (gridChoices[index]);
    };

    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (true, juce::dontSendNotification);
    snapButton.onClick = [this] { arrangeView.setSnapEnabled (snapButton.getToggleState()); };
    tracksSmallerButton.onClick = [this] { arrangeView.setTrackHeight (arrangeView.getTrackHeight() - 16); };
    tracksLargerButton.onClick = [this] { arrangeView.setTrackHeight (arrangeView.getTrackHeight() + 16); };
    trackVisibilityButton.onClick = [this] { showTrackVisibilityMenu(); };

    inspectorButton.setClickingTogglesState (true);
    inspectorButton.setToggleState (true, juce::dontSendNotification);
    inspectorButton.onClick = [this]
    {
        inspectorViewport.setVisible (inspectorButton.getToggleState());
        resized();
    };

    lowerZoneButton.setClickingTogglesState (true);
    lowerZoneButton.setToggleState (true, juce::dontSendNotification);
    lowerZoneButton.onClick = [this]
    {
        lowerZone.setVisible (lowerZoneButton.getToggleState());
        resized();
    };

    projectNameLabel.setJustificationType (juce::Justification::centredRight);
    projectNameLabel.setColour (juce::Label::textColourId, theme::textSecondary);

    // Spurauswahl im Arrangement füllt den Inspector; Plugin-Fenster verwaltet die MixConsole.
    arrangeView.onTrackSelected = [this] (services::TrackHandle* track) { inspector.setTrack (track); };
    arrangeView.onOpenMidiClip = [this] (const juce::String& clipId) { openMidiClip (clipId); };
    arrangeView.onOpenAudioClip = [this] (services::TrackHandle* track, const juce::String& clipId)
    {
        if (track == nullptr)
            return;

        sampleEditor.setClip (services.tracks.getTrackId (*track), clipId);
        showLowerZoneTab (4);
    };
    sampleEditor.onError = [this] (const juce::String& message) { showError (message); };
    arrangeView.onClipSelectionChanged = [this] { updateToolbar(); };
    arrangeView.createInstrumentTrack = [this] (const juce::String& name) -> services::TrackHandle*
    {
        // MIDI-Datei ohne passende Spur: Instrumentenspur mit FK Synth (immer vorhanden, klingt ohne Dateien).
        for (const auto& instrument : services.plugins.getAvailableInstruments())
            if (instrument.name == "FK Synth")
                if (const auto created = services.tracks.createInstrumentTrack (name, instrument.id))
                    return *created;

        showError (TRANS ("No instrument track: FK Synth not found (plugin manager)."));
        return nullptr;
    };
    arrangeView.onError = [this] (const juce::String& message) { showError (message); };
    arrangeView.onAskMidiTempo =
        [] (const juce::String& fileName, double fileBpm, std::function<void (bool adopt)> answer)
    {
        const auto options =
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle (TRANS ("MIDI file with its own tempo"))
                .withMessage (plugins::i18n::format (
                    TRANS ("{file} has its own tempo ({bpm} BPM) or a different time signature. Adopt it in the project?\n\nAdopting replaces tempo and time signature from the insert position. Keeping places the notes in beats at the current tempo."),
                    { { "file", fileName }, { "bpm", juce::String (fileBpm, 1) } }))
                .withButton (TRANS ("Adopt tempo"))
                .withButton (TRANS ("Keep project tempo"));

        // Erster Knopf liefert 1, zweiter 0.
        juce::AlertWindow::showAsync (options, [answer = std::move (answer)] (int result) { answer (result == 1); });
    };
    keyEditor.onError = [this] (const juce::String& message) { showError (message); };
    drumEditor.onError = [this] (const juce::String& message) { showError (message); };
    transportBar.onMessage = [this] (const juce::String& message) { showError (message); };
    listEditor.onError = [this] (const juce::String& message) { showError (message); };
    instrumentBrowser.onError = [this] (const juce::String& message) { showError (message); };
    instrumentBrowser.onOpenContentManager = [this]
    { ContentManagerDialog::show (services.content, [this] { instrumentBrowser.refresh(); }); };
    arrangeView.onBakeRequested = [this] (services::TrackHandle* track, BakeAction action)
    { performBake (track, action); };
    inspector.onBakeRequested = [this] (services::TrackHandle* track, BakeAction action, int insertCount)
    { performBake (track, action, insertCount); };
    inspector.onError = [this] (const juce::String& message) { showError (message); };
    mixConsole.onError = [this] (const juce::String& message) { showError (message); };
    mediaBrowser.onImportRequested = [this] (const juce::StringArray& files)
    { arrangeView.importFilesAtPlayhead (files); };

    menuBar.setModel (this);
    addAndMakeVisible (menuBar);
    lowerZoneSplitter.onDragStarted = [this] { lowerZoneHeightAtDragStart = lowerZoneHeight; };
    lowerZoneSplitter.onDragged = [this] (int distanceY)
    {
        lowerZoneHeight = lowerZoneHeightAtDragStart - distanceY;
        resized();
    };
    lowerZoneSplitter.onReset = [this]
    {
        lowerZoneHeight = theme::mixerHeight;
        resized();
    };
    addAndMakeVisible (lowerZoneSplitter);
    inspector.onOpenInsertEditor = [this] (const juce::String& insertId) { mixConsole.openInsertEditor (insertId); };
    inspector.onOpenInstrument = [this] (const juce::String& instrumentId)
    { mixConsole.openInsertEditor (instrumentId); };
    arrangeView.onOpenInstrument = [this] (services::TrackHandle* track)
    {
        if (track == nullptr)
            return;

        if (const auto instrument = services.plugins.getInstrument (*track))
            mixConsole.openInsertEditor (instrument->id);
        else
            showError (TRANS ("This track has no instrument."));
    };

    for (auto* component : std::initializer_list<juce::Component*> { &newButton,           &openButton,
                                                                     &saveButton,          &saveAsButton,
                                                                     &recentButton,        &settingsButton,
                                                                     &exportButton,        &addTrackButton,
                                                                     &addInstrumentButton, &addGroupButton,
                                                                     &addFxButton,         &addFolderButton,
                                                                     &undoButton,          &redoButton,
                                                                     &historyButton,       &zoomMenuButton,
                                                                     &overviewButton,      &cutButton,
                                                                     &copyButton,          &pasteButton,
                                                                     &duplicateButton,     &glueButton,
                                                                     &deleteClipsButton,   &midiLearnButton,
                                                                     &midiLearnHint,       &audioSettingsButton,
                                                                     &pluginManagerButton, &selectToolButton,
                                                                     &drawToolButton,      &splitToolButton,
                                                                     &eraseToolButton,     &glueToolButton,
                                                                     &zoomToolButton,      &muteToolButton,
                                                                     &rangeToolButton,     &nudgeLeftButton,
                                                                     &nudgeRightButton,    &gridBox,
                                                                     &snapButton,          &tracksSmallerButton,
                                                                     &tracksLargerButton,  &trackVisibilityButton,
                                                                     &inspectorButton,     &lowerZoneButton,
                                                                     &projectNameLabel,    &inspectorViewport,
                                                                     &arrangeView,         &lowerZone,
                                                                     &transportBar })
        addAndMakeVisible (component);

    inspectorViewport.setViewedComponent (&inspector, false);
    inspectorViewport.setScrollBarsShown (true, false);
    setWantsKeyboardFocus (true);
    services.project.addListener (*this);
    updateToolbar();
    midiLearnChanged(); // Hinweis anfangs ausblenden

    keyBindings.onChanged = [this] { updateShortcutTooltips(); };
    updateShortcutTooltips();

    if (services.updates.isAvailable() && services.updates.isAutomaticCheckEnabled())
        checkForUpdates (false);
}

void ProjectComponent::openWebsite (const juce::String& path)
{
    juce::URL (juce::String (core::buildInfo::websiteUrl) + path).launchInDefaultBrowser();
}

void ProjectComponent::chooseLanguage (std::size_t index)
{
    if (index >= plugins::i18n::allLanguages.size())
        return;

    auto setting = languageSetting();
    const auto language = plugins::i18n::allLanguages[index];

    if (language == setting.resolve())
        return;

    if (const auto chosen = setting.choose (language); ! chosen)
    {
        showError (services::errorText (chosen.error()));
        return;
    }

    // Die DAW baut ihre Oberfläche beim Start; ein Neuaufbau im laufenden Betrieb lohnt nicht.
    // Plugins und Standalone-Programme übernehmen die Wahl beim nächsten Öffnen ihres Fensters.
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        plugins::i18n::nativeName (language),
        TRANS ("The new language takes effect the next time FK Audio Suite starts. FK plugins switch the next time their window opens."));
}

void ProjectComponent::checkForUpdates (bool userAsked)
{
    // Im Hintergrund, mit einer Kopie des Dienstes: Endet das Programm während der Abfrage, zeigt
    // der Thread auf nichts, was schon abgebaut ist. Das Ergebnis kommt über callAsync zurück.
    juce::Thread::launch (
        [checker = services.updates, safeThis = juce::Component::SafePointer<ProjectComponent> (this), userAsked]
        {
            auto result = checker.check();
            juce::MessageManager::callAsync (
                [safeThis, userAsked, result = std::move (result)]
                {
                    if (safeThis == nullptr)
                        return;

                    if (! result)
                    {
                        if (userAsked)
                            safeThis->showError (services::errorText (result.error()));

                        return;
                    }

                    if (! result->has_value())
                    {
                        if (userAsked)
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::MessageBoxIconType::InfoIcon,
                                TRANS ("Updates"),
                                TRANS ("FK Audio Suite is up to date."));
                        return;
                    }

                    const auto update = **result;
                    const auto options =
                        juce::MessageBoxOptions()
                            .withIconType (juce::MessageBoxIconType::InfoIcon)
                            .withTitle (TRANS ("New version"))
                            .withMessage (plugins::i18n::format (
                                TRANS ("FK Audio Suite {version} is available (from {date}). Installed version: {installed}."),
                                { { "version", update.version },
                                  { "date", update.date },
                                  { "installed", juce::String (core::buildInfo::version) } }))
                            .withButton (TRANS ("Download"))
                            .withButton (TRANS ("Later"));
                    juce::AlertWindow::showAsync (options,
                                                  [url = update.url] (int button)
                                                  {
                                                      if (button == 1)
                                                          juce::URL (url).launchInDefaultBrowser();
                                                  });
                });
        });
}

ProjectComponent::~ProjectComponent()
{
    menuBar.setModel (nullptr);
    services.midiLearn.removeListener (*this);
    services.project.removeListener (*this);
}

void ProjectComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);
    g.setColour (theme::backgroundPanel);
    g.fillRect (getLocalBounds().removeFromTop (theme::menuBarHeight + toolbarHeight));
}

std::vector<ProjectComponent::ToolbarItem> ProjectComponent::getToolbarItems()
{
    constexpr int icon = 32;
    const auto labelled = [] (juce::Component& component, int width, bool group = false)
    { return ToolbarItem { &component, width, icon, group }; };
    const auto plain = [] (juce::Component& component, bool group = false)
    { return ToolbarItem { &component, icon, icon, group }; };

    return { labelled (newButton, 70),
             labelled (openButton, 80),
             plain (recentButton),
             labelled (saveButton, 96),
             plain (saveAsButton),
             plain (settingsButton),
             labelled (exportButton, 84),
             labelled (addTrackButton, 84, true),
             labelled (addInstrumentButton, 108),
             labelled (addGroupButton, 90),
             labelled (addFxButton, 64),
             labelled (addFolderButton, 90),
             plain (undoButton, true),
             plain (redoButton),
             plain (historyButton),
             plain (cutButton, true),
             plain (copyButton),
             plain (pasteButton),
             plain (duplicateButton),
             plain (glueButton),
             plain (deleteClipsButton),
             plain (selectToolButton, true),
             plain (rangeToolButton),
             plain (drawToolButton),
             plain (splitToolButton),
             plain (eraseToolButton),
             plain (glueToolButton),
             plain (zoomToolButton),
             plain (muteToolButton),
             plain (nudgeLeftButton, true),
             plain (nudgeRightButton),
             ToolbarItem { &gridBox, 88, 88, true },
             plain (snapButton),
             plain (zoomMenuButton),
             plain (overviewButton),
             plain (tracksSmallerButton),
             plain (tracksLargerButton),
             plain (trackVisibilityButton),
             plain (inspectorButton, true),
             plain (lowerZoneButton),
             plain (audioSettingsButton, true),
             plain (pluginManagerButton),
             plain (midiLearnButton) };
}

int ProjectComponent::layoutToolbar (juce::Rectangle<int> area)
{
    constexpr int minNameWidth = 160; // Projektname oder MIDI-Learn-Hinweis braucht Platz
    const auto items = getToolbarItems();
    const auto inner = area.reduced (theme::spacingMedium, 0);

    // Beschriftungen einzeln opfern, die der Spur-Buttons zuerst, Neu/Öffnen/Speichern zuletzt;
    // erst wenn alle weg sind, bricht die Leiste um. Jede Funktion bleibt als Symbol mit Tooltip sichtbar.
    const std::array<juce::Component*, 9> sacrificeOrder { &addFolderButton,     &addFxButton,    &addGroupButton,
                                                           &addInstrumentButton, &addTrackButton, &exportButton,
                                                           &saveButton,          &openButton,     &newButton };
    std::set<juce::Component*> compact;

    const auto requiredWidth = [&items, &compact]
    {
        int width = 0;

        for (const auto& item : items)
            width += (compact.contains (item.component) ? item.compactWidth : item.width) + theme::spacingSmall +
                     (item.startsGroup ? theme::spacingMedium : 0);

        return width;
    };

    for (auto* candidate : sacrificeOrder)
    {
        if (requiredWidth() + minNameWidth <= inner.getWidth())
            break;

        compact.insert (candidate);
    }

    for (const auto& item : items)
        if (auto* button = dynamic_cast<IconButton*> (item.component))
            button->setCompact (compact.contains (item.component));

    const int rowHeight = theme::toolbarHeight;
    int row = 0;
    int x = inner.getX();

    for (const auto& item : items)
    {
        const int width = compact.contains (item.component) ? item.compactWidth : item.width;
        const int gap = item.startsGroup && x > inner.getX() ? theme::spacingMedium : 0;

        if (x + gap + width > inner.getRight())
        {
            ++row;
            x = inner.getX();
        }
        else
        {
            x += gap;
        }

        item.component->setBounds (
            juce::Rectangle<int> (x, area.getY() + row * rowHeight, width, rowHeight).reduced (0, theme::spacingSmall));
        x += width + theme::spacingSmall;
    }

    // Rest der letzten Reihe: Hinweis im Lernmodus, sonst der Projektname; reicht er nicht, eine Reihe mehr.
    if (inner.getRight() - x < minNameWidth)
    {
        ++row;
        x = inner.getX();
    }

    auto rest = juce::Rectangle<int> (x, area.getY() + row * rowHeight, inner.getRight() - x, rowHeight)
                    .reduced (0, theme::spacingSmall);

    if (midiLearnHint.isVisible())
        midiLearnHint.setBounds (rest.removeFromLeft (juce::jmin (320, rest.getWidth() / 2)));

    projectNameLabel.setBounds (rest);
    return (row + 1) * rowHeight;
}

void ProjectComponent::resized()
{
    auto area = getLocalBounds();
    menuBar.setBounds (area.removeFromTop (theme::menuBarHeight));

    const auto newToolbarHeight = layoutToolbar (area.withHeight (theme::toolbarHeight * 4));
    area.removeFromTop (newToolbarHeight);

    if (newToolbarHeight != toolbarHeight)
    {
        toolbarHeight = newToolbarHeight;
        repaint();
    }

    transportBar.setBounds (area.removeFromBottom (transportBar.getRequiredHeight (getWidth())));

    lowerZoneSplitter.setVisible (lowerZone.isVisible());

    if (lowerZone.isVisible())
    {
        // Mindestens ein Streifen Projektzone bleibt sichtbar, die untere Zone nie winzig.
        constexpr int minLowerZoneHeight = 160;
        constexpr int minArrangeHeight = 120;
        lowerZoneHeight = juce::jlimit (
            minLowerZoneHeight, juce::jmax (minLowerZoneHeight, area.getHeight() - minArrangeHeight), lowerZoneHeight);
        lowerZone.setBounds (area.removeFromBottom (lowerZoneHeight));
        lowerZoneSplitter.setBounds (area.removeFromBottom (ZoneSplitter::thickness));
    }

    if (inspectorViewport.isVisible())
    {
        // Unter der Mindesthöhe rollt der Inspector, statt Fader und Knöpfe zu verschlucken.
        constexpr int inspectorMinimumHeight = 660;
        inspectorViewport.setBounds (area.removeFromLeft (theme::inspectorWidth));
        const bool needsScrolling = inspectorViewport.getHeight() < inspectorMinimumHeight;
        const auto width = theme::inspectorWidth - (needsScrolling ? inspectorViewport.getScrollBarThickness() : 0);
        inspector.setSize (width, juce::jmax (inspectorViewport.getHeight(), inspectorMinimumHeight));
    }

    arrangeView.setBounds (area);
}

bool ProjectComponent::keyPressed (const juce::KeyPress& key)
{
    // Tastenkürzel sind Zusatz zu Buttons und Menüs; welche Taste was tut, steht in der Befehlstabelle
    // (Bearbeiten > Tastaturbefehle, Vorgabe wie Cubase).
    if (const auto command = keyBindings.find (key))
        return performCommand (*command);

    return false;
}

bool ProjectComponent::performCommand (Command command)
{
    switch (command)
    {
        case Command::play:
            services.transport.togglePlay();
            break;
        case Command::record:
            services.transport.record();
            break;
        case Command::retrospectiveRecord:
            transportBar.captureRetrospective();
            break;
        case Command::toggleLoop:
            services.timeline.setLooping (! services.timeline.isLooping());
            break;
        case Command::rewindBar:
        case Command::forwardBar:
            transportBar.stepBars (command == Command::forwardBar ? 1 : -1);
            break;
        case Command::returnToStart:
            services.transport.returnToStart();
            break;
        case Command::jumpLeftLocator:
        case Command::jumpRightLocator:
            transportBar.jumpToLocator (command == Command::jumpLeftLocator);
            break;
        case Command::setLeftLocator:
        case Command::setRightLocator:
            transportBar.setLocatorToPlayhead (command == Command::setLeftLocator);
            break;
        case Command::togglePunch:
            transportBar.togglePunch();
            break;
        case Command::addMarker:
            (void)services.markers.addMarker (services.transport.getPositionSeconds());
            break;
        case Command::previousMarker:
        case Command::nextMarker:
        {
            const auto position = services.transport.getPositionSeconds();
            const auto target = command == Command::nextMarker ? services.markers.getNextMarkerSeconds (position)
                                                               : services.markers.getPreviousMarkerSeconds (position);

            if (target)
                services.transport.setPositionSeconds (*target);

            break;
        }
        case Command::toggleMetronome:
            services.timeline.setMetronomeEnabled (! services.timeline.isMetronomeEnabled());
            break;

        case Command::save:
            saveButton.triggerClick();
            break;
        case Command::saveAs:
            saveAsButton.triggerClick();
            break;
        case Command::projectSettings:
            settingsButton.triggerClick();
            break;
        case Command::exportProject:
            exportButton.triggerClick();
            break;

        case Command::undo:
            undoButton.triggerClick();
            break;
        case Command::redo:
            redoButton.triggerClick();
            break;
        case Command::history:
            historyButton.triggerClick();
            break;
        case Command::cut:
            cutButton.triggerClick();
            break;
        case Command::copy:
            copyButton.triggerClick();
            break;
        case Command::paste:
            pasteButton.triggerClick();
            break;
        case Command::duplicate:
            duplicateButton.triggerClick();
            break;
        case Command::glue:
            glueButton.triggerClick();
            break;
        case Command::selectAll:
            arrangeView.selectAllClips();
            break;
        case Command::deleteSelection:
        case Command::deleteAndClose:
            // Mit Bereichsauswahl wirkt Löschen auf den Bereich, sonst auf die Clips.
            if (arrangeView.getRangeSelection())
                arrangeView.performRangeAction (command == Command::deleteAndClose
                                                    ? ArrangeView::RangeAction::removeAndClose
                                                    : ArrangeView::RangeAction::remove);
            else
                deleteClipsButton.triggerClick();
            break;
        case Command::toggleClipMute:
            performClipAction (ClipAction::toggleMute);
            break;
        case Command::nudgeLeft:
        case Command::nudgeRight:
            arrangeView.nudgeSelectedClips (command == Command::nudgeRight ? 1 : -1);
            break;

        case Command::toolSelect:
            selectToolButton.triggerClick();
            break;
        case Command::toolRange:
            rangeToolButton.triggerClick();
            break;
        case Command::toolSplit:
            splitToolButton.triggerClick();
            break;
        case Command::toolGlue:
            glueToolButton.triggerClick();
            break;
        case Command::toolErase:
            eraseToolButton.triggerClick();
            break;
        case Command::toolZoom:
            zoomToolButton.triggerClick();
            break;
        case Command::toolMute:
            muteToolButton.triggerClick();
            break;
        case Command::toolDraw:
            drawToolButton.triggerClick();
            break;

        case Command::zoomIn:
        case Command::zoomOut:
            arrangeView.zoomStep (command == Command::zoomIn);
            break;
        case Command::zoomProject:
            arrangeView.zoomToProject();
            break;
        case Command::zoomSelection:
            if (! arrangeView.zoomToSelection())
                showError (TRANS ("Nothing selected: select clips or a range first."));
            break;

        case Command::inspector:
            inspectorButton.triggerClick();
            break;
        case Command::mixer:
            showLowerZoneTab (0);
            break;
        case Command::keyboard:
            showLowerZoneTab (1);
            break;
        case Command::keyEditor:
            showLowerZoneTab (2);
            break;
        case Command::mediaBrowser:
            showLowerZoneTab (3);
            break;
        case Command::sampleEditor:
            showLowerZoneTab (4);
            break;
        case Command::drumEditor:
            showLowerZoneTab (5);
            break;
        case Command::listEditor:
            showLowerZoneTab (6);
            break;
        case Command::instrumentBrowser:
            showLowerZoneTab (7);
            break;
        case Command::overview:
            toggleOverview();
            break;
        case Command::shortcutEditor:
            ShortcutEditor::show (keyBindings);
            break;
    }

    return true;
}

void ProjectComponent::updateShortcutTooltips()
{
    const std::vector<std::pair<juce::SettableTooltipClient*, Command>> buttons {
        { &saveButton, Command::save },
        { &saveAsButton, Command::saveAs },
        { &settingsButton, Command::projectSettings },
        { &exportButton, Command::exportProject },
        { &undoButton, Command::undo },
        { &redoButton, Command::redo },
        { &historyButton, Command::history },
        { &cutButton, Command::cut },
        { &copyButton, Command::copy },
        { &pasteButton, Command::paste },
        { &duplicateButton, Command::duplicate },
        { &glueButton, Command::glue },
        { &deleteClipsButton, Command::deleteSelection },
        { &selectToolButton, Command::toolSelect },
        { &rangeToolButton, Command::toolRange },
        { &splitToolButton, Command::toolSplit },
        { &glueToolButton, Command::toolGlue },
        { &eraseToolButton, Command::toolErase },
        { &zoomToolButton, Command::toolZoom },
        { &muteToolButton, Command::toolMute },
        { &drawToolButton, Command::toolDraw },
        { &nudgeLeftButton, Command::nudgeLeft },
        { &nudgeRightButton, Command::nudgeRight },
        { &overviewButton, Command::overview },
        { &inspectorButton, Command::inspector },
    };

    for (const auto& [button, command] : buttons)
    {
        auto* component = dynamic_cast<juce::Component*> (button);
        auto [entry, inserted] = tooltipBases.try_emplace (component);

        if (inserted)
        {
            // Beim ersten Mal die feste Taste vom Ende des Tooltips abtrennen: "Kopieren (Strg+C)" → "Kopieren".
            auto base = button->getTooltip();

            if (base.endsWithChar (')'))
                base = base.upToLastOccurrenceOf (" (", false, false);

            entry->second = base;
        }

        const auto shortcut = keyBindings.shortcutText (command);
        button->setTooltip (shortcut.isEmpty() ? entry->second : entry->second + " (" + shortcut + ")");
    }
}

void ProjectComponent::projectChanged()
{
    updateToolbar();
    checkDeviceSampleRate();
}

void ProjectComponent::checkDeviceSampleRate()
{
    // Einmal je geöffnetem Projekt fragen, nicht bei jeder Änderung erneut.
    const auto editFile = services.project.getEditFile();

    if (! services.project.hasProject() || editFile == lastSampleRateCheck)
        return;

    lastSampleRateCheck = editFile;

    if (services.projectSettings.deviceMatchesProject())
        return;

    const auto projectRate = services.projectSettings.getSettings().sampleRate;
    const auto deviceRate = services.projectSettings.getDeviceSampleRate();
    const auto projectRateText = juce::String (juce::roundToInt (projectRate));
    const auto deviceRateText = juce::String (juce::roundToInt (deviceRate));

    // Unter JACK/PipeWire bestimmt der Server die Rate: Dann lässt sich nur das Projekt anpassen.
    if (! services.projectSettings.canDeviceRunAt (projectRate))
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (
                juce::MessageBoxIconType::QuestionIcon,
                TRANS ("Sample rate"),
                plugins::i18n::format (
                    TRANS ("The project runs at {projectRate} Hz, the audio device at {deviceRate} Hz. The device cannot be switched (under PipeWire/JACK the server decides the rate).\n\nSwitch the project to {deviceRate} Hz? Existing recordings stay unchanged, the engine converts them during playback."),
                    { { "projectRate", projectRateText }, { "deviceRate", deviceRateText } }),
                TRANS ("Switch project"),
                TRANS ("Leave as is"),
                this),
            [safeThis = juce::Component::SafePointer (this), deviceRate] (int choice)
            {
                if (safeThis == nullptr || choice != 1)
                    return;

                auto settings = safeThis->services.projectSettings.getSettings();
                settings.sampleRate = deviceRate;

                if (const auto result = safeThis->services.projectSettings.setSettings (settings); ! result)
                    safeThis->showError (services::errorText (result.error()));
            });
        return;
    }

    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::QuestionIcon,
            TRANS ("Sample rate"),
            plugins::i18n::format (
                TRANS ("The project runs at {projectRate} Hz, the audio device at {deviceRate} Hz.\n\nSwitch the audio device? Otherwise the engine converts during playback, and new recordings use the device rate."),
                { { "projectRate", projectRateText }, { "deviceRate", deviceRateText } }),
            TRANS ("Switch device"),
            TRANS ("Leave as is"),
            this),
        [safeThis = juce::Component::SafePointer (this), projectRate] (int choice)
        {
            if (safeThis == nullptr || choice != 1)
                return;

            if (const auto result = safeThis->services.projectSettings.setDeviceSampleRate (projectRate); ! result)
                safeThis->showError (services::errorText (result.error()));
        });
}

void ProjectComponent::showProjectSettings()
{
    if (! services.project.hasProject())
        return;

    ProjectSettingsDialog::show (services.projectSettings, services.autosave);
}

void ProjectComponent::showNewMenu()
{
    juce::PopupMenu menu;
    menu.addItem (itemEmptyProject, TRANS ("Empty project..."));

    const auto templates = services.templates.getTemplates();

    if (! templates.empty())
    {
        menu.addSectionHeader (TRANS ("From template"));

        for (std::size_t i = 0; i < templates.size(); ++i)
            menu.addItem (templateMenuBase + static_cast<int> (i), templates[i].getFileNameWithoutExtension());
    }

    menu.addSeparator();
    menu.addItem (itemSaveTemplate, TRANS ("Save current project as template..."), services.project.hasProject());
    menu.addItem (itemOpenTemplateFolder, TRANS ("Open templates folder (rename, delete)"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (newButton),
                        [safeThis = juce::Component::SafePointer (this), templates] (int chosen)
                        {
                            if (safeThis == nullptr || chosen == 0)
                                return;

                            if (chosen >= templateMenuBase)
                            {
                                const auto index = static_cast<std::size_t> (chosen - templateMenuBase);

                                if (index < templates.size())
                                    safeThis->newProject (templates[index]);
                            }
                            else
                            {
                                safeThis->menuItemSelected (chosen, 0);
                            }
                        });
}

void ProjectComponent::saveAsTemplate()
{
    if (! services.project.hasProject())
        return;

    auto* window = new juce::AlertWindow (TRANS ("Save as template"),
                                          TRANS ("Tracks, instruments, effects, routing, tempo and settings go into the template, audio clips do not. A template with the same name is replaced."),
                                          juce::MessageBoxIconType::NoIcon,
                                          this);
    window->addTextEditor ("name", services.project.getProjectName(), TRANS ("Name"));
    window->addButton (TRANS ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // deleteWhenDismissed: JUCE löscht das Fenster nach dem Rückruf; bis dahin ist es gültig.
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create (
                                 [safeThis = juce::Component::SafePointer (this), window] (int choice)
                                 {
                                     if (safeThis == nullptr || choice != 1)
                                         return;

                                     if (const auto saved = safeThis->services.templates.saveAsTemplate (
                                             window->getTextEditorContents ("name"));
                                         ! saved)
                                         safeThis->showError (services::errorText (saved.error()));
                                 }),
                             true);
}

void ProjectComponent::exportDiagnostics()
{
    juce::FileChooser chooser (
        TRANS ("Save diagnostic report"),
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile ("FK-Diagnose " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M") + ".zip"),
        "*.zip");

    if (! chooser.browseForFileToSave (true))
        return;

    const auto target = chooser.getResult().withFileExtension (".zip");

    if (const auto result =
            services.diagnostics.exportReport (target, juce::JUCEApplication::getInstance()->getApplicationVersion());
        ! result)
    {
        showError (services::errorText (result.error()));
        return;
    }

    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        TRANS ("Diagnostic report"),
        plugins::i18n::format (
            TRANS ("Saved to\n{file}\n\nContains a system description, the latest logs and hang or crash reports. Nothing is sent automatically."),
            { { "file", target.getFullPathName() } }));
}

void ProjectComponent::closeProjectThen (std::function<void()> action)
{
    if (! services.project.isModified())
    {
        services.project.closeProject();
        action();
        return;
    }

    auto options = juce::MessageBoxOptions::makeOptionsYesNoCancel (
        juce::MessageBoxIconType::QuestionIcon,
        TRANS ("Unsaved changes"),
        plugins::i18n::format (TRANS ("The project \"{name}\" has unsaved changes. Save first?"),
                               { { "name", services.project.getProjectName() } }),
        TRANS ("Save"),
        TRANS ("Don't save"),
        TRANS ("Cancel"),
        this);

    // Rückgabe: 1 = Speichern, 2 = Nicht speichern, 0 = Abbrechen.
    juce::AlertWindow::showAsync (options,
                                  [safeThis = juce::Component::SafePointer (this), action] (int choice)
                                  {
                                      if (safeThis == nullptr || choice == 0)
                                          return;

                                      if (choice == 1)
                                          if (const auto result = safeThis->services.project.saveProject(); ! result)
                                          {
                                              safeThis->showError (services::errorText (result.error()));
                                              return;
                                          }

                                      safeThis->services.project.closeProject();
                                      action();
                                  });
}

void ProjectComponent::newProject (const juce::File& templateFile)
{
    // Modale Dialoge sind bis M3 bewusst erlaubt (JUCE_MODAL_LOOPS_PERMITTED); asynchron später.
    // Erst den Ort wählen, dann schließen: Wer den Dialog abbricht, behält sein Projekt.
    juce::FileChooser chooser (TRANS ("New project: name and location"),
                               juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                               editFileWildcard);

    if (! chooser.browseForFileToSave (false))
        return;

    const auto chosen = chooser.getResult();

    closeProjectThen (
        [this, chosen, templateFile]
        {
            const auto parent = chosen.getParentDirectory();
            const auto name = chosen.getFileNameWithoutExtension();
            const auto result = templateFile == juce::File()
                                    ? services.project.createProject (parent, name)
                                    : services.project.createProjectFromTemplate (parent, name, templateFile);

            if (! result)
                showError (services::errorText (result.error()));
        });
}

void ProjectComponent::openProject()
{
    juce::FileChooser chooser (
        TRANS ("Open project"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), editFileWildcard);

    if (! chooser.browseForFileToOpen())
        return;

    openProjectFile (chooser.getResult());
}

void ProjectComponent::openProjectFile (const juce::File& editFile)
{
    closeProjectThen (
        [this, editFile]
        {
            const auto autosave = services::ProjectService::findNewerAutosave (editFile);

            if (! autosave)
            {
                if (const auto result = services.project.openProject (editFile); ! result)
                    showError (services::errorText (result.error()));

                return;
            }

            // Eine automatische Sicherung, die neuer ist als die Projektdatei, bleibt nur nach einem Absturz übrig.
            // Rückgabe: 1 = Sicherung wiederherstellen, 0 = gespeicherte Fassung öffnen.
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions::makeOptionsOkCancel (
                    juce::MessageBoxIconType::WarningIcon,
                    TRANS ("Restore?"),
                    plugins::i18n::format (
                        TRANS ("The project was not closed properly. There is an automatic backup from {time}, newer than the saved version.\n\nThe saved version is kept in the Backups folder in any case."),
                        { { "time", autosave->getLastModificationTime().toString (true, true, false, true) } }),
                    TRANS ("Restore backup"),
                    TRANS ("Open saved version"),
                    this),
                [safeThis = juce::Component::SafePointer (this), editFile, autosaveFile = *autosave] (int choice)
                {
                    if (safeThis == nullptr)
                        return;

                    auto& project = safeThis->services.project;
                    core::Result<void> result;

                    if (choice == 1)
                    {
                        result = project.restoreBackup (editFile, autosaveFile);
                    }
                    else
                    {
                        services::ProjectService::archiveAutosave (editFile);
                        result = project.openProject (editFile);
                    }

                    if (! result)
                        safeThis->showError (services::errorText (result.error()));
                });
        });
}

void ProjectComponent::saveProject()
{
    if (const auto result = services.project.saveProject(); ! result)
        showError (services::errorText (result.error()));
}

void ProjectComponent::saveProjectAs()
{
    if (! services.project.hasProject())
        return;

    juce::FileChooser chooser (TRANS ("Save as: new project name and location"),
                               services.project.getProjectFolder().getParentDirectory().getChildFile (
                                   plugins::i18n::format (TRANS ("{name} copy"),
                                                          { { "name", services.project.getProjectName() } })),
                               editFileWildcard);

    if (! chooser.browseForFileToSave (false))
        return;

    const auto chosen = chooser.getResult();

    if (const auto result =
            services.project.saveProjectAs (chosen.getParentDirectory(), chosen.getFileNameWithoutExtension());
        ! result)
        showError (services::errorText (result.error()));
}

void ProjectComponent::addRecentProjectsTo (juce::PopupMenu& menu)
{
    const auto recent = services.project.getRecentProjects();
    int id = recentMenuBase;

    for (const auto& file : recent)
        menu.addItem (id++,
                      file.getFileNameWithoutExtension() + "   (" +
                          file.getParentDirectory().getParentDirectory().getFullPathName() + ")");

    if (recent.empty())
        menu.addItem (-1, TRANS ("No projects yet"), false);
}

void ProjectComponent::openRecentProject (int menuItemId)
{
    const auto recent = services.project.getRecentProjects();
    const auto index = static_cast<std::size_t> (menuItemId - recentMenuBase);

    if (index < recent.size())
        openProjectFile (recent[index]);
}

void ProjectComponent::addTrackVisibilityItems (juce::PopupMenu& menu)
{
    const auto allTracks = services.tracks.getAudioTracks();
    bool anyHidden = false;

    for (std::size_t i = 0; i < allTracks.size(); ++i)
    {
        const auto info = services.tracks.getTrackInfo (*allTracks[i]);
        menu.addItem (visibilityMenuBase + static_cast<int> (i), info.name, true, ! info.hidden);
        anyHidden = anyHidden || info.hidden;
    }

    if (allTracks.empty())
        menu.addItem (-1, TRANS ("No tracks"), false);

    menu.addSeparator();
    menu.addItem (itemShowAllTracks, TRANS ("Show all"), anyHidden);
}

void ProjectComponent::toggleTrackVisibility (int menuItemId)
{
    const auto allTracks = services.tracks.getAudioTracks();
    const auto index = static_cast<std::size_t> (menuItemId - visibilityMenuBase);

    if (index < allTracks.size())
        services.tracks.setHidden (*allTracks[index], ! services.tracks.getTrackInfo (*allTracks[index]).hidden);
}

void ProjectComponent::showTrackVisibilityMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (TRANS ("Visible tracks (tick = visible)"));
    addTrackVisibilityItems (menu);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (trackVisibilityButton),
                        [safeThis = juce::Component::SafePointer (this)] (int chosen)
                        {
                            if (safeThis != nullptr && chosen != 0)
                                safeThis->menuItemSelected (chosen, 0);
                        });
}

void ProjectComponent::showRecentMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (TRANS ("Recently opened"));
    addRecentProjectsTo (menu);
    menu.addSeparator();
    menu.addItem (itemRestoreBackup, TRANS ("Restore backup..."), services.project.hasProject());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (recentButton),
                        [safeThis = juce::Component::SafePointer (this)] (int chosen)
                        {
                            if (safeThis == nullptr || chosen == 0)
                                return;

                            if (chosen == itemRestoreBackup)
                                safeThis->restoreBackup();
                            else if (chosen >= recentMenuBase)
                                safeThis->openRecentProject (chosen);
                        });
}

void ProjectComponent::restoreBackup()
{
    if (! services.project.hasProject())
        return;

    const auto editFile = services.project.getEditFile();
    const auto backups = services::ProjectService::getBackups (editFile);

    if (backups.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            TRANS ("Backups"),
            TRANS ("This project has no backups yet. Every save with changes creates one."));
        return;
    }

    // Auswahl als Menü statt Dateidialog: neueste zuerst, mit Uhrzeit.
    juce::PopupMenu menu;
    menu.addSectionHeader (TRANS ("Choose backup (the current version is kept as a backup)"));

    for (std::size_t i = 0; i < backups.size(); ++i)
        menu.addItem (static_cast<int> (i) + 1,
                      backups[i].getLastModificationTime().toString (true, true, false, true) + "   " +
                          backups[i].getFileNameWithoutExtension());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (recentButton),
                        [safeThis = juce::Component::SafePointer (this), editFile, backups] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            const auto backup = backups[static_cast<std::size_t> (chosen - 1)];
                            safeThis->closeProjectThen (
                                [safeThis, editFile, backup]
                                {
                                    if (safeThis == nullptr)
                                        return;

                                    if (const auto result = safeThis->services.project.restoreBackup (editFile, backup);
                                        ! result)
                                        safeThis->showError (services::errorText (result.error()));
                                });
                        });
}

void ProjectComponent::exportMixdown()
{
    if (! services.project.hasProject())
    {
        showError (TRANS ("No project open."));
        return;
    }

    ExportDialog::show ({ services.exporter, services.project, services.projectSettings, services.tracks });
}

void ProjectComponent::toggleOverview()
{
    arrangeView.setOverviewShown (! arrangeView.isOverviewShown());
    overviewButton.setToggleState (arrangeView.isOverviewShown(), juce::dontSendNotification);
}

void ProjectComponent::showZoomMenu()
{
    if (! services.project.hasProject())
        return;

    // Feste IDs für die Befehle, Takt-Zooms ab 100, eigene Presets ab 200 und zum Löschen ab 300.
    enum
    {
        zoomProject = 1,
        zoomSelection,
        zoomIn,
        zoomOut,
        savePreset,
        barsBase = 100,
        presetBase = 200,
        removeBase = 300
    };

    constexpr std::array<int, 4> barCounts { 1, 4, 16, 64 };
    const auto presets = services.timeline.getZoomPresets();

    juce::PopupMenu menu;
    addIconItem (
        menu, zoomProject, Icon::zoomFit, keyBindings.menuText (TRANS ("Whole project"), Command::zoomProject));
    addIconItem (
        menu, zoomSelection, Icon::zoomFit, keyBindings.menuText (TRANS ("To selection"), Command::zoomSelection));
    addIconItem (menu, zoomIn, Icon::plus, keyBindings.menuText (TRANS ("Zoom in"), Command::zoomIn));
    addIconItem (menu, zoomOut, Icon::minus, keyBindings.menuText (TRANS ("Zoom out"), Command::zoomOut));
    menu.addSectionHeader (TRANS ("Bars"));

    for (std::size_t i = 0; i < barCounts.size(); ++i)
    {
        const int count = barCounts[i];
        menu.addItem (barsBase + static_cast<int> (i), plugins::i18n::plural (count, "{count} bar", "{count} bars"));
    }

    menu.addSectionHeader (TRANS ("Custom presets"));

    if (presets.empty())
        menu.addItem (-1, TRANS ("(none yet, \"Save current zoom\" creates one)"), false);

    for (std::size_t i = 0; i < presets.size(); ++i)
        menu.addItem (presetBase + static_cast<int> (i), presets[i].name);

    menu.addSeparator();
    addIconItem (menu, savePreset, Icon::plus, TRANS ("Save current zoom..."));

    if (! presets.empty())
    {
        juce::PopupMenu removeMenu;

        for (std::size_t i = 0; i < presets.size(); ++i)
            removeMenu.addItem (removeBase + static_cast<int> (i), presets[i].name);

        menu.addSubMenu (TRANS ("Delete preset"), removeMenu);
    }

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&zoomMenuButton),
        [safeThis = juce::Component::SafePointer<ProjectComponent> (this), presets, barCounts] (int chosen)
        {
            if (safeThis == nullptr || chosen <= 0)
                return;

            auto& view = safeThis->arrangeView;
            const auto [from, to] = view.getVisibleRange();

            if (chosen == zoomProject)
                view.zoomToProject();
            else if (chosen == zoomSelection)
            {
                if (! view.zoomToSelection())
                    safeThis->showError (TRANS ("Nothing selected: select clips or a range first."));
            }
            else if (chosen == zoomIn || chosen == zoomOut)
                view.zoomStep (chosen == zoomIn);
            else if (chosen == savePreset)
                safeThis->saveZoomPreset();
            else if (chosen >= removeBase)
            {
                if (const auto removed = safeThis->services.timeline.removeZoomPreset (
                        presets[static_cast<std::size_t> (chosen - removeBase)].name);
                    ! removed)
                    safeThis->showError (services::errorText (removed.error()));
            }
            else if (chosen >= presetBase)
                view.showTimeRange (from,
                                    from + presets[static_cast<std::size_t> (chosen - presetBase)].visibleSeconds);
            else if (chosen >= barsBase)
            {
                const auto bars = barCounts[static_cast<std::size_t> (chosen - barsBase)];
                const auto start = safeThis->services.timeline.getPreviousBarSeconds (from + 1.0e-6);
                view.showTimeRange (start, start + bars * safeThis->services.timeline.barLengthSeconds (start));
            }
        });
}

void ProjectComponent::saveZoomPreset()
{
    const auto [from, to] = arrangeView.getVisibleRange();
    auto window = std::make_shared<juce::AlertWindow> (
        TRANS ("Save zoom"),
        plugins::i18n::format (TRANS ("Name for the current zoom ({seconds} seconds visible):"),
                               { { "seconds", juce::String (to - from, 1) } }),
        juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name",
                           plugins::i18n::format (TRANS ("Zoom {seconds} s"),
                                                  { { "seconds", juce::String (juce::roundToInt (to - from)) } }));
    window->addButton (TRANS ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safeThis = juce::Component::SafePointer<ProjectComponent> (this), window, seconds = to - from] (int result)
            {
                if (safeThis == nullptr || result != 1)
                    return;

                if (const auto saved = safeThis->services.timeline.saveZoomPreset (
                        { window->getTextEditorContents ("name"), seconds });
                    ! saved)
                    safeThis->showError (services::errorText (saved.error()));
            }));
}

void ProjectComponent::addTrack()
{
    juce::PopupMenu menu;
    menu.addItem (1, TRANS ("Audio track mono (one input channel)"));
    menu.addItem (2, TRANS ("Audio track stereo (channel pair)"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrackButton),
                        [safeThis = juce::Component::SafePointer<ProjectComponent> (this)] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            const int numChannels = result == 1 ? 1 : 2;
                            const auto trackNumber =
                                static_cast<int> (safeThis->services.tracks.getAudioTracks().size()) + 1;

                            if (const auto created = safeThis->services.tracks.createAudioTrack (
                                    plugins::i18n::format (TRANS ("Audio {number}"), { { "number", juce::String (trackNumber) } }),
                                    numChannels);
                                ! created)
                                safeThis->showError (services::errorText (created.error()));
                        });
}

void ProjectComponent::addInstrumentTrack()
{
    const auto instruments = services.plugins.getAvailableInstruments();
    auto menu = buildPluginMenu (instruments);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addInstrumentButton),
                        [safeThis = juce::Component::SafePointer<ProjectComponent> (this), instruments] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            const auto& chosen = instruments[static_cast<std::size_t> (result - 1)];
                            const auto trackNumber =
                                static_cast<int> (safeThis->services.tracks.getAudioTracks().size()) + 1;

                            if (const auto created = safeThis->services.tracks.createInstrumentTrack (
                                    chosen.name + " " + juce::String (trackNumber), chosen.id);
                                ! created)
                                safeThis->showError (services::errorText (created.error()));
                        });
}

void ProjectComponent::addGroupTrack()
{
    const auto groupNumber = static_cast<int> (services.tracks.getAudioTracks().size()) + 1;

    if (const auto created = services.tracks.createGroupTrack (plugins::i18n::format (
            TRANS ("Group {number}"), { { "number", juce::String (groupNumber) } }));
        ! created)
        showError (services::errorText (created.error()));
}

void ProjectComponent::addFolderTrack()
{
    if (const auto created =
            services.tracks.createFolderTrack (plugins::i18n::format (
            TRANS ("Folder {number}"), { { "number", juce::String (services.tracks.getFolders().size() + 1) } }));
        ! created)
        showError (services::errorText (created.error()));
}

void ProjectComponent::addFxTrack()
{
    const auto fxNumber = static_cast<int> (services.sends.getFxTracks().size()) + 1;

    if (const auto created = services.sends.createFxTrack (plugins::i18n::format (
            TRANS ("FX {number}"), { { "number", juce::String (fxNumber) } }));
        ! created)
        showError (services::errorText (created.error()));
}

void ProjectComponent::insertMidiClipAtCursor()
{
    auto* track = arrangeView.getSelectedTrack();

    if (track == nullptr || ! services::playsMidi (services.tracks.getTrackInfo (*track).kind))
    {
        showError (TRANS ("Please select an instrument or MIDI track first."));
        return;
    }

    const auto start = services.transport.getPositionSeconds();

    if (const auto result =
            services.midiClips.createEmptyClip (*track, start, services.timeline.barLengthSeconds (start));
        ! result)
        showError (services::errorText (result.error()));
    else
        openMidiClip (*result);
}

void ProjectComponent::importAudioFile()
{
    // Dialog asynchron; der Chooser lebt im shared_ptr bis zum Callback. Ablage am
    // Positionszeiger auf der ausgewählten Spur, sonst neue Spur (wie beim Abwurf ins Leere).
    auto chooser = std::make_shared<juce::FileChooser> (
        TRANS ("Import audio or MIDI file"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectMultipleItems,
                          [this, chooser] (const juce::FileChooser& fc)
                          {
                              juce::StringArray files;

                              for (const auto& file : fc.getResults())
                                  files.add (file.getFullPathName());

                              if (files.isEmpty())
                                  return;

                              const auto x = theme::trackHeaderWidth + arrangeView.getLocalBounds().getX() +
                                             1; // Zeit: Positionszeiger (siehe unten)
                              juce::ignoreUnused (x);
                              arrangeView.importFilesAtPlayhead (files);
                          });
}

void ProjectComponent::openMidiClip (const juce::String& clipId)
{
    // Wie Cubase: Spuren mit Drum-Map öffnen ihre Clips im Drum-Editor, alle anderen im Key Editor.
    // Der Listen-Editor zeigt immer denselben Clip mit.
    listEditor.setClip (clipId);
    const auto clip = services.midiClips.getClip (clipId);

    if (clip && services.drumMaps.getMapForTrack (clip->trackId))
    {
        drumEditor.setClip (clipId);
        showLowerZoneTab (5);
        return;
    }

    keyEditor.setClip (clipId);
    showLowerZoneTab (2);
}

void ProjectComponent::showLowerZoneTab (int tab)
{
    if (! lowerZone.isVisible())
        lowerZoneButton.triggerClick();

    switch (tab)
    {
        case 0:
            lowerZone.showMixer();
            break;
        case 1:
            lowerZone.showKeyboard();
            break;
        case 2:
            lowerZone.showKeyEditor();
            break;
        case 3:
            lowerZone.showMedia();
            break;
        case 5:
            lowerZone.showDrumEditor();
            break;
        case 6:
            lowerZone.showListEditor();
            break;
        case 7:
            lowerZone.showInstrumentBrowser();
            break;
        default:
            lowerZone.showSampleEditor();
            break;
    }
}

juce::StringArray ProjectComponent::getMenuBarNames()
{
    return { TRANS ("File"), TRANS ("Edit"), TRANS ("Track"), TRANS ("View"), TRANS ("Transport"), TRANS ("Studio"), TRANS ("Help") };
}

juce::PopupMenu ProjectComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    const bool hasProject = services.project.hasProject();

    switch (menuIndex)
    {
        case fileMenu:
            menu.addItem (itemNew, TRANS ("New empty project..."));
            menu.addItem (itemOpen, TRANS ("Open project..."));
            menu.addItem (itemSave, keyBindings.menuText (TRANS ("Save"), Command::save), hasProject);
            menu.addItem (itemSaveAs, keyBindings.menuText (TRANS ("Save as..."), Command::saveAs), hasProject);
            {
                juce::PopupMenu recent;
                addRecentProjectsTo (recent);
                menu.addSubMenu (TRANS ("Recently opened"), recent);
            }
            menu.addItem (itemRestoreBackup, TRANS ("Restore backup..."), hasProject);
            menu.addSeparator();
            menu.addItem (itemProjectSettings,
                          keyBindings.menuText (TRANS ("Project settings..."), Command::projectSettings),
                          hasProject);
            menu.addItem (itemSaveTemplate, TRANS ("Save as template..."), hasProject);
            {
                juce::PopupMenu templates;
                int templateId = templateMenuBase;

                for (const auto& file : services.templates.getTemplates())
                    templates.addItem (templateId++, file.getFileNameWithoutExtension());

                if (templateId == templateMenuBase)
                    templates.addItem (-1, TRANS ("No templates yet"), false);

                menu.addSubMenu (TRANS ("New from template"), templates);
            }
            menu.addSeparator();
            menu.addItem (itemExport, TRANS ("Export (mixdown, stems, MIDI)..."), hasProject);
            menu.addItem (itemImportAudio, TRANS ("Import audio or MIDI file..."), hasProject);
            menu.addSeparator();
            menu.addItem (itemQuit, TRANS ("Quit"));
            break;

        case editMenu:
        {
            menu.addItem (
                itemUndo, keyBindings.menuText (TRANS ("Undo"), Command::undo), services.project.canUndo());
            menu.addItem (
                itemRedo, keyBindings.menuText (TRANS ("Redo"), Command::redo), services.project.canRedo());
            addIconItem (
                menu, itemHistory, Icon::history, keyBindings.menuText (TRANS ("History..."), Command::history), hasProject);
            addIconItem (menu,
                         itemShortcuts,
                         Icon::keyboard,
                         keyBindings.menuText (TRANS ("Keyboard shortcuts..."), Command::shortcutEditor));
            menu.addSeparator();

            const auto addAction = [&menu, this] (int id, Icon icon, const juce::String& text, ClipAction action)
            {
                juce::PopupMenu::Item item (text);
                item.itemID = id;
                item.isEnabled = arrangeView.canPerformClipAction (action);
                item.image = makeIconDrawable (icon, theme::textPrimary);
                menu.addItem (std::move (item));
            };
            addAction (itemCut, Icon::cut, keyBindings.menuText (TRANS ("Cut"), Command::cut), ClipAction::cut);
            addAction (itemCopy, Icon::copy, keyBindings.menuText (TRANS ("Copy"), Command::copy), ClipAction::copy);
            addAction (itemPaste,
                       Icon::paste,
                       keyBindings.menuText (TRANS ("Paste at playhead"), Command::paste),
                       ClipAction::paste);
            addAction (itemDuplicate,
                       Icon::duplicate,
                       keyBindings.menuText (TRANS ("Duplicate"), Command::duplicate),
                       ClipAction::duplicate);
            addAction (itemGlue, Icon::glue, keyBindings.menuText (TRANS ("Glue"), Command::glue), ClipAction::merge);
            addAction (itemDeleteClips,
                       Icon::trash,
                       keyBindings.menuText (TRANS ("Delete clips"), Command::deleteSelection),
                       ClipAction::remove);
            addAction (itemToggleClipMute,
                       Icon::toolMute,
                       keyBindings.menuText (TRANS ("Mute / unmute"), Command::toggleClipMute),
                       ClipAction::toggleMute);
            {
                const bool hasRange = arrangeView.getRangeSelection().has_value();
                juce::PopupMenu range;
                range.addItem (itemRangeRemove,
                               keyBindings.menuText (TRANS ("Delete (leave gap)"), Command::deleteSelection),
                               hasRange);
                range.addItem (itemRangeRemoveClose,
                               keyBindings.menuText (TRANS ("Remove (close gap)"), Command::deleteAndClose),
                               hasRange);
                range.addItem (itemRangeSilence, TRANS ("Insert silence"), hasRange);
                range.addItem (itemRangeSplit, TRANS ("Split at the boundaries"), hasRange);
                range.addItem (itemRangeLoop, TRANS ("Loop the range"), hasRange);
                menu.addSubMenu (TRANS ("Range (range tool, key 2)"), range, hasProject);
            }
            {
                juce::PopupMenu audio;
                const auto addAudioAction =
                    [&audio, this] (int id, Icon icon, const juce::String& text, ClipAction action)
                {
                    juce::PopupMenu::Item item (text);
                    item.itemID = id;
                    item.isEnabled = arrangeView.canPerformClipAction (action);
                    item.image = makeIconDrawable (icon, theme::textPrimary);
                    audio.addItem (std::move (item));
                };
                addAudioAction (itemNormalize, Icon::normalize, TRANS ("Normalize (peak 0 dBFS)"), ClipAction::normalize);
                addAudioAction (itemReverse, Icon::reverse, TRANS ("Reverse (backwards) on/off"), ClipAction::reverse);
                addAudioAction (itemInvertPhase, Icon::phase, TRANS ("Invert phase"), ClipAction::invertPhase);
                menu.addSubMenu (TRANS ("Audio"), audio, hasProject);
            }
            menu.addItem (itemNudgeLeft,
                          keyBindings.menuText (TRANS ("One grid step left"), Command::nudgeLeft),
                          hasProject);
            menu.addItem (itemNudgeRight,
                          keyBindings.menuText (TRANS ("One grid step right"), Command::nudgeRight),
                          hasProject);
            menu.addSeparator();
            menu.addItem (itemSelectAllClips,
                          keyBindings.menuText (TRANS ("Select all clips"), Command::selectAll),
                          hasProject);
            break;
        }

        case trackMenu:
        {
            menu.addItem (itemAudioMono, TRANS ("Audio track mono"), hasProject);
            menu.addItem (itemAudioStereo, TRANS ("Audio track stereo"), hasProject);

            const auto available = services.plugins.getAvailableInstruments();
            menu.addSubMenu (TRANS ("Instrument track"), buildPluginMenu (available, instrumentMenuBase), hasProject);
            {
                // Instrumente aus installierten Bibliotheken, je Paket ein Untermenü.
                juce::PopupMenu library;
                const auto instruments = services.content.getInstruments();
                std::map<juce::String, juce::PopupMenu> byPackage;

                for (std::size_t i = 0; i < instruments.size(); ++i)
                    byPackage[instruments[i].packageId].addItem (
                        contentMenuBase + static_cast<int> (i),
                        instruments[i].category.isNotEmpty() ? instruments[i].category + " / " + instruments[i].name
                                                             : instruments[i].name);

                for (const auto& package : services.content.getPackages())
                    if (const auto found = byPackage.find (package.id); found != byPackage.end())
                        library.addSubMenu (package.name, found->second);

                if (instruments.empty())
                    library.addItem (-1, TRANS ("No library installed yet (Studio > Content Manager)"), false);

                menu.addSubMenu (TRANS ("Instrument from library"), library, hasProject);
            }
            menu.addSubMenu (TRANS ("Rack instrument (multi-out, several MIDI tracks)"),
                             buildPluginMenu (available, rackMenuBase),
                             hasProject,
                             makeIconDrawable (Icon::connections, theme::textPrimary));
            addIconItem (menu, itemExternalMidi, Icon::midiLearn, TRANS ("MIDI track (external device)"), hasProject);
            addIconItem (
                menu, itemSamplerFromFile, Icon::addInstrumentTrack, TRANS ("Sampler track from audio file..."), hasProject);
            addIconItem (menu, itemSamplerKit, Icon::addInstrumentTrack, TRANS ("Sampler track (empty drum kit)"), hasProject);
            menu.addItem (itemGroup, TRANS ("Group track"), hasProject);
            menu.addItem (itemFx, TRANS ("FX track (sends)"), hasProject);
            menu.addItem (itemFolder, TRANS ("Folder track"), hasProject);
            menu.addSeparator();
            {
                const bool hasSelection = arrangeView.getSelectedTrack() != nullptr;
                menu.addItem (itemDuplicateTrack, TRANS ("Duplicate track"), hasSelection);
                menu.addItem (itemHideTrack, TRANS ("Hide track"), hasSelection);
                menu.addItem (itemDeleteTrack, TRANS ("Delete track"), hasSelection);
                juce::PopupMenu visibility;
                addTrackVisibilityItems (visibility);
                menu.addSubMenu (TRANS ("Show/hide tracks"), visibility, hasProject);
            }
            menu.addSeparator();
            menu.addItem (itemMidiClip, TRANS ("Insert empty MIDI clip at playhead"), hasProject);
            menu.addSeparator();
            menu.addItem (itemAutomationRead, TRANS ("Read automation"), hasProject, services.automation.isReadEnabled());
            menu.addItem (
                itemAutomationWrite, TRANS ("Write automation"), hasProject, services.automation.isWriteEnabled());
            break;
        }

        case viewMenu:
            menu.addItem (itemInspector,
                          keyBindings.menuText (TRANS ("Inspector"), Command::inspector),
                          true,
                          inspectorViewport.isVisible());
            menu.addItem (itemLowerZone, TRANS ("Lower zone"), true, lowerZone.isVisible());
            menu.addSeparator();
            menu.addItem (itemMixer,
                          keyBindings.menuText (TRANS ("MixConsole"), Command::mixer),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 0);
            menu.addItem (itemKeyboard,
                          keyBindings.menuText (TRANS ("On-screen keyboard"), Command::keyboard),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 1);
            menu.addItem (itemKeyEditor,
                          keyBindings.menuText (TRANS ("Key Editor"), Command::keyEditor),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 2);
            menu.addItem (itemMediaBrowser,
                          keyBindings.menuText (TRANS ("Media browser"), Command::mediaBrowser),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 3);
            menu.addItem (itemSampleEditor,
                          keyBindings.menuText (TRANS ("Sample Editor"), Command::sampleEditor),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 4);
            menu.addItem (itemDrumEditor,
                          keyBindings.menuText (TRANS ("Drum Editor"), Command::drumEditor),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 5);
            menu.addItem (itemListEditor,
                          keyBindings.menuText (TRANS ("List Editor"), Command::listEditor),
                          true,
                          lowerZone.isVisible() && lowerZone.getCurrentTab() == 6);
            addIconItem (menu,
                         itemInstrumentBrowser,
                         Icon::star,
                         keyBindings.menuText (TRANS ("Instrument browser"), Command::instrumentBrowser),
                         true,
                         lowerZone.isVisible() && lowerZone.getCurrentTab() == 7);
            menu.addSeparator();
            addIconItem (menu,
                         itemOverview,
                         Icon::overview,
                         keyBindings.menuText (TRANS ("Overview bar"), Command::overview),
                         hasProject,
                         arrangeView.isOverviewShown());
            addIconItem (menu,
                         itemZoomProject,
                         Icon::zoomFit,
                         keyBindings.menuText (TRANS ("Show whole project"), Command::zoomProject),
                         hasProject);
            addIconItem (menu,
                         itemZoomSelection,
                         Icon::zoomFit,
                         keyBindings.menuText (TRANS ("Zoom to selection"), Command::zoomSelection),
                         hasProject);
            addIconItem (
                menu, itemZoomIn, Icon::plus, keyBindings.menuText (TRANS ("Zoom in"), Command::zoomIn), hasProject);
            addIconItem (menu,
                         itemZoomOut,
                         Icon::minus,
                         keyBindings.menuText (TRANS ("Zoom out"), Command::zoomOut),
                         hasProject);
            break;

        case transportMenu:
            menu.addItem (itemPlay, keyBindings.menuText (TRANS ("Play / Stop"), Command::play), hasProject);
            menu.addItem (itemRecord, keyBindings.menuText (TRANS ("Record"), Command::record), hasProject);
            menu.addItem (
                itemReturnToStart, keyBindings.menuText (TRANS ("Go to start"), Command::returnToStart), hasProject);
            menu.addItem (
                itemRewind, keyBindings.menuText (TRANS ("Rewind (one bar)"), Command::rewindBar), hasProject);
            menu.addItem (
                itemForward, keyBindings.menuText (TRANS ("Forward (one bar)"), Command::forwardBar), hasProject);
            menu.addSeparator();
            menu.addItem (
                itemJumpLeft, keyBindings.menuText (TRANS ("Go to left locator"), Command::jumpLeftLocator), hasProject);
            menu.addItem (itemJumpRight,
                          keyBindings.menuText (TRANS ("Go to right locator"), Command::jumpRightLocator),
                          hasProject);
            menu.addItem (itemSetLeft,
                          keyBindings.menuText (TRANS ("Set left locator to playhead"), Command::setLeftLocator),
                          hasProject);
            menu.addItem (itemSetRight,
                          keyBindings.menuText (TRANS ("Set right locator to playhead"), Command::setRightLocator),
                          hasProject);
            menu.addItem (itemPunch,
                          keyBindings.menuText (TRANS ("Punch In/Out"), Command::togglePunch),
                          hasProject,
                          services.transport.isPunchEnabled());
            menu.addSeparator();
            menu.addItem (itemPreviousMarker,
                          keyBindings.menuText (TRANS ("Previous marker"), Command::previousMarker),
                          hasProject);
            menu.addItem (
                itemNextMarker, keyBindings.menuText (TRANS ("Next marker"), Command::nextMarker), hasProject);
            menu.addItem (itemAddMarker, keyBindings.menuText (TRANS ("Add marker"), Command::addMarker), hasProject);
            break;

        case studioMenu:
            menu.addItem (itemAudioSettings, TRANS ("Audio and MIDI settings..."));
            {
                juce::PopupMenu::Item connections (TRANS ("Studio connections..."));
                connections.itemID = itemStudioConnections;
                connections.isEnabled = hasProject;
                connections.image = makeIconDrawable (Icon::connections, theme::textPrimary);
                menu.addItem (std::move (connections));
                addIconItem (menu, itemContentManager, Icon::exportFile, TRANS ("Content Manager (free libraries)..."));
            }
            menu.addItem (itemPluginManager, TRANS ("Plugin manager..."));
            menu.addSeparator();
            menu.addItem (itemMidiLearn, TRANS ("MIDI Learn"), hasProject, services.midiLearn.isLearnActive());
            menu.addItem (itemMidiMappings, TRANS ("MIDI mappings..."), hasProject);
            menu.addSeparator();
            {
                // Sprachnamen stehen in ihrer eigenen Sprache, damit jeder den Weg zurück findet.
                juce::PopupMenu languages;
                const auto current = languageSetting().resolve();
                for (std::size_t i = 0; i < plugins::i18n::allLanguages.size(); ++i)
                {
                    const auto language = plugins::i18n::allLanguages[i];
                    languages.addItem (languageMenuBase + static_cast<int> (i),
                                       plugins::i18n::nativeName (language),
                                       true,
                                       language == current);
                }
                menu.addSubMenu (TRANS ("Language"), languages);
            }
            break;

        case helpMenu:
            menu.addItem (itemExportDiagnostics, TRANS ("Export diagnostic report..."));
            menu.addItem (itemOpenLogFolder, TRANS ("Show log folder"));
            menu.addSeparator();
            addIconItem (menu,
                         itemCheckUpdates,
                         Icon::exportFile,
                         services.updates.isAvailable() ? TRANS ("Check for updates...")
                                                        : TRANS ("Check for updates (developer build without update address)"),
                         services.updates.isAvailable());
            menu.addItem (itemAutoUpdateCheck,
                          TRANS ("Check for updates at startup"),
                          services.updates.isAvailable(),
                          services.updates.isAutomaticCheckEnabled());
            menu.addSeparator();
            // Die DAW ist kostenlos; verkauft werden nur die Pro-Ausgaben der Plugins (ADR 0008).
            addIconItem (menu, itemBuyPro, Icon::plugins, TRANS ("FK plugins: buy Pro..."));
            addIconItem (menu, itemActivatePro, Icon::locked, TRANS ("Activate Pro key (instructions)..."));
            menu.addItem (itemDownloadPage, TRANS ("Open download page..."));
            menu.addSeparator();
            menu.addItem (itemAbout, TRANS ("About FK Audio Suite"));
            break;

        default:
            break;
    }

    return menu;
}

void ProjectComponent::menuItemSelected (int menuItemId, int)
{
    if (menuItemId >= contentMenuBase)
    {
        const auto instruments = services.content.getInstruments();
        const auto index = static_cast<std::size_t> (menuItemId - contentMenuBase);

        if (index < instruments.size())
            if (const auto created =
                    services.sampler.createTrackFromSfz (instruments[index].sfzFile, instruments[index].name);
                ! created)
                showError (services::errorText (created.error()));

        return;
    }

    if (menuItemId >= languageMenuBase)
    {
        chooseLanguage (static_cast<std::size_t> (menuItemId - languageMenuBase));
        return;
    }

    if (menuItemId >= visibilityMenuBase)
    {
        toggleTrackVisibility (menuItemId);
        return;
    }

    if (menuItemId >= templateMenuBase)
    {
        const auto templates = services.templates.getTemplates();
        const auto index = static_cast<std::size_t> (menuItemId - templateMenuBase);

        if (index < templates.size())
            newProject (templates[index]);

        return;
    }

    if (menuItemId >= recentMenuBase)
    {
        openRecentProject (menuItemId);
        return;
    }

    if (menuItemId >= rackMenuBase)
    {
        const auto instruments = services.plugins.getAvailableInstruments();
        const auto index = static_cast<std::size_t> (menuItemId - rackMenuBase);

        if (index < instruments.size())
            if (const auto created =
                    services.racks.createRackInstrument (instruments[index].id, instruments[index].name);
                ! created)
                showError (services::errorText (created.error()));

        return;
    }

    if (menuItemId >= instrumentMenuBase)
    {
        const auto instruments = services.plugins.getAvailableInstruments();
        const auto index = static_cast<std::size_t> (menuItemId - instrumentMenuBase);

        if (index < instruments.size())
        {
            const auto trackNumber = static_cast<int> (services.tracks.getAudioTracks().size()) + 1;

            if (const auto created = services.tracks.createInstrumentTrack (
                    instruments[index].name + " " + juce::String (trackNumber), instruments[index].id);
                ! created)
                showError (services::errorText (created.error()));
        }

        return;
    }

    switch (menuItemId)
    {
        case itemNew:
            newProject();
            break;
        case itemOpen:
            openProject();
            break;
        case itemSave:
            saveProject();
            break;
        case itemSaveAs:
            saveProjectAs();
            break;
        case itemRestoreBackup:
            restoreBackup();
            break;
        case itemProjectSettings:
            showProjectSettings();
            break;
        case itemSaveTemplate:
            saveAsTemplate();
            break;
        case itemEmptyProject:
            newProject();
            break;
        case itemOpenTemplateFolder:
            services.templates.getTemplateFolder().createDirectory();
            services.templates.getTemplateFolder().startAsProcess();
            break;
        case itemExportDiagnostics:
            exportDiagnostics();
            break;
        case itemFolder:
            addFolderTrack();
            break;
        case itemToggleClipMute:
            performClipAction (ClipAction::toggleMute);
            break;
        case itemRangeRemove:
            arrangeView.performRangeAction (ArrangeView::RangeAction::remove);
            break;
        case itemRangeRemoveClose:
            arrangeView.performRangeAction (ArrangeView::RangeAction::removeAndClose);
            break;
        case itemRangeSilence:
            arrangeView.performRangeAction (ArrangeView::RangeAction::insertSilence);
            break;
        case itemRangeSplit:
            arrangeView.performRangeAction (ArrangeView::RangeAction::split);
            break;
        case itemRangeLoop:
            arrangeView.performRangeAction (ArrangeView::RangeAction::loop);
            break;
        case itemNormalize:
            performClipAction (ClipAction::normalize);
            break;
        case itemReverse:
            performClipAction (ClipAction::reverse);
            break;
        case itemInvertPhase:
            performClipAction (ClipAction::invertPhase);
            break;
        case itemNudgeLeft:
        case itemNudgeRight:
            arrangeView.nudgeSelectedClips (menuItemId == itemNudgeRight ? 1 : -1);
            break;
        case itemPlay:
            services.transport.togglePlay();
            break;
        case itemRecord:
            services.transport.record();
            break;
        case itemReturnToStart:
            services.transport.returnToStart();
            break;
        case itemRewind:
            transportBar.stepBars (-1);
            break;
        case itemForward:
            transportBar.stepBars (1);
            break;
        case itemJumpLeft:
        case itemJumpRight:
            transportBar.jumpToLocator (menuItemId == itemJumpLeft);
            break;
        case itemSetLeft:
        case itemSetRight:
            transportBar.setLocatorToPlayhead (menuItemId == itemSetLeft);
            break;
        case itemPunch:
            transportBar.togglePunch();
            break;
        case itemPreviousMarker:
        case itemNextMarker:
        {
            const auto position = services.transport.getPositionSeconds();
            const auto target = menuItemId == itemNextMarker ? services.markers.getNextMarkerSeconds (position)
                                                             : services.markers.getPreviousMarkerSeconds (position);

            if (target)
                services.transport.setPositionSeconds (*target);

            break;
        }
        case itemAddMarker:
            (void)services.markers.addMarker (services.transport.getPositionSeconds());
            break;
        case itemDuplicateTrack:
            if (auto* track = arrangeView.getSelectedTrack())
                if (const auto copy = services.tracks.duplicateTrack (*track); ! copy)
                    showError (services::errorText (copy.error()));
            break;
        case itemHideTrack:
            if (auto* track = arrangeView.getSelectedTrack())
                services.tracks.setHidden (*track, true);
            break;
        case itemDeleteTrack:
            if (auto* track = arrangeView.getSelectedTrack())
                services.tracks.deleteTrack (*track);
            break;
        case itemShowAllTracks:
            for (auto* track : services.tracks.getAudioTracks())
                if (services.tracks.getTrackInfo (*track).hidden)
                    services.tracks.setHidden (*track, false);
            break;
        case itemOpenLogFolder:
            services.diagnostics.getFolders().logs.startAsProcess();
            break;
        case itemExport:
            exportMixdown();
            break;
        case itemQuit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        case itemUndo:
            services.project.undo();
            break;
        case itemCut:
            cutButton.triggerClick();
            break;

        case itemCopy:
            copyButton.triggerClick();
            break;

        case itemPaste:
            pasteButton.triggerClick();
            break;

        case itemDuplicate:
            duplicateButton.triggerClick();
            break;

        case itemGlue:
            glueButton.triggerClick();
            break;

        case itemDeleteClips:
            deleteClipsButton.triggerClick();
            break;

        case itemSelectAllClips:
            arrangeView.selectAllClips();
            break;

        case itemMidiLearn:
            midiLearnButton.triggerClick();
            break;

        case itemMidiMappings:
            showMidiMappings();
            break;

        case itemRedo:
            services.project.redo();
            break;
        case itemAudioMono:
            if (const auto r = services.tracks.createAudioTrack (
                    plugins::i18n::format (
                        TRANS ("Audio {number}"),
                        { { "number", juce::String (services.tracks.getAudioTracks().size() + 1) } }),
                    1);
                ! r)
                showError (services::errorText (r.error()));
            break;
        case itemAudioStereo:
            if (const auto r = services.tracks.createAudioTrack (
                    plugins::i18n::format (
                        TRANS ("Audio {number}"),
                        { { "number", juce::String (services.tracks.getAudioTracks().size() + 1) } }),
                    2);
                ! r)
                showError (services::errorText (r.error()));
            break;
        case itemGroup:
            addGroupTrack();
            break;
        case itemMidiClip:
            insertMidiClipAtCursor();
            break;
        case itemImportAudio:
            importAudioFile();
            break;
        case itemAutomationRead:
            services.automation.setReadEnabled (! services.automation.isReadEnabled());
            break;
        case itemAutomationWrite:
            services.automation.setWriteEnabled (! services.automation.isWriteEnabled());
            break;
        case itemInspector:
            inspectorButton.triggerClick();
            break;
        case itemLowerZone:
            lowerZoneButton.triggerClick();
            break;
        case itemMixer:
            showLowerZoneTab (0);
            break;
        case itemKeyboard:
            showLowerZoneTab (1);
            break;
        case itemKeyEditor:
            showLowerZoneTab (2);
            break;
        case itemMediaBrowser:
            showLowerZoneTab (3);
            break;
        case itemSampleEditor:
            showLowerZoneTab (4);
            break;
        case itemDrumEditor:
            showLowerZoneTab (5);
            break;
        case itemListEditor:
            showLowerZoneTab (6);
            break;
        case itemInstrumentBrowser:
            showLowerZoneTab (7);
            break;
        case itemAudioSettings:
            showAudioSettings();
            break;
        case itemHistory:
            UndoHistoryPanel::show (services.project);
            break;
        case itemSamplerKit:
            if (const auto created = services.sampler.createSamplerTrack(); ! created)
                showError (services::errorText (created.error()));
            break;
        case itemSamplerFromFile:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                TRANS ("Sample for the sampler track"), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            chooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeThis = juce::Component::SafePointer<ProjectComponent> (this),
                 chooser] (const juce::FileChooser& fc)
                {
                    if (safeThis == nullptr || fc.getResult() == juce::File())
                        return;

                    if (const auto created = safeThis->services.sampler.createSamplerTrack (fc.getResult()); ! created)
                        safeThis->showError (services::errorText (created.error()));
                });
            break;
        }
        case itemExternalMidi:
        {
            const auto trackNumber = static_cast<int> (services.tracks.getAudioTracks().size()) + 1;

            if (const auto created = services.externalMidi.createMidiTrack (
                    plugins::i18n::format (TRANS ("MIDI {number}"), { { "number", juce::String (trackNumber) } }));
                ! created)
                showError (services::errorText (created.error()));

            break;
        }
        case itemShortcuts:
            ShortcutEditor::show (keyBindings);
            break;
        case itemOverview:
            toggleOverview();
            break;
        case itemZoomProject:
            arrangeView.zoomToProject();
            break;
        case itemZoomSelection:
            if (! arrangeView.zoomToSelection())
                showError (TRANS ("Nothing selected: select clips or a range first."));
            break;
        case itemZoomIn:
        case itemZoomOut:
            arrangeView.zoomStep (menuItemId == itemZoomIn);
            break;
        case itemStudioConnections:
            StudioConnectionsDialog::show (services.tracks);
            break;
        case itemCheckUpdates:
            checkForUpdates (true);
            break;
        case itemBuyPro:
            openWebsite (localisedPath ("pricing/"));
            break;
        case itemActivatePro:
            openWebsite (localisedPath ("activate/"));
            break;
        case itemDownloadPage:
            openWebsite (localisedPath ("download/"));
            break;
        case itemAutoUpdateCheck:
            services.updates.setAutomaticCheckEnabled (! services.updates.isAutomaticCheckEnabled());
            break;
        case itemContentManager:
            ContentManagerDialog::show (services.content, [this] { instrumentBrowser.refresh(); });
            break;
        case itemPluginManager:
            showPluginManager();
            break;
        case itemAbout:
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "FK Audio Suite",
                                                    plugins::i18n::format (
                                                        TRANS ("Open DAW for Linux and Windows.\n\nVersion {version} ({channel})\nBuild {sha} from {date}, {platform}\n\nGPLv3 - source code on GitHub: FelixKempin/fk-audiosuite"),
                                                        { { "version", juce::String (core::buildInfo::version) },
                                                          { "channel", juce::String (core::buildInfo::channel) },
                                                          { "sha", juce::String (core::buildInfo::gitSha) },
                                                          { "date", juce::String (core::buildInfo::buildDate) },
                                                          { "platform", juce::String (core::buildInfo::platform) } })
                                                        + "\n\n" + trademarkNotice());
            break;
        default:
            break;
    }
}

void ProjectComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Audio settings");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (new AudioSettingsComponent (
        services.engineContext, services.midi)); // JUCE-API verlangt hier einen besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void ProjectComponent::showPluginManager()
{
    if (pluginManagerWindow != nullptr)
    {
        pluginManagerWindow->toFront (true);
        return;
    }

    pluginManagerWindow = std::make_unique<PluginManagerWindow> (services.plugins);
    pluginManagerWindow->onClosed = [this] { pluginManagerWindow.reset(); };
}

void ProjectComponent::performBake (services::TrackHandle* track, BakeAction action, int insertCount)
{
    if (track == nullptr)
        return;

    // Rendern blockiert, bis die Datei fertig ist; die Engine zeigt dabei ein Fortschrittsfenster
    // mit Abbrechen (FkUIBehaviour).
    core::Result<void> result;

    switch (action)
    {
        case BakeAction::bakeTrack:
            result = services.bake.bakeTrack (*track);
            break;
        case BakeAction::bakeInserts:
            result = services.bake.bakeInserts (*track, insertCount);
            break;
        case BakeAction::unbake:
            result = services.bake.unbake (*track);
            break;
    }

    if (! result && result.error().code != core::ErrorCode::cancelled)
        showError (services::errorText (result.error()));
}

void ProjectComponent::showError (const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, TRANS ("Error"), message);
}

void ProjectComponent::selectTool (Tool tool)
{
    arrangeView.setTool (tool);
}

void ProjectComponent::toggleInspector()
{
    inspectorButton.triggerClick();
}

void ProjectComponent::midiLearnChanged()
{
    midiLearnButton.setToggleState (services.midiLearn.isLearnActive(), juce::dontSendNotification);
    midiLearnHint.setText (services.midiLearn.getLearnHint(), juce::dontSendNotification);
    midiLearnHint.setVisible (services.midiLearn.isLearnActive());
    resized();
}

void ProjectComponent::showMidiMappings()
{
    // Liste aller gelernten Zuordnungen, je Zeile ein Entfernen-Button; leer erklärt den Weg.
    class MappingsList final : public juce::Component, private services::MidiLearnService::Listener
    {
    public:
        explicit MappingsList (services::MidiLearnService& service)
            : learn (service)
            , clearButton (TRANS ("Remove all"))
        {
            hint.setJustificationType (juce::Justification::centredTop);
            hint.setColour (juce::Label::textColourId, theme::textSecondary);
            clearButton.onClick = [this] { learn.removeAllMappings(); };
            addAndMakeVisible (hint);
            addAndMakeVisible (clearButton);
            learn.addListener (*this);
            rebuild();
            setSize (460, 320);
        }

        ~MappingsList() override { learn.removeListener (*this); }

        void resized() override
        {
            auto area = getLocalBounds().reduced (theme::spacingLarge);
            clearButton.setBounds (area.removeFromBottom (26).removeFromRight (130));
            area.removeFromBottom (theme::spacingMedium);

            if (rows.empty())
            {
                hint.setBounds (area);
                return;
            }

            for (auto& row : rows)
            {
                auto line = area.removeFromTop (26);
                row.remove->setBounds (line.removeFromRight (26));
                line.removeFromRight (theme::spacingSmall);
                row.label->setBounds (line);
                area.removeFromTop (2);
            }
        }

    private:
        struct Row
        {
            std::unique_ptr<juce::Label> label;
            std::unique_ptr<IconButton> remove;
        };

        void midiLearnChanged() override { rebuild(); }

        void rebuild()
        {
            rows.clear();

            for (const auto& mapping : learn.getMappings())
            {
                Row row;
                row.label = std::make_unique<juce::Label> (
                    juce::String(), mapping.controllerName + utf8 ("  \u2192  ") + mapping.parameterName);
                row.remove = std::make_unique<IconButton> (Icon::trash, TRANS ("Remove this mapping"));
                row.remove->onClick = [this, index = mapping.index]
                {
                    if (const auto result = learn.removeMapping (index); ! result)
                        juce::Logger::writeToLog ("MIDI-Zuordnung: " + services::errorText (result.error()));
                };
                addAndMakeVisible (*row.label);
                addAndMakeVisible (*row.remove);
                rows.push_back (std::move (row));
            }

            hint.setText (TRANS ("No mappings yet.\n\nSwitch on MIDI Learn in the toolbar, touch a control in the DAW (plugin window or mixer), then move the control or pad on your controller. The mapping is saved with the project."),
                          juce::dontSendNotification);
            hint.setVisible (rows.empty());
            clearButton.setEnabled (! rows.empty());
            resized();
        }

        services::MidiLearnService& learn;
        juce::Label hint;
        juce::TextButton clearButton;
        std::vector<Row> rows;
    };

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("MIDI mappings");
    options.dialogBackgroundColour = theme::backgroundPanel;
    options.content.setOwned (new MappingsList (services.midiLearn)); // JUCE-API verlangt einen besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void ProjectComponent::performClipAction (ClipAction action)
{
    arrangeView.performClipAction (action);
    updateToolbar(); // Zwischenablage und Auswahl haben sich geändert
}

void ProjectComponent::toggleLowerZone()
{
    lowerZoneButton.triggerClick();
}

void ProjectComponent::updateToolbar()
{
    const bool hasProject = services.project.hasProject();

    saveButton.setEnabled (hasProject);
    saveAsButton.setEnabled (hasProject);
    settingsButton.setEnabled (hasProject);
    exportButton.setEnabled (hasProject);
    addTrackButton.setEnabled (hasProject);
    addInstrumentButton.setEnabled (hasProject);
    addGroupButton.setEnabled (hasProject);
    addFxButton.setEnabled (hasProject);
    addFolderButton.setEnabled (hasProject);
    undoButton.setEnabled (hasProject);
    redoButton.setEnabled (hasProject);
    cutButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::cut));
    copyButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::copy));
    pasteButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::paste));
    duplicateButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::duplicate));
    glueButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::merge));
    deleteClipsButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::remove));
    nudgeLeftButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::remove));
    nudgeRightButton.setEnabled (hasProject && arrangeView.canPerformClipAction (ClipAction::remove));
    projectNameLabel.setText (hasProject ? services.project.getProjectName() : TRANS ("No project: New or Open"),
                              juce::dontSendNotification);
}

} // namespace fk::ui
