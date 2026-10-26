#include "fk/plugins/EqView.h"

#include "fk/plugins/EqProcessor.h"
#include "fk/plugins/ui/PluginTheme.h"

namespace fk::plugins
{

EqView::EqView (EqProcessor& processor)
    : spectrum (processor.getSpectrumAnalyser())
    , curve ([&processor] (float hz) { return processor.getMagnitudeDb (hz); })
{
    spectrum.setDecibelRange (-24.0f, 24.0f);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (curve);
    addAndMakeVisible (handles);

    auto& parameters = processor.getParameterState();
    const std::array<juce::Colour, 5> colours {
        ui::theme::textSecondary, ui::theme::accent, ui::theme::meterLow, ui::theme::meterMid, ui::theme::meterHigh
    };
    int index = 0;

    for (const auto& band : processor.describeBands())
    {
        auto* gain = band.gainId.isEmpty() ? nullptr : parameters.getParameter (band.gainId);
        auto* q = band.qId.isEmpty() ? nullptr : parameters.getParameter (band.qId);
        handles.addHandle (*parameters.getParameter (band.frequencyId),
                           gain,
                           q,
                           colours[static_cast<std::size_t> (index % static_cast<int> (colours.size()))],
                           band.shortLabel);
        ++index;
    }

    // Griff bewegt → Parameter → Kurve neu zeichnen. Der Umweg über den Parameter ist Absicht:
    // Automation und Undo laufen denselben Weg, die Kurve stimmt immer mit dem Klang überein.
    handles.onChanged = [this] { curve.refresh(); };
}

void EqView::resized()
{
    const auto area = getLocalBounds();
    spectrum.setBounds (area);
    curve.setBounds (area);
    handles.setBounds (area);
    curve.setScale (spectrum.getScale());
    handles.setScale (spectrum.getScale());
}

} // namespace fk::plugins
