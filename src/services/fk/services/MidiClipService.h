#pragma once

#include "fk/core/Result.h"
#include "fk/services/ControllerTypes.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <juce_events/juce_events.h>

#include <functional>
#include <vector>

namespace fk::services
{

/// Eine Note im Key Editor, in Schlägen (Viertelnoten) relativ zum Clip-Anfang.
/// Die Kennung ist der Index in der Notenliste; nach jeder Änderung neu holen (midiClipChanged).
struct EditableNote
{
    int index = 0;
    int noteNumber = 60;
    double startBeats = 0.0;
    double lengthBeats = 1.0;
    int velocity = 100;
    juce::String articulation; ///< Spielweise (Kennung aus der Expression Map der Spur), leer = keine
};

/// Ein Controller-Ereignis (CC, Pitch-Bend, Aftertouch) in Schlägen relativ zum Clip-Anfang.
/// Die Kennung ist der Index in der Ereignisliste des Clips; nach jeder Änderung neu holen.
struct ControllerEvent
{
    int index = 0;
    int type = 0; ///< siehe controllerTypes
    double beats = 0.0;
    int value = 0;
};

/// MIDI-Funktionen (Cubase: MIDI > Funktionen), auf die Auswahl oder den ganzen Clip.
enum class MidiFunction
{
    fixedLength,      ///< alle Noten gleich lang (beats)
    legato,           ///< jede Note bis zum Anfang der nächsten verlängern
    transpose,        ///< um semitones verschieben
    humanizeTiming,   ///< Anfänge zufällig um bis zu ±amount Schläge versetzen
    humanizeVelocity, ///< Anschlag zufällig um bis zu ±amount ändern
    fixedVelocity,    ///< alle mit velocity
    scaleVelocity,    ///< Anschlag × amount (1,1 = 10 % lauter)
    deleteDoubles     ///< gleiche Note am gleichen Anfang nur einmal
};

struct MidiFunctionParams
{
    MidiFunction function = MidiFunction::transpose;
    double beats = 0.25;  ///< feste Länge
    int semitones = 0;    ///< Transponieren
    int velocity = 100;   ///< fester Anschlag
    double amount = 0.0;  ///< Humanize-Stärke oder Faktor
    int randomSeed = 1;   ///< Humanize: gleicher Startwert, gleiches Ergebnis (Tests)
};

/// Quantisieren mit Stärke, Swing und Groove (Cubase: Quantisierungsfeld).
struct QuantiseSettings
{
    double gridBeats = 0.25;
    double strength = 1.0;      ///< 1 = ganz aufs Raster, 0,5 = halber Weg
    double swing = 0.0;         ///< 0..1: jeder zweite Rasterschritt nach hinten, 1 = Triolen-Gefühl
    std::vector<double> groove; ///< Versatz je Rasterschritt in Schlägen, wiederholt sich (leer = keiner)
};

/// Momentaufnahme eines MIDI-Clips für den Editor.
struct MidiClipInfo
{
    juce::String id;
    juce::String name;
    juce::String trackId;      ///< Spur des Clips (Drum-Map, Instrument)
    double startSeconds = 0.0; ///< Anfang im Projekt (für Tonart und Tempo an dieser Stelle)
    double lengthBeats = 4.0;  ///< Länge des Clips in Schlägen
    std::vector<EditableNote> notes;
    std::vector<ControllerEvent> controllers; ///< alle Arten, nach Zeit sortiert
};

/// Anwendungsfälle für den Inhalt von MIDI-Clips: Noten anlegen, verschieben, löschen,
/// quantisieren. Rechnet in Schlägen; die Umrechnung in Sekunden macht Tracktion.
class MidiClipService final : private ProjectService::Listener,
                              private juce::ValueTree::Listener,
                              private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Noten eines MIDI-Clips haben sich geändert (auch durch Aufnahme oder Undo).
        virtual void midiClipChanged() = 0;
    };

    explicit MidiClipService (ProjectService& projectService);
    ~MidiClipService() override;

    /// Legt einen leeren MIDI-Clip auf der Spur an. Nur sinnvoll auf Instrumentenspuren.
    [[nodiscard]] core::Result<juce::String>
    createEmptyClip (TrackHandle& track, double startSeconds, double lengthSeconds);

    [[nodiscard]] core::Result<MidiClipInfo> getClip (const juce::String& clipId) const;

    [[nodiscard]] core::Result<void>
    addNote (const juce::String& clipId, int noteNumber, double startBeats, double lengthBeats, int velocity);
    /// Mehrere Noten mit gleichem Anfang und gleicher Länge (ein Akkord) in einem Undo-Schritt.
    [[nodiscard]] core::Result<void> addChord (const juce::String& clipId,
                                               const std::vector<int>& noteNumbers,
                                               double startBeats,
                                               double lengthBeats,
                                               int velocity);
    [[nodiscard]] core::Result<void> removeNote (const juce::String& clipId, int noteIndex);
    [[nodiscard]] core::Result<void>
    moveNote (const juce::String& clipId, int noteIndex, int newNoteNumber, double newStartBeats);
    [[nodiscard]] core::Result<void> setNoteLength (const juce::String& clipId, int noteIndex, double newLengthBeats);
    [[nodiscard]] core::Result<void> setNoteVelocity (const juce::String& clipId, int noteIndex, int velocity);

    // Mehrere Noten in einem Undo-Schritt (Key Editor mit Auswahl). Die Kennung jeder Note ist
    // ihr Index vor der Änderung; danach neu holen.

    /// Setzt Tonhöhe, Anfang, Länge und Anschlag der Noten mit diesen Indizes.
    [[nodiscard]] core::Result<void>
    updateNotes (const juce::String& clipId, const std::vector<EditableNote>& notes, const juce::String& actionName);

    /// Fügt Noten hinzu (Index wird ignoriert), zum Beispiel Kopien beim Ziehen mit Alt.
    [[nodiscard]] core::Result<void>
    addNotes (const juce::String& clipId, const std::vector<EditableNote>& notes, const juce::String& actionName);

    [[nodiscard]] core::Result<void> removeNotes (const juce::String& clipId, const std::vector<int>& noteIndices);

    /// Spielweise der Noten setzen (leer = entfernen), ein Undo-Schritt. Wie sie beim Plugin ankommt,
    /// sagt die Expression Map der Spur (ExpressionMapService).
    [[nodiscard]] core::Result<void>
    setArticulation (const juce::String& clipId, const std::vector<int>& noteIndices, const juce::String& articulationId);

    /// Rückt Notenanfänge auf das nächste Vielfache von gridBeats (z. B. 0,25 = Sechzehntel).
    /// Mit Indizes nur diese Noten (die Auswahl), sonst alle.
    [[nodiscard]] core::Result<void>
    quantise (const juce::String& clipId, double gridBeats, const std::vector<int>& noteIndices = {});

    [[nodiscard]] core::Result<void>
    quantise (const juce::String& clipId, const QuantiseSettings& settings, const std::vector<int>& noteIndices = {});

    /// Zielposition einer Note beim Quantisieren (reine Rechnung, für Tests sichtbar).
    [[nodiscard]] static double quantisedStart (double startBeats, const QuantiseSettings& settings);

    /// Groove aus einem Clip: mittlerer Versatz der Noten je Rasterschritt eines Takts (stepsPerCycle).
    [[nodiscard]] core::Result<std::vector<double>>
    extractGroove (const juce::String& clipId, double gridBeats, int stepsPerCycle) const;

    /// Alle MIDI-Clips des Projekts: Kennung und "Spur: Clip" (für die Groove-Auswahl).
    [[nodiscard]] std::vector<std::pair<juce::String, juce::String>> listClips() const;

    /// Wendet eine MIDI-Funktion an (ein Undo-Schritt). Mit Indizes nur diese Noten, sonst alle.
    [[nodiscard]] core::Result<void>
    applyFunction (const juce::String& clipId, const MidiFunctionParams& params, const std::vector<int>& noteIndices = {});

    /// Die Rechnung dahinter, ohne Projekt: Liefert die geänderten Noten; bei deleteDoubles fehlen die
    /// Doppelten im Ergebnis (der Aufrufer löscht, was fehlt).
    [[nodiscard]] static std::vector<EditableNote> transform (std::vector<EditableNote> notes,
                                                              const MidiFunctionParams& params,
                                                              double clipLengthBeats);

    [[nodiscard]] static juce::String functionName (MidiFunction function);

    /// Kürzeste Note: eine 1/128-Triole.
    static constexpr double minNoteLengthBeats = 4.0 / 128.0 * 2.0 / 3.0;

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /// Eine Note über updateNotes ändern (Grundlage der Einzelnoten-Methoden).
    [[nodiscard]] core::Result<void> changeNote (const juce::String& clipId,
                                                 int noteIndex,
                                                 const juce::String& actionName,
                                                 const std::function<void (EditableNote&)>& change);

    /// Notenzeiger zu den Indizes holen; fehlt einer, schlägt alles fehl.
    [[nodiscard]] core::Result<std::vector<te::MidiNote*>> findNotes (te::MidiClip& clip,
                                                                      const std::vector<int>& noteIndices) const;
    /// Gebackene Spuren sperren ihre Clips (ADR 0009).
    [[nodiscard]] static core::Result<void> checkClipUnlocked (te::MidiClip& clip);
    [[nodiscard]] te::MidiClip* findClip (const juce::String& clipId) const;
    [[nodiscard]] te::MidiNote* findNote (te::MidiClip& clip, int noteIndex) const;

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipService)
};

} // namespace fk::services
