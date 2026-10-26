#pragma once

#include "fk/services/ClipService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ClipView.h"
#include "fk/ui/TimeMapping.h"
#include "fk/ui/Tool.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace fk::ui
{

/// Bearbeitungsaktionen für die Clip-Auswahl; Buttons, Menü, Kontextmenü und Tastenkürzel rufen dieselbe.
enum class ClipAction
{
    cut,
    copy,
    paste,
    duplicate,
    merge,
    remove,
    toggleMute, ///< stumm, wenn einer der Auswahl hörbar ist; sonst alle hörbar
    normalize,  ///< Clip-Gain auf Spitze 0 dBFS (nicht destruktiv)
    reverse,    ///< rückwärts abspielen ein/aus
    invertPhase ///< Phase umkehren (neue Datei)
};

/// Gemeinsame Einstellungen aller Spuren, vom ArrangeView gesetzt. Ein Wert, keine Referenz:
/// Jede Spur bekommt eine Kopie und muss nicht wissen, woher sie kommt.
struct LaneSettings
{
    TimeMapping mapping;
    Tool tool = Tool::select;
    services::GridResolution gridResolution = services::GridResolution::beat();
    bool snapEnabled = true;
};

/// Der Zeitachsen-Teil einer Spur: Raster plus Clips, inklusive Mausbedienung je Werkzeug.
class TrackLane final : public juce::Component, public juce::SettableTooltipClient
{
public:
    /// Wird bei jedem Klick in die Spur gerufen (Spur auswählen), unabhängig vom Werkzeug.
    std::function<void()> onSelected;

    /// Doppelklick auf einen MIDI-Clip: im Key Editor öffnen.
    std::function<void (const juce::String& clipId)> onOpenMidiClip;

    /// Doppelklick auf einen Audio-Clip: im Sample-Editor öffnen.
    std::function<void (const juce::String& clipId)> onOpenAudioClip;

    /// Klick auf einen Clip mit dem Auswahlwerkzeug (auch Rechtsklick); die Modifier entscheiden,
    /// ob die Auswahl ersetzt (keine), umgeschaltet (Strg) oder erweitert (Shift) wird.
    std::function<void (const juce::String& clipId, juce::ModifierKeys mods)> onClipClicked;

    /// Klick ins Leere mit dem Auswahlwerkzeug: Auswahl aufheben.
    std::function<void()> onEmptyClicked;

    /// Clip losgelassen nach dem Ziehen: um deltaSeconds verschieben, Zielzeile aus screenY.
    /// Ohne diesen Rückruf verschiebt die Spur den Clip selbst (nur auf derselben Spur).
    std::function<void (const juce::String& clipId, double deltaSeconds, int screenY)> onClipMoveRequested;

    /// Während des Ziehens: Vorschau der Zielzeile (screenY) und der neuen Zeit.
    std::function<void (double newStartSeconds, int screenY)> onClipDragPreview;

    /// Eintrag im Kontextmenü des Clips gewählt (wirkt auf die Auswahl).
    std::function<void (ClipAction action)> onClipAction;

    /// Eine Aktion ging nicht; der Text ist für den Benutzer gedacht (Projektfenster zeigt ihn).
    std::function<void (const juce::String& message)> onError;

    /// Aufziehen über mehrere Spuren: Lasso (Auswahlwerkzeug auf leerer Fläche) oder Bereich
    /// (Bereichswerkzeug). Das Arrangement zeichnet und wertet aus; die Spur meldet nur den Zeiger.
    std::function<void (juce::Point<int> screenPosition, juce::ModifierKeys mods)> onMarqueeStart;
    std::function<void (juce::Point<int> screenPosition, juce::ModifierKeys mods)> onMarqueeDrag;
    std::function<void (juce::Point<int> screenPosition, juce::ModifierKeys mods)> onMarqueeEnd;

    /// Rechtsklick mit dem Bereichswerkzeug: Menü für den Bereich.
    std::function<void (juce::Point<int> screenPosition)> onRangeMenu;

    /// Lupe: um diese Zeit vergrößern (zoomIn) oder verkleinern.
    std::function<void (double seconds, bool zoomIn)> onZoomRequested;

    TrackLane (services::TrackService& trackService,
               services::ClipService& clipService,
               services::MidiClipService& midiClipService,
               services::TimelineService& timelineService,
               services::WaveformService& waveformService,
               services::TrackHandle& track,
               juce::Colour trackColour);

    void setSettings (LaneSettings newSettings);

    /// Zeigt den Bereich der laufenden Aufnahme (Start bis jetzt); leer = keine Aufnahme auf dieser Spur.
    void setRecordingPreview (std::optional<std::pair<double, double>> rangeSeconds);

    /// Liest die Clips neu aus dem Service und baut die Ansicht auf.
    void refresh();

    /// Markiert die Clips mit diesen Kennungen als ausgewählt (alle anderen nicht).
    void setSelectedClips (const std::set<juce::String>& clipIds);

    /// Kennung des Clips unter der x-Position (leer, wenn keiner).
    [[nodiscard]] juce::String getClipIdAt (int x) const;

    /// Zusätzliche Höhe unter den Clips für aufgeklappte Take-Lanes (0 ohne Takes).
    [[nodiscard]] int getExtraHeight() const;
    static constexpr int takeLaneHeight = 18;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    bool marqueeActive = false; ///< Lasso oder Bereich wird gerade aufgezogen
    /// Laufende Mausaktion mit dem Auswahlwerkzeug: Clip verschieben, Rand trimmen oder Fade ziehen.
    struct DragState
    {
        enum class Mode
        {
            move,
            trimLeft,
            trimRight,
            fadeIn,
            fadeOut
        };

        ClipView* clip = nullptr; // geliehen, Element von clipViews
        Mode mode = Mode::move;
        double originalStartSeconds = 0.0;
        double originalLengthSeconds = 0.0;
        double originalOffsetSeconds = 0.0;
        double fadeInSeconds = 0.0;
        double fadeOutSeconds = 0.0;
        bool moved = false;   // erst nach echter Bewegung wird das Modell geändert
        bool stretch = false; ///< Alt an der rechten Kante: strecken statt trimmen (Time-Stretch)
    };

    /// Breite der Trim-Zone an beiden Clip-Rändern in Pixeln.
    static constexpr int edgeZone = 6;
    [[nodiscard]] DragState::Mode modeAt (const ClipView& clip, juce::Point<int> local) const;
    [[nodiscard]] std::pair<double, double> trimmedRange (const DragState& state, double deltaSeconds) const;

    void layoutClips();
    void createMidiClipAt (int x);
    void createMidiClip (double startSeconds, double endSeconds);
    void showClipMenu (ClipView& clip, juce::Point<int> screenPosition);
    void showClipGainEditor (ClipView& clip, juce::Rectangle<int> screenArea);
    void showStretchEditor (const juce::String& clipId, juce::Rectangle<int> screenArea);
    void addTakeFromFile (const juce::String& clipId);
    void paintTakeLanes (juce::Graphics& g);
    [[nodiscard]] int clipAreaHeight() const;

    /// Laufende Auswahl eines Bereichs in einer Take-Lane (Comping: Take für Bereich übernehmen).
    struct TakeDrag
    {
        juce::String clipId;
        int takeIndex = 0;
        double fromSeconds = 0.0;
        double toSeconds = 0.0;
    };
    std::optional<TakeDrag> takeDrag;
    [[nodiscard]] ClipView* findClipAt (int x) const;
    [[nodiscard]] double snapped (double seconds) const;
    void reportError (const core::Error& error) const;

    services::TrackService& tracks;
    services::ClipService& clips;
    services::MidiClipService& midiClips;
    services::TimelineService& timeline;
    services::WaveformService& waveforms;
    services::TrackHandle& track;
    juce::Colour colour;
    LaneSettings settings;
    std::vector<std::unique_ptr<ClipView>> clipViews;
    std::optional<DragState> drag;
    std::optional<std::pair<double, double>> recordingPreview;

    /// Stift: MIDI-Clip wird aufgezogen (Anfang, aktuelles Ende), Vorschau bis zum Loslassen.
    std::optional<std::pair<double, double>> drawPreview;

    /// Gebacken (selbst oder über eine Gruppe): Clips abgedunkelt mit Hinweis, leer = bearbeitbar.
    juce::String bakedHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLane)
};

} // namespace fk::ui
