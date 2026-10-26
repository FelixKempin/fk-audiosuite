#pragma once

#include "fk/services/ExpressionMapService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/NoteValue.h"
#include "fk/services/SignatureService.h"
#include "fk/services/StepInputService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/ArticulationLane.h"
#include "fk/ui/ControllerLane.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/NoteGrid.h"
#include "fk/ui/Tool.h"
#include "fk/ui/VelocityLane.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Klaviatur links neben dem Notenraster, zeigt die Tonhöhe jeder Zeile.
class PianoKeys final : public juce::Component, public juce::SettableTooltipClient
{
public:
    /// Taste gedrückt (Vorhören) oder beim Gleiten eine andere Taste erreicht.
    std::function<void (int noteNumber)> onKeyDown;
    /// Maustaste losgelassen: klingende Note beenden.
    std::function<void()> onKeysUp;

    PianoKeys();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    [[nodiscard]] int noteNumberAt (int y) const;

    int pressedNote = -1;
};

/// Key Editor (Piano Roll) in der unteren Zone: Werkzeuge, Raster, Quantisieren, Klaviatur, Notenraster.
class KeyEditor final : public juce::Component,
                        private services::MidiClipService::Listener,
                        private services::SignatureService::Listener,
                        private juce::Timer
{
public:
    /// Eine Änderung wurde abgelehnt; der Text ist für den Benutzer (Projektfenster zeigt ihn).
    std::function<void (const juce::String& message)> onError;

    /// Die Services des Key Editors als Bündel (CODING_GUIDELINES.md §9: mehr als vier Parameter).
    struct Services
    {
        services::MidiClipService& midiClips;
        services::MidiControllerService& midiControllers;
        services::NotePreviewService& preview;
        services::TransportService& transport;
        services::TimelineService& timeline;
        services::SignatureService& signature;
        services::StepInputService* stepInput = nullptr;          ///< ohne: keine Step-Eingabe (Tests)
        services::ExpressionMapService* expressionMaps = nullptr; ///< ohne: keine Spielweisen
    };

    /// Für Tests: die Artikulations-Lane.
    [[nodiscard]] ArticulationLane& getArticulationLane() noexcept { return articulationLane; }

    explicit KeyEditor (Services services);
    ~KeyEditor() override;

    /// Zeigt den Clip; leere Kennung = nichts anzeigen.
    void setClip (const juce::String& clipId);
    [[nodiscard]] juce::String getClipId() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    void midiClipChanged() override;
    void timerCallback() override; // Positionszeiger
    void signatureChanged() override;
    void reload();
    void rebuildChordChoices();
    void insertChord();
    void showFunctionsMenu();
    void showQuantiseOptions();
    void toggleStepInput();
    void stepInputChanged();
    void refreshExpressionMap();
    [[nodiscard]] NoteGrid::ChordChoice currentChordChoice() const;
    void selectTool (Tool tool);
    [[nodiscard]] double currentGridBeats() const;
    [[nodiscard]] double currentNoteLengthBeats() const;
    void fillNoteValueBox (juce::ComboBox& box, bool withLinkItem);
    void reportError (const core::Error& error);
    void previewNote (int noteNumber, int velocity);
    void addControllerLane (int controllerType);
    void removeControllerLane (ControllerLane* lane);

    services::MidiClipService& midiClips;
    services::MidiControllerService& midiControllers;
    services::NotePreviewService& preview;
    services::TransportService& transport;
    services::TimelineService& timeline;
    double clipStartBeats = 0.0;
    double clipLengthBeats = 0.0;
    services::SignatureService& signature;
    juce::String clipId;
    services::KeyPointInfo currentKey;

    juce::Label titleLabel;
    IconButton selectToolButton;
    IconButton drawToolButton;
    IconButton eraseToolButton;
    juce::ComboBox gridBox;   ///< Raster: Einrasten und Quantisieren (alle Notenwerte)
    juce::ComboBox lengthBox; ///< Länge neuer Noten; erster Eintrag "wie Raster"
    IconButton quantiseButton;
    IconButton functionsButton; ///< MIDI-Funktionen: Länge, Legato, Transponieren, Humanize, Anschlag
    IconButton quantiseOptionsButton; ///< Stärke, Swing, Groove
    IconButton stepInputButton;       ///< Step-Eingabe ein/aus
    IconButton stepBackButton;        ///< Step-Cursor einen Schritt zurück
    IconButton stepPauseButton;       ///< Step-Cursor weiter ohne Note (Pause)
    services::QuantiseSettings quantiseSettings;
    juce::String grooveName;          ///< Clip, aus dem der Groove stammt (leer = keiner)
    services::StepInputService* stepInput = nullptr;
    services::ExpressionMapService* expressionMaps = nullptr;
    juce::ComboBox expressionMapBox; ///< Expression Map der Spur (Spielweisen)
    std::vector<engine::ExpressionMap> mapChoices;
    juce::String clipTrackId;
    std::vector<services::NoteValue> noteValues = services::NoteValue::all();
    juce::Label keyLabel;
    juce::ComboBox degreeBox;
    juce::ComboBox chordKindBox;
    juce::ComboBox inversionBox;
    IconButton chordButton;
    IconButton addLaneButton;
    IconButton previewButton; ///< Vorhören an/aus (an: Tasten, Setzen und Ziehen klingen)

    /// Viewport, der sein Rollen meldet, damit die Velocity-Lane mitläuft.
    class ScrollReportingViewport final : public juce::Viewport
    {
    public:
        std::function<void (int x)> onScrolled;
        void visibleAreaChanged (const juce::Rectangle<int>& newVisibleArea) override
        {
            if (onScrolled)
                onScrolled (newVisibleArea.getX());
        }
    };

    ScrollReportingViewport viewport;
    juce::Component content;
    PianoKeys keys;
    NoteGrid grid;
    VelocityLane velocityLane;
    ArticulationLane articulationLane;
    /// Controller-Lanes unter der Velocity-Lane (Sustain, Mod-Wheel, Pitch-Bend, ...), beliebig viele.
    std::vector<std::unique_ptr<ControllerLane>> controllerLanes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyEditor)
};

} // namespace fk::ui
