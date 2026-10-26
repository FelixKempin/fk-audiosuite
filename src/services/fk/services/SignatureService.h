#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TimelineService.h"

#include <juce_events/juce_events.h>

#include <vector>

namespace fk::services
{

/// Skalen, die Tonart und Stufenakkorde kennen. Reihenfolge = Reihenfolge in Listen der Oberfläche.
enum class ScaleKind
{
    major,
    minor,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    locrian,
    harmonicMinor,
    melodicMinor
};

/// Dreiklang oder Septakkord auf einer Stufe.
enum class ChordKind
{
    triad,
    seventh
};

/// Ein Tempopunkt der Signaturspur. ramp = lineare Rampe bis zum nächsten Punkt (wie die
/// Cubase-Tempospur), sonst Sprung, der bis zum nächsten Punkt gilt.
struct TempoPointInfo
{
    int index = 0;
    double beat = 0.0; ///< Viertelnoten ab Projektanfang
    double seconds = 0.0;
    double bpm = 120.0;
    bool ramp = false;
};

/// Ein Taktartwechsel; liegt immer auf einem Taktanfang.
struct TimeSignaturePointInfo
{
    int index = 0;
    double beat = 0.0;
    double seconds = 0.0;
    int bar = 1; ///< 1-basiert
    TimeSignature signature;
};

/// Tempo- und Taktart-Stand einer MIDI-Datei ab einer Stelle (Viertelnoten ab Dateianfang).
struct MidiTempoChange
{
    double quarterNote = 0.0;
    double quarterBpm = 120.0; ///< Viertel pro Minute, wie MIDI-Dateien zählen
    TimeSignature signature;
};

/// Eine Tonart ab einer Position: Grundton (0 = C … 11 = H) und Skala.
struct KeyPointInfo
{
    int index = 0;
    double beat = 0.0;
    double seconds = 0.0;
    int rootNote = 0;
    ScaleKind scale = ScaleKind::major;
    juce::String name; ///< "C-Dur", "A-Moll", "D-Dorisch"
};

/// Anwendungsfälle der Signaturspur: Tempo (Sprung oder Rampe), Taktart und Tonart am Raster,
/// dazu Musiktheorie für den Key Editor: Skalentöne und Akkorde nach Stufen relativ zur Tonart.
/// Alles liegt im Projekt (Tracktion: TempoSequence, PitchSequence); der erste Punkt jeder
/// Art ist immer da und lässt sich nicht entfernen.
class SignatureService final : private ProjectService::Listener,
                               private juce::ValueTree::Listener,
                               private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Tempo-, Taktart- oder Tonartpunkte haben sich geändert (auch durch Undo).
        virtual void signatureChanged() = 0;
    };

    explicit SignatureService (ProjectService& projectService);
    ~SignatureService() override;

    // Tempo
    [[nodiscard]] std::vector<TempoPointInfo> getTempoPoints() const;
    [[nodiscard]] double getTempoAt (double seconds) const;
    [[nodiscard]] core::Result<int> insertTempoPoint (double seconds, double bpm, bool ramp);
    [[nodiscard]] core::Result<void> setTempoPoint (int index, double bpm, bool ramp);
    [[nodiscard]] core::Result<void> moveTempoPoint (int index, double seconds);
    [[nodiscard]] core::Result<void> removeTempoPoint (int index);

    /// Tempo und Taktarten einer MIDI-Datei; leer, wenn sie keine Tempo-Angabe enthält.
    [[nodiscard]] static std::vector<MidiTempoChange> readMidiFileTempo (const juce::File& midiFile);

    /// true, wenn die Datei ein eigenes Tempo oder eine eigene Taktart hat, die an der Stelle vom
    /// Projekt abweicht. Die Oberfläche fragt dann beim Import, ob sie übernommen werden.
    [[nodiscard]] bool midiFileTempoDiffers (const juce::File& midiFile, double startSeconds) const;

    /// Ersetzt Tempo und Taktart ab startSeconds durch die der Datei (ein Undo-Schritt). Vor dem
    /// Einfügen der Noten aufrufen, damit Clip-Länge und Noten zum neuen Tempo passen.
    [[nodiscard]] core::Result<void> applyMidiFileTempo (const juce::File& midiFile, double startSeconds);

    // Taktart
    [[nodiscard]] std::vector<TimeSignaturePointInfo> getTimeSignaturePoints() const;
    /// Setzt den Wechsel auf den Taktanfang, in dem seconds liegt.
    [[nodiscard]] core::Result<int> insertTimeSignature (double seconds, TimeSignature signature);
    [[nodiscard]] core::Result<void> setTimeSignature (int index, TimeSignature signature);
    [[nodiscard]] core::Result<void> removeTimeSignature (int index);

    // Tonart
    [[nodiscard]] std::vector<KeyPointInfo> getKeyPoints() const;
    [[nodiscard]] KeyPointInfo getKeyAt (double seconds) const;
    [[nodiscard]] KeyPointInfo getProjectKey() const;
    [[nodiscard]] core::Result<int> insertKey (double seconds, int rootNote, ScaleKind scale);
    [[nodiscard]] core::Result<void> setKey (int index, int rootNote, ScaleKind scale);
    [[nodiscard]] core::Result<void> moveKey (int index, double seconds);
    [[nodiscard]] core::Result<void> removeKey (int index);

    // Musiktheorie (ohne Projekt nutzbar)
    [[nodiscard]] static juce::String getNoteName (int rootNote);
    [[nodiscard]] static juce::String getScaleName (ScaleKind scale);
    [[nodiscard]] static juce::String getKeyName (int rootNote, ScaleKind scale);
    /// Tonklassen 0..11 der Skala, beginnend beim Grundton.
    [[nodiscard]] static std::vector<int> getScalePitchClasses (int rootNote, ScaleKind scale);
    [[nodiscard]] static bool isInScale (int noteNumber, int rootNote, ScaleKind scale);
    /// MIDI-Noten des Akkords auf der Stufe (1–7) in der Oktave (4 = eingestrichen, C4 = 60).
    [[nodiscard]] static std::vector<int>
    getChordNotes (int rootNote, ScaleKind scale, int degree, ChordKind kind, int inversion, int octave);
    /// Akkordsymbol wie "Dm", "G7", "Bdim".
    [[nodiscard]] static juce::String getChordSymbol (int rootNote, ScaleKind scale, int degree, ChordKind kind);
    /// Stufenbezeichnung "I", "ii", "vii°" nach Akkordtyp.
    [[nodiscard]] static juce::String getDegreeName (int rootNote, ScaleKind scale, int degree, ChordKind kind);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    [[nodiscard]] static bool isSignatureTree (const juce::ValueTree& tree);
    [[nodiscard]] KeyPointInfo keyInfoFor (int index) const;

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignatureService)
};

} // namespace fk::services
