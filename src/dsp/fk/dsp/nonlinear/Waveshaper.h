#pragma once

#include <span>

namespace fk::dsp::nonlinear
{

/// Form der Verzerrungskennlinie.
enum class ShapeType
{
    soft, ///< tanh: weiche Sättigung, röhrenähnlich
    hard, ///< hartes Abschneiden bei ±1, aggressiv
    tube  ///< asymmetrisch: geradzahlige Obertöne, "warm"
};

/// Verzerrer: Eingang verstärken (Drive), durch eine Kennlinie schicken, Ausgang anpassen.
/// Kein Oversampling hier; das macht die Plugin-Schicht, weil es Puffer braucht.
///
/// Echtzeit: process() allokiert nicht.
class Waveshaper final
{
public:
    void setShape (ShapeType type) noexcept;
    void setDriveDb (float driveDb) noexcept;
    void setOutputDb (float outputDb) noexcept;

    /// Die reine Kennlinie, ohne Drive und Ausgang. Für Tests und Anzeige.
    [[nodiscard]] static float shape (float input, ShapeType type) noexcept;

    void process (std::span<float> buffer) const noexcept;

private:
    ShapeType shapeType = ShapeType::soft;
    float driveLinear = 1.0f;
    float outputLinear = 1.0f;
};

} // namespace fk::dsp::nonlinear
