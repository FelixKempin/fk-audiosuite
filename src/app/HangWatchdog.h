#pragma once

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>

namespace fk::app
{

/// Erkennt, wenn die Oberfläche hängt, und hält fest, wo. Ein Timer auf dem Message-Thread
/// setzt einen Herzschlag; ein eigener Thread prüft ihn. Bleibt er länger als hangSeconds aus,
/// schreibt der Wächter einmal je Hänger einen Bericht: Stack-Traces aller Threads (über gdb),
/// Audio-Last und ob der Audio-Strom noch läuft. So lässt sich ein "manchmal reagiert nichts"
/// hinterher erklären.
class HangWatchdog final : private juce::Thread, private juce::Timer
{
public:
    /// Liefert die aktuelle Audio-Last (0..1) und die Stream-Zeit des Audio-Geräts in Sekunden;
    /// wird aus dem Wächter-Thread gerufen und darf nur Atomics lesen.
    struct AudioProbe
    {
        std::function<float()> dspLoad;
        std::function<double()> streamTime;
    };

    HangWatchdog (juce::File reportFolder, AudioProbe probe);
    ~HangWatchdog() override;

    static constexpr double hangSeconds = 4.0;

private:
    void timerCallback() override;
    void run() override;
    void writeReport (double hungForSeconds);

    juce::File folder;
    AudioProbe audio;
    std::atomic<double> lastHeartbeatMs { 0.0 };

    JUCE_DECLARE_NON_COPYABLE (HangWatchdog)
};

} // namespace fk::app
