#include "fk/services/ControllerTypes.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace fk::services
{

namespace controllerTypes
{
bool isPitchBend (int type) noexcept
{
    return type == pitchBend;
}

int maxValue (int type) noexcept
{
    return isPitchBend (type) ? 16383 : 127;
}

int defaultValue (int type) noexcept
{
    return isPitchBend (type) ? 8192 : 0;
}

juce::String name (int type)
{
    switch (type)
    {
        case modWheel:
            return TRANS ("Mod wheel (CC1)");
        case sustain:
            return TRANS ("Sustain (CC64)");
        case sostenuto:
            return TRANS ("Sostenuto (CC66)");
        case soft:
            return TRANS ("Soft (CC67)");
        case pitchBend:
            return TRANS ("Pitch bend");
        case aftertouch:
            return TRANS ("Aftertouch");
        case polyAftertouch:
            return TRANS ("Poly aftertouch");
        default:
            break;
    }

    const auto ccName = juce::MidiMessage::getControllerName (type);
    return "CC" + juce::String (type) + (ccName != nullptr ? " (" + juce::String (ccName) + ")" : juce::String());
}
} // namespace controllerTypes

} // namespace fk::services
