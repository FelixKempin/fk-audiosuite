#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

namespace fk::dsp::delay
{

/// Verzögerungsleitung mit ganzzahliger Verzögerung (Ringpuffer), ein Kanal.
/// Speicher wird nur in prepare() angefordert; push/read sind echtzeitsicher.
class DelayLine final
{
public:
    /// Reserviert Platz für maxDelaySamples. Nicht im Audio-Thread aufrufen.
    void prepare (std::size_t maxDelaySamples)
    {
        buffer.assign (maxDelaySamples + 1, 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    /// Schreibt ein Sample und liefert das um delaySamples ältere zurück.
    [[nodiscard]] float pushAndRead (float input, std::size_t delaySamples) noexcept
    {
        assert (! buffer.empty() && delaySamples < buffer.size());

        buffer[writeIndex] = input;
        const std::size_t readIndex = (writeIndex + buffer.size() - delaySamples) % buffer.size();
        writeIndex = (writeIndex + 1) % buffer.size();
        return buffer[readIndex];
    }

private:
    std::vector<float> buffer;
    std::size_t writeIndex = 0;
};

} // namespace fk::dsp::delay
