#include "fk/services/NoteValue.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

double NoteValue::beats() const noexcept
{
    const double straightBeats = 4.0 / static_cast<double> (denominator);

    switch (feel)
    {
        case Feel::dotted:
            return straightBeats * 1.5;
        case Feel::triplet:
            return straightBeats * 2.0 / 3.0;
        case Feel::straight:
            break;
    }

    return straightBeats;
}

juce::String NoteValue::name() const
{
    const auto base = "1/" + juce::String (denominator);

    switch (feel)
    {
        case Feel::dotted:
            return plugins::i18n::format (TRANS ("{note} dotted"), { { "note", base } });
        case Feel::triplet:
            return plugins::i18n::format (TRANS ("{note} triplet"), { { "note", base } });
        case Feel::straight:
            break;
    }

    return base;
}

juce::String NoteValue::shortName() const
{
    const auto base = "1/" + juce::String (denominator);

    switch (feel)
    {
        case Feel::dotted:
            return base + ".";
        case Feel::triplet:
            return base + "T";
        case Feel::straight:
            break;
    }

    return base;
}

std::vector<NoteValue> NoteValue::all()
{
    std::vector<NoteValue> values;

    for (int denominator = 1; denominator <= 128; denominator *= 2)
        for (const auto feel : { Feel::straight, Feel::dotted, Feel::triplet })
            values.push_back ({ denominator, feel });

    return values;
}

} // namespace fk::services
