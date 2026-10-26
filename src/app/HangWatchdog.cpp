#include "HangWatchdog.h"

#if JUCE_LINUX
#include <sys/prctl.h>
#include <unistd.h>
#endif

namespace fk::app
{

namespace
{
constexpr int heartbeatIntervalMs = 250;
constexpr int checkIntervalMs = 500;
constexpr int gdbTimeoutMs = 30000;
} // namespace

HangWatchdog::HangWatchdog (juce::File reportFolder, AudioProbe probe)
    : juce::Thread ("FK Hang Watchdog")
    , folder (std::move (reportFolder))
    , audio (std::move (probe))
{
#if JUCE_LINUX
    // Ubuntu erlaubt ptrace nur auf Kindprozesse (yama ptrace_scope = 1). gdb ist hier ein Kind
    // der App und will den Elternprozess lesen: das erlauben wir ausdrücklich.
    prctl (PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif

    lastHeartbeatMs = juce::Time::getMillisecondCounterHiRes();
    startTimer (heartbeatIntervalMs);
    startThread (juce::Thread::Priority::low);
}

HangWatchdog::~HangWatchdog()
{
    stopTimer();
    stopThread (2000);
}

void HangWatchdog::timerCallback()
{
    lastHeartbeatMs = juce::Time::getMillisecondCounterHiRes();
}

void HangWatchdog::run()
{
    bool reported = false;

    while (! threadShouldExit())
    {
        wait (checkIntervalMs);
        const double hungFor = (juce::Time::getMillisecondCounterHiRes() - lastHeartbeatMs.load()) * 0.001;

        if (hungFor < hangSeconds)
        {
            reported = false; // wieder lebendig: der nächste Hänger bekommt einen eigenen Bericht
            continue;
        }

        if (! reported)
        {
            reported = true;
            writeReport (hungFor);
        }
    }
}

void HangWatchdog::writeReport (double hungForSeconds)
{
    folder.createDirectory();
    const auto now = juce::Time::getCurrentTime();
    const auto file = folder.getChildFile ("haenger-" + now.formatted ("%Y%m%d-%H%M%S") + ".txt");

    // Läuft der Audio-Strom noch? Stream-Zeit vor und nach einer halben Sekunde vergleichen.
    const double streamBefore = audio.streamTime ? audio.streamTime() : 0.0;
    wait (500);
    const double streamAfter = audio.streamTime ? audio.streamTime() : 0.0;

    juce::String report;
    report << juce::String::fromUTF8 ("FK Audio Suite: Oberfläche reagiert seit ") << juce::String (hungForSeconds, 1)
           << " s nicht\n"
           << "Zeit: " << now.toString (true, true) << "\n"
           << "Audio-Last: " << juce::String (juce::roundToInt ((audio.dspLoad ? audio.dspLoad() : 0.0f) * 100.0f))
           << " %\n"
           << "Audio-Strom: "
           << (streamAfter > streamBefore ? juce::String::fromUTF8 ("läuft") : juce::String ("steht")) << "\n\n";

#if JUCE_LINUX
    const auto processId = juce::String (static_cast<int> (getpid()));
#else
    const auto processId = juce::String(); // Stack-Traces per gdb nur unter Linux
#endif

    // Stack-Traces aller Threads. gdb hält dafür den ganzen Prozess an, auch diesen Thread; es
    // schreibt deshalb direkt in eine Datei statt in eine Pipe, die hier niemand leeren könnte.
    const auto traces = file.withFileExtension ("gdb.txt");
    const juce::StringArray command { "gdb",     "-p",
                                      processId, "-batch",
                                      "-ex",     "set pagination off",
                                      "-ex",     "set logging file " + traces.getFullPathName(),
                                      "-ex",     "set logging overwrite on",
                                      "-ex",     "set logging redirect on",
                                      "-ex",     "set logging enabled on",
                                      "-ex",     "thread apply all bt 40" };
    juce::ChildProcess gdb;

    if (processId.isNotEmpty() && gdb.start (command, 0))
    {
        gdb.waitForProcessToFinish (gdbTimeoutMs);
        report << traces.loadFileAsString();
        traces.deleteFile();
    }
    else
    {
        report << juce::String::fromUTF8 ("gdb nicht verfügbar: keine Stack-Traces.\n");
    }

    file.replaceWithText (report);
    juce::Logger::writeToLog (juce::String::fromUTF8 ("Hänger erkannt, Bericht: ") + file.getFullPathName());
}

} // namespace fk::app
