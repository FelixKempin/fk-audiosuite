#include "fk/dsp/nonlinear/Waveshaper.h"

#include "fk/dsp/units/Decibels.h"

#include <algorithm>
#include <cmath>

namespace fk::dsp::nonlinear
{

void Waveshaper::setShape (ShapeType type) noexcept
{
    shapeType = type;
}

void Waveshaper::setDriveDb (float driveDb) noexcept
{
    driveLinear = units::decibelsToGain (driveDb);
}

void Waveshaper::setOutputDb (float outputDb) noexcept
{
    outputLinear = units::decibelsToGain (outputDb);
}

float Waveshaper::shape (float input, ShapeType type) noexcept
{
    switch (type)
    {
        case ShapeType::soft:
            return std::tanh (input);

        case ShapeType::hard:
            return std::clamp (input, -1.0f, 1.0f);

        case ShapeType::tube:
        {
            // Positive Halbwelle weicher als die negative: erzeugt geradzahlige Obertöne.
            const float positive = std::tanh (input);
            const float negative = std::tanh (input * 1.5f) / 1.5f;
            return input >= 0.0f ? positive : negative;
        }
    }

    return input;
}

void Waveshaper::process (std::span<float> buffer) const noexcept
{
    for (float& sample : buffer)
        sample = shape (sample * driveLinear, shapeType) * outputLinear;
}

} // namespace fk::dsp::nonlinear
