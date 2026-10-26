#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <juce_events/juce_events.h>

#include <vector>

namespace fk::services
{

/// Spur-Handle für die Oberfläche: Sie reicht es nur an Services zurück und ruft darauf
/// keine Methoden auf (ADR 0004). Der Alias macht die Grenze im UI-Code sichtbar.
using TrackHandle = te::AudioTrack;

/// Ordner-Handle, wie TrackHandle nur zum Zurückreichen an den Service.
using FolderHandle = te::FolderTrack;

/// Spur oder Ordner, für Aktionen, die beides verschieben (Basisklasse beider Handles).
using TrackOrFolderHandle = te::Track;

/// Ein Audio-Eingang des Interfaces, wie ihn die Oberfläche zur Auswahl anbietet.
struct AudioInputInfo
{
    juce::String id;     ///< stabile Geräte-ID (für Projektdateien und Vergleiche)
    juce::String name;   ///< Anzeigename, z. B. "Eingang 1"
    int numChannels = 1; ///< 1 = Mono, 2 = Stereo (bei MIDI ohne Bedeutung)
};

/// Spurart. Technisch sind alle drei Tracktion-Audiospuren; die Art bestimmt Eingänge, Anzeige und Rolle.
enum class TrackKind
{
    audio,      ///< nimmt Audio auf
    instrument, ///< hat ein Instrument-Plugin, nimmt MIDI auf
    group,      ///< Gruppenspur: nur Summe anderer Spuren, kein Eingang
    fx,         ///< FX-Spur: empfängt Sends über einen Aux-Return, kein Eingang
    rackOutput, ///< Ausgang eines Rack-Instruments: Kanalzug im Mixer, kein Eingang, keine Clips
    midi        ///< MIDI-Spur an einem externen Gerät: MIDI-Clips, Ausgang auf einen MIDI-Port
};

/// Spurarten, die MIDI aufnehmen und MIDI-Clips tragen.
[[nodiscard]] inline bool playsMidi (TrackKind kind) noexcept
{
    return kind == TrackKind::instrument || kind == TrackKind::midi;
}

/// Wohin eine Spur ihr Signal schickt.
struct OutputInfo
{
    juce::String id;   ///< "master", die Kennung einer Gruppenspur oder "device:<Geräte-ID>" (Direct Out)
    juce::String name; ///< Anzeigename
};

/// Ein Anschluss des Interfaces für den Dialog "Studio-Verbindungen".
struct StudioPort
{
    juce::String id;     ///< stabile Geräte-ID; der eigene Name ändert sie nicht
    juce::String name;   ///< Name wie auf dem Interface, z. B. "Eingang 3"
    juce::String alias;  ///< eigener Name, z. B. "Gitarren-DI" (leer = keiner)
    bool isInput = true; ///< Eingang oder Ausgangspaar
    int numChannels = 1;
};

/// Momentaufnahme einer Spur für die Anzeige. Werte, keine Handles: Die Oberfläche
/// zeichnet daraus und ruft für Änderungen den Service.
struct TrackInfo
{
    juce::String name;
    juce::Colour colour; ///< Spurfarbe; ältere Projekte ohne Farbe bekommen die Palettenfarbe ihrer Position
    bool hidden = false; ///< ausgeblendet: nicht im Arrangement und in der MixConsole, spielt aber weiter
    TrackKind kind = TrackKind::audio;
    int numChannels = 2;   ///< Mono (1) oder Stereo (2), bei Anlage festgelegt
    juce::String inputId;  ///< Audio- oder MIDI-Eingang, je nach Spurart
    juce::String outputId; ///< "master" oder Gruppenspur-Kennung
    juce::String outputName;
    bool isInstrument = false;   ///< bequemer Zugriff auf kind == instrument
    juce::String instrumentName; ///< Name des Instrument-Plugins, leer ohne Instrument
    bool muted = false;
    bool solo = false;
    bool armed = false;
    bool baked = false;          ///< selbst gebacken (ADR 0009); Details liefert der BakeService
    juce::String bakedIntoGroup; ///< Name der gebackenen Gruppe, in die die Spur mündet, sonst leer
    bool inRack = false;         ///< MIDI-Spur oder Ausgangskanal eines Rack-Instruments (RackService)
};

/// Momentaufnahme einer Ordnerspur für die Anzeige.
struct FolderInfo
{
    juce::String name;
    juce::Colour colour;
    bool collapsed = false; ///< zugeklappt: Inhalt im Arrangement verborgen (spielt weiter)
    bool muted = false;     ///< schaltet alle Spuren darin stumm
    bool solo = false;
    int numTracks = 0; ///< Spuren und Ordner direkt darin
};

/// Ein Eintrag der Spurliste im Arrangement: eine Ordnerspur oder eine Spur, mit Einrücktiefe.
struct TrackListEntry
{
    TrackHandle* track = nullptr;   ///< gesetzt bei Spuren
    FolderHandle* folder = nullptr; ///< gesetzt bei Ordnern
    int depth = 0;                  ///< 0 = oberste Ebene, 1 = in einem Ordner, ...
};

/// Eine MIDI-Note für die Anzeige, in Sekunden (die Oberfläche kennt nur Sekunden).
struct NoteInfo
{
    int noteNumber = 60;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    int velocity = 100;
};

/// Momentaufnahme eines Audio-Clips für die Anzeige.
struct ClipInfo
{
    juce::String id; ///< stabile Kennung für ClipService
    juce::String name;
    bool isMidi = false;
    std::vector<NoteInfo> notes; ///< nur bei MIDI-Clips gefüllt
    juce::File audioFile;        ///< nur bei Audio-Clips gesetzt
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    double offsetSeconds = 0.0;  ///< Versatz in der Quelldatei, mit dem der Clip beginnt
    bool muted = false;          ///< stummer Clip: bleibt sichtbar, spielt nicht
    bool reversed = false;       ///< spielt rückwärts (nur Audio-Clips)
    bool followsTempo = false;   ///< folgt dem Projekttempo (Cubase: Musikmodus), nur Audio
    double sourceBpm = 0.0;      ///< Tempo des Materials, wenn bekannt (für "folgt Tempo")
    double speedRatio = 1.0;     ///< Abspielgeschwindigkeit der Quelle (gestreckt < 1, gestaucht > 1)
    float pitchSemitones = 0.0f; ///< Tonhöhenänderung in Halbtönen, Länge bleibt
    double fadeInSeconds = 0.0;  ///< nur Audio-Clips
    double fadeOutSeconds = 0.0; ///< nur Audio-Clips
    float gainDb = 0.0f;         ///< Clip-Gain, nur Audio-Clips
    int numTakes = 0;            ///< 0 oder 1 = keine Alternativen; ab 2 gibt es Takes (Loop-Aufnahme)
    int currentTake = 0;
    std::vector<juce::String> takeNames; ///< ein Name je Take, Reihenfolge wie Index
    bool showingTakes = false;           ///< Take-Lanes unter dem Clip aufgeklappt
};

/// Anwendungsfälle für Spuren: anlegen, löschen, Eingang zuweisen, scharfschalten, Mute/Solo.
/// Liefert Tracktion-Spuren als Handles (te::AudioTrack*) an die Oberfläche; die ruft darauf
/// keine Methoden auf, sondern reicht sie hierher zurück (ADR 0004).
///
/// Änderungen meldet das Edit über seinen ValueTree; wir bündeln sie mit einem AsyncUpdater
/// zu einer Meldung pro Ereigniswelle. Zwei Arten: Der Spurbestand ändert sich (tracksChanged,
/// die Oberfläche baut neu) oder nur Inhalt und Eigenschaften (contentChanged, neu zeichnen
/// genügt). Wird eine Spur entfernt, kommt tracksChanged sofort und nicht asynchron: Die
/// Oberfläche darf keinen Augenblick länger ein Handle auf die gelöschte Spur halten.
class TrackService final : private ProjectService::Listener,
                           private juce::ValueTree::Listener,
                           private juce::AsyncUpdater,
                           private juce::ChangeListener // Geräteliste der Engine (Hot-Plug)
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Spuren wurden hinzugefügt, entfernt, umsortiert oder Eingänge neu verteilt.
        virtual void tracksChanged() = 0;

        /// Clips, Noten oder Spureigenschaften (Name, Mute, Solo) haben sich geändert; die
        /// Spuren selbst sind dieselben. Wer nichts Billigeres kann, baut einfach neu.
        virtual void contentChanged() { tracksChanged(); }
    };

    explicit TrackService (ProjectService& projectService);
    ~TrackService() override;

    /// numChannels: 1 = Mono, 2 = Stereo. Bestimmt, welche Eingänge angeboten werden.
    [[nodiscard]] core::Result<te::AudioTrack*> createAudioTrack (const juce::String& name, int numChannels = 2);

    /// Wie createAudioTrack, aber ohne eigenen Undo-Schritt: für Services, die mehrere Spuren in
    /// einem Schritt anlegen (Rack-Instrument mit MIDI-Spur und Ausgangskanal).
    [[nodiscard]] core::Result<te::AudioTrack*> appendAudioTrack (const juce::String& name, int numChannels = 2);

    /// Gruppenspur: Ziel für andere Spuren, selbst ohne Eingang; kann wieder in Gruppen oder den Master gehen.
    [[nodiscard]] core::Result<te::AudioTrack*> createGroupTrack (const juce::String& name);

    [[nodiscard]] juce::String getTrackId (te::AudioTrack& track) const;
    [[nodiscard]] te::AudioTrack* findTrack (const juce::String& trackId) const;

    /// Legt eine Spur mit dem Instrument-Plugin (Kennung aus PluginService) als erstem Insert an.
    /// Eingang ist sofort "Alle MIDI-Eingänge", damit Keyboard und Hardware ohne Umweg spielen.
    [[nodiscard]] core::Result<te::AudioTrack*> createInstrumentTrack (const juce::String& name,
                                                                       const juce::String& instrumentPluginId);
    void deleteTrack (te::AudioTrack& track);
    [[nodiscard]] std::vector<te::AudioTrack*> getAudioTracks() const;
    [[nodiscard]] TrackInfo getTrackInfo (te::AudioTrack& track) const;
    [[nodiscard]] std::vector<ClipInfo> getClips (te::AudioTrack& track) const;

    /// Nur die nicht ausgeblendeten Spuren, in Projektreihenfolge (Arrangement, MixConsole).
    [[nodiscard]] std::vector<te::AudioTrack*> getVisibleTracks() const;

    void setTrackName (te::AudioTrack& track, const juce::String& name);
    void setTrackColour (te::AudioTrack& track, juce::Colour colour);
    void setHidden (te::AudioTrack& track, bool shouldBeHidden);

    /// Verschiebt die Spur an Position newIndex in der Liste aller Spuren (0 = ganz oben).
    [[nodiscard]] core::Result<void> moveTrack (te::AudioTrack& track, int newIndex);

    // --- Ordnerspuren (Cubase: Ordnerspur). Reine Ordnung, kein Bus; Mute/Solo wirken auf den Inhalt.

    /// Neue Ordnerspur am Ende der obersten Ebene.
    [[nodiscard]] core::Result<FolderHandle*> createFolderTrack (const juce::String& name);
    [[nodiscard]] std::vector<FolderHandle*> getFolders() const;
    [[nodiscard]] FolderInfo getFolderInfo (FolderHandle& folder) const;
    [[nodiscard]] FolderHandle* getParentFolder (te::Track& track) const;

    /// Alle Spuren im Ordner, auch in Unterordnern und ausgeblendete (für die Clip-Übersicht im Ordner).
    [[nodiscard]] std::vector<TrackHandle*> getTracksInFolder (FolderHandle& folder) const;
    void setFolderName (FolderHandle& folder, const juce::String& name);
    void setFolderColour (FolderHandle& folder, juce::Colour colour);
    void setFolderMuted (FolderHandle& folder, bool shouldBeMuted);
    void setFolderSolo (FolderHandle& folder, bool shouldBeSolo);

    /// Auf- und Zuklappen ist Ansicht, kein Undo-Schritt; wird aber mit dem Projekt gespeichert.
    void setFolderCollapsed (FolderHandle& folder, bool shouldBeCollapsed);

    /// Löst den Ordner auf. keepContents: Spuren rücken an seine Stelle; sonst gehen sie mit.
    [[nodiscard]] core::Result<void> deleteFolder (FolderHandle& folder, bool keepContents);

    /// Spurliste fürs Arrangement in Anzeigereihenfolge: Ordner, darunter ihr Inhalt (eingerückt),
    /// ohne ausgeblendete Spuren und ohne den Inhalt zugeklappter Ordner.
    [[nodiscard]] std::vector<TrackListEntry> getTrackList() const;

    /// Spur oder Ordner vor before verschieben (in dessen Ordner); before == nullptr: ans Ende, oberste Ebene.
    [[nodiscard]] core::Result<void> moveTrackBefore (te::Track& track, te::Track* before);

    /// Spur oder Ordner als letztes Element in folder; folder == nullptr: aus dem Ordner auf die oberste Ebene.
    [[nodiscard]] core::Result<void> moveIntoFolder (te::Track& track, FolderHandle* folder);

    /// Eine Stelle nach oben (-1) oder unten (+1) innerhalb desselben Ordners.
    [[nodiscard]] core::Result<void> moveWithinFolder (te::Track& track, int delta);

    /// Legt eine Kopie direkt unter der Spur an: Plugins, Einstellungen, Clips, Automation.
    /// Gebackene Spuren werden abgelehnt (die gerenderte Datei gehört zur Originalspur).
    [[nodiscard]] core::Result<te::AudioTrack*> duplicateTrack (te::AudioTrack& track);
    void setMuted (te::AudioTrack& track, bool shouldBeMuted);
    void setSolo (te::AudioTrack& track, bool shouldBeSolo);

    /// Alle freigeschalteten Audio-Eingänge des aktuellen Geräts.
    [[nodiscard]] std::vector<AudioInputInfo> getAvailableAudioInputs() const;

    /// Nur die Eingänge, die zur Spur passen: Mono-Eingänge für Monospuren, Stereo-Paare für Stereospuren.
    [[nodiscard]] std::vector<AudioInputInfo> getAvailableAudioInputs (te::AudioTrack& track) const;

    /// Mögliche Ausgänge: Master, alle Gruppen, die nicht (auch indirekt) in diese Spur zurückführen,
    /// und die Ausgangspaare des Interfaces außer dem Master-Paar (Direct Out, am Master vorbei).
    [[nodiscard]] std::vector<OutputInfo> getAvailableOutputs (te::AudioTrack& track) const;
    [[nodiscard]] core::Result<void> setOutput (te::AudioTrack& track, const juce::String& outputId);
    [[nodiscard]] juce::String getOutputId (te::AudioTrack& track) const;

    /// Alle Audio-Ein- und Ausgänge des Interfaces mit ihren eigenen Namen.
    [[nodiscard]] std::vector<StudioPort> getStudioPorts() const;

    /// Eigener Anzeigename für einen Anschluss (leer = Treibername). Gilt für alle Projekte;
    /// Projekte speichern die Geräte-ID, Zuweisungen bleiben erhalten.
    [[nodiscard]] core::Result<void> setPortAlias (const juce::String& portId, const juce::String& alias);

    /// Alle MIDI-Eingänge (physische Geräte und virtuelle wie das Bildschirm-Keyboard).
    [[nodiscard]] std::vector<AudioInputInfo> getAvailableMidiInputs() const;

    /// Weist der Spur einen Eingang zu (leere ID = keiner). Ein Eingang kann mehreren Spuren dienen.
    [[nodiscard]] core::Result<void> setAudioInput (te::AudioTrack& track, const juce::String& inputId);
    [[nodiscard]] juce::String getAudioInputId (te::AudioTrack& track) const;

    /// Scharfschalten: Die Spur nimmt beim nächsten Record auf. Ohne zugewiesenen Eingang wirkungslos.
    void setArmed (te::AudioTrack& track, bool shouldBeArmed);
    [[nodiscard]] bool isArmed (te::AudioTrack& track) const;

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int oldIndex, int newIndex) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    [[nodiscard]] te::InputDeviceInstance* findInputInstanceForTrack (te::AudioTrack& track) const;
    [[nodiscard]] std::vector<AudioInputInfo> getAvailableInputs (bool midi) const;
    [[nodiscard]] static bool isMidiDevice (const te::InputDevice& device);
    [[nodiscard]] static bool hasInstrument (te::AudioTrack& track);
    [[nodiscard]] static bool isGroupTrack (te::AudioTrack& track);
    [[nodiscard]] static int getNumChannels (te::AudioTrack& track);
    [[nodiscard]] TrackKind getKind (te::AudioTrack& track) const;
    [[nodiscard]] te::InputDeviceInstance* findInputInstanceById (const juce::String& inputId) const;

    void markStructureChanged();
    void markContentChanged();

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;
    bool structureChangePending = false;
    bool contentChangePending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackService)
};

} // namespace fk::services
