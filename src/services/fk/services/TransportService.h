#pragma once

#include "fk/services/ProjectService.h"

#include <juce_events/juce_events.h>

#include <optional>
#include <string_view>

namespace fk::services
{

/// Anwendungsfälle für Play, Stop, Aufnahme und Positionierung.
/// Hängt am jeweils offenen Edit und wechselt mit, wenn ProjectService ein neues lädt.
///
/// Zustandsänderungen (läuft / nimmt auf) meldet Tracktion als ChangeBroadcaster;
/// wir übersetzen das in unsere eigene, schmale Listener-Schnittstelle (ISP, SOLID "I").
class TransportService final : private ProjectService::Listener, private juce::ChangeListener, private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Play/Stop/Record hat sich geändert. Die Position wird nicht gemeldet; dafür pollen.
        virtual void transportStateChanged() = 0;
    };

    explicit TransportService (ProjectService& projectService);
    ~TransportService() override;

    void play();
    void stop();
    void togglePlay();

    /// Startet die Aufnahme auf allen scharfgeschalteten Spuren. Ohne scharfe Spur passiert nichts.
    void record();

    /// Retrospektive Aufnahme (Cubase: MIDI, das ohne Aufnahme gespielt wurde, nachträglich holen):
    /// Die letzten 30 Sekunden der MIDI-Eingänge werden auf den scharfen Spuren zu Clips.
    /// Liefert die Zahl der neuen Clips; 0 = nichts gespielt oder keine Spur scharf.
    [[nodiscard]] core::Result<int> captureRetrospective();

    void returnToStart();

    /// Vorzähler vor der Aufnahme (Metronom zählt ein, Aufnahme beginnt am Positionszeiger).
    enum class CountIn
    {
        none,
        oneBar,
        twoBars
    };

    [[nodiscard]] CountIn getCountIn() const;
    void setCountIn (CountIn countIn);
    [[nodiscard]] static juce::String getCountInName (CountIn countIn);
    /// Punch In/Out: Aufgenommen wird nur zwischen den Locatoren (dem Loop-Bereich), auch wenn
    /// der Transport früher startet. Schließt Loop aus: Punch an schaltet Loop ab und umgekehrt.
    [[nodiscard]] bool isPunchEnabled() const;
    void setPunchEnabled (bool shouldPunch);

    /// Pre-Roll: Bei Punch-Aufnahme startet die Wiedergabe so viele Sekunden vor dem linken Locator.
    /// Post-Roll: Nach dem rechten Locator läuft sie so lange weiter und stoppt dann.
    /// 0 = aus. Gespeichert im Projekt.
    [[nodiscard]] double getPreRollSeconds() const;
    void setPreRollSeconds (double seconds);
    [[nodiscard]] double getPostRollSeconds() const;
    void setPostRollSeconds (double seconds);

    void setPositionSeconds (double seconds);

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isRecording() const;
    [[nodiscard]] double getPositionSeconds() const;

    /// Auslastung des Audio-Callbacks (0..1). Ab etwa 0,98 schaltet die Engine Blöcke stumm,
    /// damit das Gerät nicht hängen bleibt: Dann ist der Ton weg, bis die Last sinkt.
    [[nodiscard]] float getDspLoad() const;

    /// Position, an der die laufende Aufnahme begonnen hat (für die Live-Anzeige in den Spuren).
    [[nodiscard]] double getRecordStartSeconds() const noexcept;

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void attachToCurrentEdit();
    void timerCallback() override;
    [[nodiscard]] double getRollSetting (const juce::Identifier& property) const;
    void setRollSetting (const juce::Identifier& property, double seconds, const juce::String& undoName);
    void notifyStateChanged();

    ProjectService& projectService;
    te::TransportControl* attachedTransport = nullptr; // nur geliehen, gehört dem Edit
    double recordStartSeconds = 0.0;
    std::optional<double> stopAtSeconds; ///< Post-Roll: hier stoppen (nur während einer Punch-Aufnahme)
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportService)
};

} // namespace fk::services
