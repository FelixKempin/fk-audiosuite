#include "fk/ui/Theme.h"

#include "fk/services/TrackColours.h"

#include <array>

namespace fk::ui::theme
{

juce::Colour trackColourForIndex (int index)
{
    return services::paletteColour (index);
}

} // namespace fk::ui::theme
