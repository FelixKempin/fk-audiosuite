#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <optional>
#include <string_view>
#include <vector>

namespace fk::services
{

/// Ein Take eines Clips (Loop-Aufnahme oder per Datei hinzugefügt).
struct TakeInfo
{
    int index = 0;
    juce::String name;
    bool isCurrent = false;
};

/// Anwendungsfälle für Clips: verschieben, schneiden, löschen, Audiodatei importieren, Takes.
/// Clips werden über ihre ID angesprochen (ClipInfo::id), nie über Zeiger: Die Oberfläche
/// hält damit nichts, was beim nächsten Umbau ins Leere zeigen könnte.
class ClipService final
{
public:
    explicit ClipService (ProjectService& projectService);

    /// Legt eine Audiodatei als neuen Clip auf die Spur. Länge = Dateilänge.
    [[nodiscard]] core::Result<juce::String>
    importAudioFile (te::AudioTrack& track, const juce::File& audioFile, double startSeconds);

    /// Zahl der Spuren mit Noten in einer Standard-MIDI-Datei (0 = unlesbar oder leer).
    [[nodiscard]] static int countMidiTracks (const juce::File& midiFile);

    /// Legt eine MIDI-Datei als Clip auf die Spur: midiTrackIndex wählt eine Spur der Datei
    /// (Zählung nur über Spuren mit Noten), -1 nimmt alle zusammen. Noten in Schlägen, das Tempo
    /// der Datei wird nicht übernommen (das Projekt behält seines).
    [[nodiscard]] core::Result<juce::String>
    importMidiFile (te::AudioTrack& track, const juce::File& midiFile, double startSeconds, int midiTrackIndex = -1);

    [[nodiscard]] core::Result<void> moveClip (const juce::String& clipId, double newStartSeconds);

    /// Teilt den Clip an der Position; liegt sie außerhalb, passiert nichts.
    [[nodiscard]] core::Result<void> splitClip (const juce::String& clipId, double atSeconds);

    [[nodiscard]] core::Result<void> deleteClip (const juce::String& clipId);

    /// Löscht mehrere Clips in einem Undo-Schritt; unbekannte Kennungen werden übergangen.
    [[nodiscard]] core::Result<void> deleteClips (const std::vector<juce::String>& clipIds);

    /// Normalisieren ohne die Datei zu ändern: Clip-Gain so, dass die lauteste Stelle im Clip targetDb
    /// erreicht (begrenzt auf ±24 dB). Nur Audio-Clips; MIDI-Clips in der Auswahl werden übersprungen.
    [[nodiscard]] core::Result<void> normalizeClips (const std::vector<juce::String>& clipIds, float targetDb = 0.0f);

    /// Rückwärts abspielen ein/aus. Tracktion legt dafür im Hintergrund eine umgekehrte Kopie an.
    [[nodiscard]] core::Result<void> setClipsReversed (const std::vector<juce::String>& clipIds, bool shouldBeReversed);

    /// Phase umkehren: schreibt den hörbaren Ausschnitt jedes Clips invertiert in eine neue Datei in Audio/
    /// und lässt den Clip darauf zeigen. Die alte Datei bleibt; Undo zeigt wieder auf sie.
    [[nodiscard]] core::Result<void> invertPhase (const std::vector<juce::String>& clipIds);

    /// Bearbeitungen im Sample-Editor. Sie wirken auf das Material, nicht auf das Clip-Gain.
    enum class AudioProcess
    {
        silence,     ///< Stille
        invertPhase, ///< Phase umkehren
        reverse,     ///< Abschnitt rückwärts
        normalize,   ///< Spitze des Abschnitts auf 0 dBFS
        gain,        ///< um gainDb verstärken oder abschwächen
        fadeIn,      ///< linear von 0 auf 1
        fadeOut      ///< linear von 1 auf 0
    };

    /// Abschnitt innerhalb eines Clips, in Sekunden ab Clip-Anfang (Clip-Zeit).
    struct ClipRange
    {
        double fromSeconds = 0.0;
        double toSeconds = 0.0;
    };

    /// Wendet eine Bearbeitung auf den hörbaren Teil jedes Clips an (oder nur auf range) und schreibt das
    /// Ergebnis als neue Datei in Audio/; Undo zeigt wieder auf die alte. Nicht bei Clips, die dem
    /// Projekttempo folgen oder rückwärts laufen (deren Zeit lässt sich nicht eindeutig umrechnen).
    [[nodiscard]] core::Result<void> processAudio (const std::vector<juce::String>& clipIds,
                                                   AudioProcess process,
                                                   std::optional<ClipRange> range = std::nullopt,
                                                   float gainDb = 0.0f);

    // --- Time-Stretch und Pitch-Shift (Signalsmith Stretch).

    /// Tempo des Materials schätzen (SoundTouch-BPM-Erkennung über die ganze Quelldatei).
    /// Fehler, wenn kein sinnvolles Tempo (29–200 BPM) erkennbar ist, z. B. bei Flächen ohne Rhythmus.
    [[nodiscard]] core::Result<double> detectTempo (const juce::String& clipId) const;

    /// Clip folgt dem Projekttempo (Cubase: Musikmodus): Tempoänderungen strecken das Material mit.
    /// sourceBpm: Tempo des Materials; 0 = erkennen. follow = false spielt wieder in Originalgeschwindigkeit.
    [[nodiscard]] core::Result<void>
    setFollowsTempo (const std::vector<juce::String>& clipIds, bool follow, double sourceBpm = 0.0);

    /// Tonhöhe in Halbtönen ändern, ohne die Länge zu ändern (−24 bis +24).
    [[nodiscard]] core::Result<void> setPitch (const std::vector<juce::String>& clipIds, float semitones);

    /// Clip auf eine neue Länge strecken oder stauchen; das Material bleibt vollständig drin.
    /// Geht nicht, solange der Clip dem Projekttempo folgt (dann bestimmt das Tempo die Länge).
    [[nodiscard]] core::Result<void> stretchClip (const juce::String& clipId, double newLengthSeconds);

    // --- Bereichswerkzeug: Zeitbereich [from, to] auf mehreren Spuren (Cubase: Bereichsauswahl).

    /// Material im Bereich entfernen. closeGap = false: Lücke bleibt (Stille); true: Späteres rückt nach.
    [[nodiscard]] core::Result<void>
    deleteRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds, bool closeGap);

    /// Stille einfügen: Clips ab from rücken um die Bereichslänge nach hinten, Clips über from werden geteilt.
    [[nodiscard]] core::Result<void>
    insertSilence (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds);

    /// Alle Clips an den Bereichsgrenzen teilen.
    [[nodiscard]] core::Result<void>
    splitAtRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds);

    /// Kennungen aller Clips, die den Bereich auf diesen Spuren berühren (Lasso).
    [[nodiscard]] std::vector<juce::String>
    getClipsInRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds) const;

    /// Clips stumm schalten oder wieder hörbar machen (Mute-Werkzeug, Kontextmenü). Ein Undo-Schritt.
    [[nodiscard]] core::Result<void> setClipsMuted (const std::vector<juce::String>& clipIds, bool shouldBeMuted);

    /// Trimmt den Clip auf den Bereich [newStart, newEnd]. Der linke Rand nimmt den Quellversatz
    /// mit (das Material bleibt an seiner Stelle im Projekt), der rechte ändert nur die Länge.
    /// Grenzen: nie vor den Anfang des Materials oder vor 0, Ende immer nach dem Anfang.
    [[nodiscard]] core::Result<void>
    trimClip (const juce::String& clipId, double newStartSeconds, double newEndSeconds);

    /// Verschiebt mehrere Clips gemeinsam um deltaSeconds und um trackDelta Spuren nach unten
    /// (negativ = nach oben). Zielspuren müssen zur Clipart passen (Audio auf Audiospur, MIDI auf
    /// Instrumentenspur); passt eine nicht, ändert sich nichts. Ein Undo-Schritt.
    [[nodiscard]] core::Result<void>
    moveClips (const std::vector<juce::String>& clipIds, double deltaSeconds, int trackDelta);

    // Zwischenablage (nur innerhalb der Anwendung). Kopien merken sich Zeit- und Spurabstand
    // zum ersten Clip, damit ein Block aus mehreren Spuren seine Form behält.
    [[nodiscard]] core::Result<void> copyClips (const std::vector<juce::String>& clipIds);
    [[nodiscard]] core::Result<void> cutClips (const std::vector<juce::String>& clipIds);
    [[nodiscard]] bool hasClipboardContent() const noexcept;

    /// Fügt die Zwischenablage ein: der erste Clip auf die Spur bei atSeconds, die anderen mit
    /// ihrem Abstand dazu. Fehlt eine Zielspur oder passt die Art nicht, ändert sich nichts.
    [[nodiscard]] core::Result<std::vector<juce::String>> pasteClips (te::AudioTrack& track, double atSeconds);

    /// Kopiert die Clips direkt hinter das Ende der Auswahl (als Block, Spuren bleiben).
    [[nodiscard]] core::Result<std::vector<juce::String>> duplicateClips (const std::vector<juce::String>& clipIds);

    /// Verbindet Clips einer Spur zu einem: MIDI führt die Noten zusammen, Audio rendert die
    /// Clips (ohne Plugins, mit Fades und Gain) in eine neue Datei in Audio/. Liefert den neuen Clip.
    [[nodiscard]] core::Result<juce::String> mergeClips (const std::vector<juce::String>& clipIds);

    /// Ein-/Ausblendung in Sekunden (nur Audio-Clips); zusammen nie länger als der Clip.
    /// Kein eigener Undo-Schritt: Wer zieht, ruft vorher einmal beginChange().
    [[nodiscard]] core::Result<void> setFades (const juce::String& clipId, double fadeInSeconds, double fadeOutSeconds);

    /// Clip-Gain in dB (nur Audio-Clips), -24..+24. Kein eigener Undo-Schritt (siehe setFades).
    [[nodiscard]] core::Result<void> setGainDb (const juce::String& clipId, float gainDb);

    /// Beginnt einen Undo-Schritt für eine zusammenhängende Änderung (Fade ziehen, Gain regeln).
    void beginChange (const juce::String& actionName);

    // Takes. Tracktion legt sie bei Loop-Aufnahmen an (ein Take je Durchlauf); addTakeFromFile
    // ergänzt einen aus einer Datei (nur Audio-Clips).
    [[nodiscard]] std::vector<TakeInfo> getTakes (const juce::String& clipId) const;
    [[nodiscard]] core::Result<void> setCurrentTake (const juce::String& clipId, int takeIndex);
    [[nodiscard]] core::Result<void> addTakeFromFile (const juce::String& clipId, const juce::File& audioFile);
    [[nodiscard]] core::Result<void> setShowingTakes (const juce::String& clipId, bool shouldShow);

    /// Comping ohne Render: schneidet den Clip an den Grenzen des Bereichs (Projektzeit in Sekunden)
    /// und schaltet das Mittelstück auf den Take. Liefert die Kennung des Mittelstücks.
    /// Alle Stücke behalten alle Takes, so bleibt jede Entscheidung später änderbar.
    [[nodiscard]] core::Result<juce::String>
    useTakeForRange (const juce::String& clipId, int takeIndex, double fromSeconds, double toSeconds);

    /// Verteilt die Takes auf Spuren unter dieser: je Take ein Clip auf der nächsten Spur (fehlt sie,
    /// wird sie angelegt). Der Clip selbst bleibt mit seinem aktuellen Take.
    [[nodiscard]] core::Result<void> unpackTakes (const juce::String& clipId);

    /// Entfernt alle Takes außer dem aktuellen (nur Audio-Clips; Dateien bleiben erhalten).
    [[nodiscard]] core::Result<void> deleteUnusedTakes (const juce::String& clipId);

private:
    /// Gebackene Spuren sperren ihre Clips (ADR 0009); liefert die Meldung für die erste gesperrte.
    [[nodiscard]] core::Result<void> checkUnlocked (const std::vector<juce::String>& clipIds) const;
    [[nodiscard]] te::Clip* findClip (const juce::String& clipId) const;

    /// Kopiert eine Datei von außerhalb in den Projektordner (Audio/) und liefert die Kopie;
    /// Dateien im Projekt bleiben, wo sie sind. Fehler, wenn die Kopie misslingt.
    [[nodiscard]] core::Result<juce::File> ensureInProject (const juce::File& audioFile) const;

    /// Prüft, ob der Clip auf die Spur darf: Audio-Clips auf Spuren ohne Instrument, MIDI-Clips
    /// auf Spuren mit Instrument (dieselbe Regel wie TrackService::getKind).
    [[nodiscard]] static bool clipFitsTrack (const te::Clip& clip, te::AudioTrack& track);
    [[nodiscard]] static bool clipStateFitsTrack (const juce::ValueTree& clipState, te::AudioTrack& track);

    /// Ein Eintrag der Zwischenablage: Zustand des Clips plus Abstand zum ersten kopierten Clip.
    struct ClipboardEntry
    {
        juce::ValueTree state;
        double startOffsetSeconds = 0.0;
        int trackOffset = 0;
    };

    [[nodiscard]] std::vector<ClipboardEntry> makeEntries (const std::vector<juce::String>& clipIds) const;
    [[nodiscard]] core::Result<std::vector<juce::String>>
    insertEntries (const std::vector<ClipboardEntry>& entries, te::AudioTrack& firstTrack, double atSeconds);

    ProjectService& projectService;
    std::vector<ClipboardEntry> clipboard;
};

} // namespace fk::services
