#pragma once

#include "fk/services/AutomationService.h"
#include "fk/services/ClipService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MixerService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/AutomationHeader.h"
#include "fk/ui/AutomationLane.h"
#include "fk/ui/FolderHeader.h"
#include "fk/ui/MarkerTrack.h"
#include "fk/ui/OverviewBar.h"
#include "fk/ui/SignatureTrack.h"
#include "fk/ui/Theme.h"
#include "fk/ui/TimeMapping.h"
#include "fk/ui/TimeRuler.h"
#include "fk/ui/Tool.h"
#include "fk/ui/TrackHeader.h"
#include "fk/ui/TrackLane.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace fk::ui
{

/// Projektzone (UI_CONCEPT.md §2): Lineal oben, darunter pro Spur Kopf und Zeitachse,
/// Positionszeiger darüber, horizontale Bildlaufleiste unten. Zoom mit Strg+Mausrad,
/// horizontal rollen mit Shift+Mausrad.
class ArrangeView final : public juce::Component,
                          public juce::FileDragAndDropTarget,
                          public juce::DragAndDropTarget, // aus dem Medien-Browser
                          private services::TrackService::Listener,
                          private services::AutomationService::Listener,
                          private services::TimelineService::Listener,
                          private juce::ScrollBar::Listener,
                          private juce::Timer
{
public:
    /// Wird gerufen, wenn sich die ausgewählte Spur ändert (nullptr = keine).
    std::function<void (services::TrackHandle*)> onTrackSelected;

    /// Doppelklick auf einen MIDI-Clip: im Key Editor öffnen.
    std::function<void (const juce::String& clipId)> onOpenMidiClip;

    /// Doppelklick auf einen Audio-Clip (Spur und Clip), für den Sample-Editor.
    std::function<void (services::TrackHandle* track, const juce::String& clipId)> onOpenAudioClip;

    /// Button im Spurkopf: Plugin-Fenster des Spurinstruments öffnen.
    std::function<void (services::TrackHandle* track)> onOpenInstrument;

    /// Bake-Menü im Spurkopf: backen oder lösen (das Projektfenster ruft den BakeService).
    std::function<void (services::TrackHandle* track, BakeAction action)> onBakeRequested;

    /// Clip-Auswahl hat sich geändert (Buttons im Projektfenster aktivieren/deaktivieren).
    std::function<void()> onClipSelectionChanged;

    /// Eine Clip-Aktion ist fehlgeschlagen; der Text ist für den Benutzer gedacht.
    std::function<void (const juce::String& message)> onError;

    /// Alle Services, die das Arrangement braucht (CODING_GUIDELINES.md §9: mehr als vier Parameter → struct).
    struct Services
    {
        services::TrackService& tracks;
        services::ClipService& clips;
        services::MidiClipService& midiClips;
        services::TransportService& transport;
        services::TimelineService& timeline;
        services::WaveformService& waveforms;
        services::MixerService& mixer;
        services::AutomationService& automation;
        services::SignatureService& signature;
        services::MarkerService& markers;
    };

    explicit ArrangeView (Services services);
    ~ArrangeView() override;

    [[nodiscard]] services::TrackHandle* getSelectedTrack() const noexcept;

    // Clip-Auswahl (UI_CONCEPT.md §Clip-Auswahl). Kennungen statt Zeiger: Ein Umbau der Zeilen
    // ändert die Auswahl nicht; gelöschte Clips fallen beim nächsten Umbau heraus.
    [[nodiscard]] std::vector<juce::String> getSelectedClipIds() const;
    void selectClip (const juce::String& clipId, juce::ModifierKeys mods);
    void selectAllClips();
    void clearClipSelection();

    /// Führt eine Aktion auf der Auswahl aus; Einfügen zielt auf Positionszeiger und gewählte Spur.
    void performClipAction (ClipAction action);
    [[nodiscard]] bool canPerformClipAction (ClipAction action) const;

    void setTool (Tool tool);
    [[nodiscard]] TimeMapping getTimeMapping() const noexcept;

    /// Spurhöhe in Pixeln (56 kompakt, 72 normal, 120 groß); Wert wird begrenzt.
    void setTrackHeight (int height);
    [[nodiscard]] int getTrackHeight() const noexcept;
    static constexpr int minTrackHeight = 56;
    static constexpr int maxTrackHeight = 160;

    /// Automationsspur einer Spur auf- oder zuklappen (Button im Spurkopf ruft dasselbe).
    void setAutomationShown (services::TrackHandle* track, bool shown);
    void setGridResolution (services::GridResolution resolution);

    /// Bereichsauswahl (Bereichswerkzeug): Zeitbereich auf einer Gruppe von Spuren.
    struct RangeSelection
    {
        double fromSeconds = 0.0;
        double toSeconds = 0.0;
        std::vector<services::TrackHandle*> tracks;
    };

    enum class RangeAction
    {
        remove,         ///< Material löschen, Lücke bleibt (Entf)
        removeAndClose, ///< löschen und Späteres nachrücken (Shift+Entf)
        insertSilence,  ///< Stille in Bereichslänge einfügen
        split,          ///< an den Grenzen teilen
        loop,           ///< Locatoren auf den Bereich
        clear           ///< Auswahl aufheben
    };

    [[nodiscard]] const std::optional<RangeSelection>& getRangeSelection() const noexcept;
    void performRangeAction (RangeAction action);

    /// Um diese Zeit vergrößern oder verkleinern (Lupe); die Stelle bleibt unter der Maus.
    void zoomAt (double seconds, bool zoomIn);

    /// Zoom und Scroll so, dass from..to genau die Spurbreite füllt.
    void showTimeRange (double fromSeconds, double toSeconds);
    [[nodiscard]] std::pair<double, double> getVisibleRange() const;

    /// Ganzes Projekt zeigen (Cubase: Shift+F); ohne Clips die ersten 16 Takte.
    void zoomToProject();

    /// Auf die Auswahl zoomen (Cubase: Alt+S): Bereich, sonst ausgewählte Clips. false = nichts gewählt.
    bool zoomToSelection();

    /// Ein Schritt rein oder raus um den Positionszeiger (Cubase: H und G).
    void zoomStep (bool zoomIn);

    /// Übersichtsleiste über dem Lineal ein- oder ausblenden.
    void setOverviewShown (bool shouldShow);
    [[nodiscard]] bool isOverviewShown() const noexcept { return overview.isVisible(); }

    /// Ausgewählte Clips um einen Rasterschritt nach links (-1) oder rechts (+1) schieben (Nudge).
    void nudgeSelectedClips (int direction);
    void setSnapEnabled (bool shouldSnap);

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;

    /// Audio- und MIDI-Dateien aus dem Dateimanager: Abwurf legt sie als Clips an der Zeit unter
    /// dem Zeiger auf die Spur unter dem Zeiger; passt die Spurart nicht (Audio auf Audiospur,
    /// MIDI auf Instrumentenspur) oder ist dort keine → neue Spur. Mehrere Dateien und mehrere
    /// Spuren einer MIDI-Datei landen auf aufeinanderfolgenden Spuren (neue werden angelegt).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /// Ziehen aus dem Medien-Browser: Die Beschreibung beginnt mit MediaBrowser::dragDescriptionPrefix
    /// und enthält die Pfade zeilenweise.
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails& details) override;
    void itemDragMove (const SourceDetails& details) override;
    void itemDragExit (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    /// Wie mit dem Tempo einer MIDI-Datei verfahren wird, die vom Projekt abweicht.
    enum class MidiTempo
    {
        ask,   ///< nachfragen (onAskMidiTempo), ohne Rückruf: Projekttempo behalten
        adopt, ///< Tempo und Taktart der Datei ins Projekt übernehmen
        keep   ///< Projekttempo behalten, Noten in Schlägen einfügen
    };

    /// Importiert Dateien wie ein Abwurf an (x, y) in ArrangeView-Koordinaten; auch für Tests.
    void importFiles (const juce::StringArray& files, int x, int y, MidiTempo midiTempo = MidiTempo::ask);

    /// Rückfrage beim Import einer MIDI-Datei mit eigenem Tempo; answer(true) übernimmt es.
    std::function<void (const juce::String& fileName, double fileBpm, std::function<void (bool adopt)> answer)>
        onAskMidiTempo;

    /// Wie importFiles, Zeit = Positionszeiger, Spur = ausgewählte Audiospur (sonst neue).
    void importFilesAtPlayhead (const juce::StringArray& files);
    [[nodiscard]] static bool isAudioFile (const juce::File& file);
    [[nodiscard]] static bool isMidiFile (const juce::File& file);
    [[nodiscard]] static bool isImportableFile (const juce::File& file);

    /// Legt eine Instrumentenspur mit dem Standardinstrument an (für MIDI-Dateien ohne passende
    /// Zielspur). Das Projektfenster setzt den Rückruf, weil nur es die Instrumente kennt.
    std::function<services::TrackHandle*(const juce::String& name)> createInstrumentTrack;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    /// Eine Zeile = Spurkopf + Zeitachse. Reines Datenbündel ohne eigene Logik.
    struct TrackRow
    {
        services::TrackHandle* track = nullptr;
        std::unique_ptr<TrackHeader> header;
        std::unique_ptr<TrackLane> lane;
        /// Eine Automationslage: Kopf links, Kurve rechts; nur sichtbar, wenn aufgeklappt.
        struct Lane
        {
            std::unique_ptr<AutomationHeader> header;
            std::unique_ptr<AutomationLane> lane;
        };
        std::vector<Lane> automationLanes;
        bool automationShown = false;
        int depth = 0; ///< Ordnerebene: Spurkopf wird eingerückt
    };

    /// Eine Ordnerzeile: Ordnerkopf und Clip-Übersicht.
    struct FolderRow
    {
        services::FolderHandle* folder = nullptr;
        std::unique_ptr<FolderHeader> header;
        std::unique_ptr<FolderLane> lane;
        int depth = 0;
    };

    /// Reihenfolge der Zeilen: Ordner und Spuren gemischt; rows bleibt die Liste der Spurzeilen.
    struct RowSlot
    {
        bool isFolder = false;
        std::size_t index = 0; ///< in folderRows oder rows
    };

    /// Wohin eine gezogene Spur oder ein Ordner fällt: vor before (nullptr = ans Ende) oder in into.
    struct ReorderTarget
    {
        services::TrackOrFolderHandle* before = nullptr;
        services::FolderHandle* into = nullptr;
        int lineY = 0;                   ///< Einfügelinie, lokal
        juce::Rectangle<int> folderArea; ///< hervorgehobener Ordner bei "hinein"
    };

    void tracksChanged() override;
    void contentChanged() override;
    void automationChanged() override;
    void timelineChanged() override;
    void scrollBarMoved (juce::ScrollBar* scrollBar, double newRangeStart) override;
    void timerCallback() override;

    void rebuildRows();
    void layoutRows();
    void selectTrack (services::TrackHandle* track);
    void pushClipSelectionToRows();
    void pruneClipSelection();
    void repaintPlayheadAt (int laneX);
    void moveClips (const juce::String& draggedClipId, double deltaSeconds, int screenY, std::size_t sourceRow);
    [[nodiscard]] int rowIndexAtScreenY (int screenY) const;
    [[nodiscard]] ReorderTarget reorderTargetAt (int screenY) const;
    void reorderTrack (services::TrackOrFolderHandle& track, const ReorderTarget& target);
    void marqueeStarted (juce::Point<int> screenPosition);
    void marqueeMoved (juce::Point<int> screenPosition, juce::ModifierKeys mods);
    void marqueeEnded (juce::Point<int> screenPosition, juce::ModifierKeys mods);
    void showRangeMenu (juce::Point<int> screenPosition);
    /// Zeit unter einer x-Position (lokal), am Raster außer mit Shift.
    [[nodiscard]] double secondsAtLocalX (int x, bool snap) const;
    /// Spuren, deren Zeitachse den senkrechten Bereich (lokal) berührt, von oben nach unten.
    [[nodiscard]] std::vector<services::TrackHandle*> tracksBetween (int localY1, int localY2) const;
    void addFolderRow (services::FolderHandle& folder, int depth);
    [[nodiscard]] juce::Component& headerOf (const RowSlot& slot) const;

    /// Spuren, die als Zeile sichtbar sind (ohne ausgeblendete und ohne Inhalt zugeklappter Ordner).
    [[nodiscard]] std::vector<services::TrackHandle*> displayedTracks() const;
    void reportError (const core::Error& error);
    void applySettings();
    void updateScrollBar();
    void updateOverviewContent();
    [[nodiscard]] double getTimelineLengthSeconds() const;
    [[nodiscard]] double getContentEndSeconds() const; ///< Ende des letzten Clips, 0 ohne Clips
    [[nodiscard]] int getLaneWidth() const;

    services::TrackService& tracks;
    services::ClipService& clips;
    services::MidiClipService& midiClips;
    services::TransportService& transport;
    services::TimelineService& timeline;
    services::WaveformService& waveforms;
    services::MixerService& mixer;
    services::AutomationService& automation;
    services::SignatureService& signature;
    std::set<services::TrackHandle*> tracksWithAutomationShown; // Anzeigezustand, nicht im Projekt
    services::TrackHandle* selectedTrack = nullptr;             // nur geliehen, wird bei jedem Umbau neu geprüft
    std::set<juce::String> selectedClips;

    LaneSettings settings;
    OverviewBar overview;
    TimeRuler ruler;
    SignatureTrack signatureTrack;
    MarkerTrack markerTrack;
    juce::Viewport viewport;
    juce::Component rowsContainer;
    juce::ScrollBar horizontalScrollBar { false };
    std::vector<TrackRow> rows;
    std::vector<FolderRow> folderRows;
    std::vector<RowSlot> rowOrder;
    int lastPlayheadX = -1;
    std::optional<ReorderTarget> reorderTarget; ///< beim Ziehen einer Spur oder eines Ordners

    /// Lasso oder Bereich im Aufziehen (lokale Koordinaten).
    struct Marquee
    {
        juce::Point<int> start;
        juce::Point<int> current;
        bool isRange = false;
    };
    std::optional<Marquee> marquee;
    std::optional<RangeSelection> rangeSelection;

    /// Vorschau während des Ziehens: Zielspur (Index, -1 = neue Spur), Zeit und Beschriftung.
    struct DropPreview
    {
        int rowIndex = -1;
        double seconds = 0.0;
        juce::String label;
    };
    std::optional<DropPreview> dropPreview;
    [[nodiscard]] DropPreview previewFor (int x, int y) const;
    [[nodiscard]] DropPreview previewFor (int x, int y, services::TrackKind wantedKind) const;
    int trackHeight = theme::trackHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangeView)
};

} // namespace fk::ui
