#include "fk/services/TimeFormat.h"

#include <cmath>

namespace fk::services
{

juce::String formatTime (double seconds, TimeDisplay display, double sampleRate, int framesPerSecond)
{
    seconds = std::max (seconds, 0.0);

    switch (display)
    {
        case TimeDisplay::timecode:
        {
            const auto fps = std::max (framesPerSecond, 1);
            // Frames abrunden: Frame 11 läuft, bis Frame 12 beginnt (wie jede Timecode-Anzeige).
            const auto totalFrames = static_cast<long long> (std::floor (seconds * fps + 1.0e-9));
            const auto frames = totalFrames % fps;
            const auto totalSeconds = totalFrames / fps;
            return juce::String::formatted ("%02lld:%02lld:%02lld:%02lld",
                                            totalSeconds / 3600,
                                            (totalSeconds / 60) % 60,
                                            totalSeconds % 60,
                                            frames);
        }

        case TimeDisplay::samples:
        {
            // Tausendergruppen mit schmalem Leerzeichen lesen sich bei großen Zahlen leichter.
            auto digits = juce::String (static_cast<juce::int64> (std::llround (seconds * sampleRate)));
            juce::String grouped;

            for (int i = 0; i < digits.length(); ++i)
            {
                if (i > 0 && (digits.length() - i) % 3 == 0)
                    grouped << juce::String::charToString (0x202f);

                grouped << digits[i];
            }

            return grouped;
        }

        case TimeDisplay::seconds:
            break;
    }

    const auto totalMilliseconds = static_cast<long long> (std::llround (seconds * 1000.0));
    return juce::String::formatted (
        "%lld:%02lld.%03lld", totalMilliseconds / 60000, (totalMilliseconds / 1000) % 60, totalMilliseconds % 1000);
}

juce::String getTimeDisplayName (TimeDisplay display)
{
    switch (display)
    {
        case TimeDisplay::timecode:
            return TRANS ("Timecode");
        case TimeDisplay::samples:
            return TRANS ("Samples");
        case TimeDisplay::seconds:
            break;
    }

    return TRANS ("Time (min:sec)");
}

} // namespace fk::services
