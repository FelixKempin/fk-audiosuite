#include "fk/dsp/delay/FractionalDelayLine.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace fk::dsp::delay
{

void FractionalDelayLine::prepare (std::size_t maximumDelaySamples)
{
    // Zwei Samples Reserve hinter dem Lesepunkt für die Interpolation.
    const std::size_t size = std::bit_ceil (maximumDelaySamples + 3);
    buffer.assign (size, 0.0f);
    mask = size - 1;
    writeIndex = 0;
    maximumDelay = static_cast<float> (std::max (maximumDelaySamples, std::size_t { 1 }));
}

void FractionalDelayLine::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void FractionalDelayLine::push (float input) noexcept
{
    writeIndex = (writeIndex + 1) & mask;
    buffer[writeIndex] = input;
}

float FractionalDelayLine::at (std::size_t delay) const noexcept
{
    return buffer[(writeIndex - delay) & mask];
}

float FractionalDelayLine::read (float delaySamples) const noexcept
{
    const float delay = std::clamp (delaySamples, minimumDelay, maximumDelay);
    const float whole = std::floor (delay);
    const auto index = static_cast<std::size_t> (whole);
    const float f = delay - whole;

    // Lagrange-Gewichte für die Stützstellen −1, 0, 1, 2 um den ganzzahligen Teil.
    const float fPlus1 = f + 1.0f;
    const float fMinus1 = f - 1.0f;
    const float fMinus2 = f - 2.0f;
    const float wBefore = -f * fMinus1 * fMinus2 * (1.0f / 6.0f);
    const float wAt = fPlus1 * fMinus1 * fMinus2 * 0.5f;
    const float wAfter = -fPlus1 * f * fMinus2 * 0.5f;
    const float wAfterNext = fPlus1 * f * fMinus1 * (1.0f / 6.0f);

    return wBefore * at (index - 1) + wAt * at (index) + wAfter * at (index + 1) + wAfterNext * at (index + 2);
}

float FractionalDelayLine::getMaximumDelay() const noexcept
{
    return maximumDelay;
}

} // namespace fk::dsp::delay
