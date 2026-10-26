#pragma once

#include "fk/engine/EngineContext.h"
#include "fk/services/AutomationService.h"
#include "fk/services/AutosaveService.h"
#include "fk/services/BakeService.h"
#include "fk/services/ClipService.h"
#include "fk/services/DiagnosticsService.h"
#include "fk/services/ExportService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/ContentService.h"
#include "fk/services/DrumMapService.h"
#include "fk/services/ExpressionMapService.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/LibraryService.h"
#include "fk/services/RackService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/UpdateService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/MidiLearnService.h"
#include "fk/services/MidiService.h"
#include "fk/services/MixerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/PreviewService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/SendService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TemplateService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ArrangeView.h"
#include "fk/ui/Commands.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/Inspector.h"
#include "fk/ui/KeyEditor.h"
#include "fk/ui/LowerZone.h"
#include "fk/ui/MediaBrowser.h"
#include "fk/ui/MixConsole.h"
#include "fk/ui/PluginManagerWindow.h"
#include "fk/ui/Theme.h"
#include "fk/ui/Tool.h"
#include "fk/ui/TransportBar.h"
#include "fk/ui/ZoneSplitter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>
#include <memory>

namespace fk::ui
{

/// Alle Services, die die Oberfläche braucht, als ein Bündel. Spart lange Parameterlisten
/// (CODING_GUIDELINES.md §9: mehr als vier Parameter → struct).
struct UiServices
{
    engine::EngineContext& engineContext;
    services::ProjectService& project;
    services::TransportService& transport;
    services::TimelineService& timeline;
    services::TrackService& tracks;
    services::ClipService& clips;
    services::WaveformService& waveforms;
    services::PluginService& plugins;
    services::MixerService& mixer;
    services::MidiService& midi;
    services::MidiClipService& midiClips;
    services::MidiControllerService& midiControllers;
    services::NotePreviewService& notePreview;
    services::BakeService& bake;
    services::ExportService& exporter;
    services::SendService& sends;
    services::AutomationService& automation;
    services::SignatureService& signature;
    services::PreviewService& preview;
    services::MidiLearnService& midiLearn;
    services::AutosaveService& autosave;
    services::ProjectSettingsService& projectSettings;
    services::TemplateService& templates;
    services::DiagnosticsService& diagnostics;
    services::MarkerService& markers;
    services::RackService& racks;
    services::ExternalMidiService& externalMidi;
    services::DrumMapService& drumMaps;
    services::SamplerService& sampler;
    services::ExpressionMapService& expressionMaps;
    services::ContentService& content;
    services::LibraryService& library;
    services::UpdateService& updates;
};

/// Inhalt des Projektfensters: Werkzeugleiste oben, Inspector links, Projektzone in der Mitte,
/// untere Zone mit Reitern, Transport unten. Jede Funktion hat einen Button mit Symbol und
/// Tooltip; Tastenkürzel sind Zusatz (CODING_GUIDELINES.md §10a).
class ProjectComponent final : public juce::Component,
                               public juce::DragAndDropContainer, // Ziehen aus dem Medien-Browser
                               private services::ProjectService::Listener,
                               private services::MidiLearnService::Listener,
                               private juce::MenuBarModel
{
public:
    explicit ProjectComponent (UiServices services);
    ~ProjectComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    /// Fragt bei ungespeicherten Änderungen nach (Speichern, Nicht speichern, Abbrechen), schließt das
    /// Projekt und ruft dann action. Bei Abbrechen oder Speicherfehler passiert nichts.
    void closeProjectThen (std::function<void()> action);

private:
    /// Menüleiste (juce::MenuBarModel): dieselben Aktionen wie die Buttons, zusätzlich erreichbar.
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemId, int topLevelMenuIndex) override;

    void projectChanged() override;
    void midiLearnChanged() override;
    void showMidiMappings();

    void newProject (const juce::File& templateFile = {});
    void showNewMenu();
    void saveAsTemplate();
    void showProjectSettings();
    void showZoomMenu();
    void saveZoomPreset();
    void toggleOverview();
    void checkForUpdates (bool userAsked);
    /// Speichert die Oberflächensprache für alle FK-Programme (Index in i18n::allLanguages).
    void chooseLanguage (std::size_t index);
    /// Seite der Website im Browser (Preise, Aktivierung, Download); path beginnt mit /.
    void openWebsite (const juce::String& path);

    /// Führt einen Befehl der Tastenbelegung aus; dieselben Wege wie Buttons und Menüs.
    bool performCommand (Command command);

    /// Tooltips der Buttons nennen die aktuelle Taste (ändert sich im Editor Tastaturbefehle).
    void updateShortcutTooltips();
    void checkDeviceSampleRate();
    void exportDiagnostics();
    void openProject();
    void openProjectFile (const juce::File& editFile);
    void saveProject();
    void saveProjectAs();
    void showRecentMenu();
    void showTrackVisibilityMenu();
    void addTrackVisibilityItems (juce::PopupMenu& menu);
    void toggleTrackVisibility (int menuItemId);
    void restoreBackup();
    void addRecentProjectsTo (juce::PopupMenu& menu);
    void openRecentProject (int menuItemId);
    void exportMixdown();
    void addTrack();
    void addInstrumentTrack();
    void addGroupTrack();
    void addFxTrack();
    void addFolderTrack();
    void insertMidiClipAtCursor();
    void openMidiClip (const juce::String& clipId);
    void importAudioFile();
    void showLowerZoneTab (int tab);
    void showAudioSettings();
    void showPluginManager();
    void showError (const juce::String& message);
    void performBake (services::TrackHandle* track, BakeAction action, int insertCount = -1);
    void selectTool (Tool tool);
    void performClipAction (ClipAction action);
    void toggleInspector();
    void toggleLowerZone();
    void updateToolbar();

    /// Ein Platz in der Werkzeugleiste: Breite mit und ohne Beschriftung, neue Gruppe davor.
    struct ToolbarItem
    {
        juce::Component* component = nullptr;
        int width = 32;
        int compactWidth = 32;
        bool startsGroup = false;
    };
    [[nodiscard]] std::vector<ToolbarItem> getToolbarItems();

    /// Legt die Werkzeugleiste aus und liefert ihre Höhe: erst kompakt (nur Symbole), dann umbrechen.
    int layoutToolbar (juce::Rectangle<int> area);

    UiServices services;
    std::vector<services::GridResolution> gridChoices; ///< Einträge der Rasterwahl, ID = Index + 1
    juce::File lastSampleRateCheck;                    ///< Projekt, für das die Raten-Frage schon kam
    int toolbarHeight = theme::toolbarHeight;          ///< wächst, wenn die Leiste umbricht
    services::ShortcutService shortcutStore { services.engineContext };
    services::StepInputService stepInput { services.project, services.midiClips };
    KeyBindings keyBindings { shortcutStore };
    std::map<juce::Component*, juce::String> tooltipBases; ///< Tooltip ohne Taste, für updateShortcutTooltips

    // Tooltips brauchen genau ein Fenster pro Anwendung; es hängt am Desktop.
    juce::TooltipWindow tooltipWindow { nullptr, 500 };

    IconButton newButton;
    IconButton openButton;
    IconButton saveButton;
    IconButton saveAsButton;
    IconButton recentButton;
    IconButton settingsButton;
    IconButton exportButton;
    IconButton addTrackButton;
    IconButton addInstrumentButton;
    IconButton addGroupButton;
    IconButton addFxButton;
    IconButton addFolderButton;
    IconButton undoButton;
    IconButton redoButton;
    IconButton historyButton;
    IconButton zoomMenuButton;
    IconButton overviewButton;
    IconButton cutButton;
    IconButton copyButton;
    IconButton pasteButton;
    IconButton duplicateButton;
    IconButton glueButton;
    IconButton deleteClipsButton;
    IconButton midiLearnButton;
    juce::Label midiLearnHint;
    IconButton audioSettingsButton;
    IconButton pluginManagerButton;

    juce::MenuBarComponent menuBar;
    IconButton selectToolButton;
    IconButton drawToolButton;
    IconButton splitToolButton;
    IconButton eraseToolButton;
    IconButton glueToolButton;
    IconButton zoomToolButton;
    IconButton muteToolButton;
    IconButton rangeToolButton;
    IconButton nudgeLeftButton;
    IconButton nudgeRightButton;
    juce::ComboBox gridBox;
    IconButton snapButton;
    IconButton tracksSmallerButton;
    IconButton tracksLargerButton;
    IconButton trackVisibilityButton;
    IconButton inspectorButton;
    IconButton lowerZoneButton;
    juce::Label projectNameLabel;

    Inspector inspector;
    juce::Viewport inspectorViewport; ///< Inspector rollbar, wenn das Fenster zu niedrig ist
    ArrangeView arrangeView;
    MixConsole mixConsole;
    VirtualKeyboard virtualKeyboard;
    KeyEditor keyEditor;
    MediaBrowser mediaBrowser;
    SampleEditor sampleEditor;
    DrumEditor drumEditor;
    ListEditor listEditor;
    InstrumentBrowser instrumentBrowser;
    LowerZone lowerZone;
    ZoneSplitter lowerZoneSplitter;
    int lowerZoneHeight = theme::mixerHeight;
    int lowerZoneHeightAtDragStart = theme::mixerHeight;
    TransportBar transportBar;
    std::unique_ptr<PluginManagerWindow> pluginManagerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectComponent)
};

} // namespace fk::ui
