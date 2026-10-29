#pragma once

#include "fk/ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

/// Werkzeug für die Bilder des Benutzerhandbuchs (docs/manual/MAINTAINING.md): Szenen zeichnen echte
/// Komponenten, markieren Bedienelemente mit Nummern und schreiben PNGs nach FK_MANUAL_IMAGE_DIR.
namespace fk::tests::manual
{

/// Zielordner der Bilder oder leer; ohne Ordner überspringt die Szene (normaler Testlauf).
inline juce::File imageFolder()
{
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_MANUAL_IMAGE_DIR", {});
    return directory.isEmpty() ? juce::File() : juce::File (directory);
}

/// Erstes Kind (beliebig tief), dessen Tooltip mit dem Text beginnt. Fehlt es, ist das Kapitel veraltet:
/// Die Szene schlägt fehl, statt eine Markierung an die falsche Stelle zu setzen.
inline juce::Component& findByTooltip (juce::Component& root, const juce::String& tooltipStart)
{
    std::function<juce::Component*(juce::Component&)> search = [&] (juce::Component& parent) -> juce::Component*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* client = dynamic_cast<juce::TooltipClient*> (child))
                if (child->isVisible() && client->getTooltip().startsWith (tooltipStart))
                    return child;

            if (auto* found = search (*child))
                return found;
        }

        return nullptr;
    };

    auto* found = search (root);
    INFO ("Kein sichtbares Element mit Tooltip \"" << tooltipStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Linke Maustaste in Koordinaten des Ziels, um Szenen per Mausgeste aufzubauen (Auswahlrechteck, Ziehen).
inline juce::MouseEvent mouseEvent (juce::Component& target, juce::Point<float> position, juce::Point<float> down)
{
    return { juce::Desktop::getInstance().getMainMouseSource(),
             position,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &target,
             &target,
             juce::Time::getCurrentTime(),
             down,
             juce::Time::getCurrentTime(),
             1,
             position != down };
}

/// Eine nummerierte Markierung: über dem Bild (Werkzeugleiste) oder in der linken oberen Ecke des Ziels.
struct Callout
{
    enum class Placement
    {
        above,
        inside
    };

    int number = 1;
    juce::Rectangle<int> target; ///< in Koordinaten der fotografierten Komponente
    Placement placement = Placement::inside;
};

/// Markierung für ein Kind der fotografierten Komponente, beschnitten auf den sichtbaren Teil
/// (ein Raster im Viewport ist viel größer als sein Ausschnitt).
inline Callout calloutFor (int number, juce::Component& root, juce::Component& target, Callout::Placement placement)
{
    auto area = root.getLocalArea (&target, target.getLocalBounds());

    for (auto* parent = target.getParentComponent(); parent != nullptr && parent != &root; parent = parent->getParentComponent())
        area = area.getIntersection (root.getLocalArea (parent, parent->getLocalBounds()));

    return { number, area.getIntersection (root.getLocalBounds()), placement };
}

/// Fotografiert die Komponente (oder den Ausschnitt area), zeichnet die Markierungen und schreibt
/// images/<name>.png. Ausschnitte halten breite Leisten im PDF lesbar (A4: rund 900 px Bildbreite).
inline void saveShot (juce::Component& component,
                      const juce::String& name,
                      std::vector<Callout> callouts = {},
                      std::optional<juce::Rectangle<int>> area = std::nullopt)
{
    const auto shotArea = area.value_or (component.getLocalBounds());

    for (auto& callout : callouts)
        callout.target = callout.target.getIntersection (shotArea).translated (-shotArea.getX(), -shotArea.getY());

    constexpr float badge = 18.0f;
    constexpr int strip = 28; // Rand über dem Bild für Markierungen der Werkzeugleiste

    const bool needsStrip = std::ranges::any_of (callouts, [] (const Callout& callout)
                                                 { return callout.placement == Callout::Placement::above; });
    const int top = needsStrip ? strip : 0;
    const auto snapshot = component.createComponentSnapshot (shotArea);

    // Lage der Nummer: über dem Bild (Leisten), rechts neben kleinen Zielen (Listen, Buttons; verdeckt
    // keine Beschriftung) oder innen oben rechts in Flächen (links stehen meist Beschriftungen).
    const auto circleFor = [top] (const Callout& callout)
    {
        const auto target = callout.target.translated (0, top);

        if (callout.placement == Callout::Placement::above)
            return juce::Rectangle<float> (static_cast<float> (target.getCentreX()) - badge / 2.0f, 4.0f, badge, badge);

        const bool small = target.getHeight() < 40 || target.getWidth() < 80;
        const auto x = small ? static_cast<float> (target.getRight()) + 4.0f : static_cast<float> (target.getRight()) - badge - 4.0f;
        const auto y = small ? static_cast<float> (target.getCentreY()) - badge / 2.0f : static_cast<float> (target.getY()) + 4.0f;
        return juce::Rectangle<float> (x, y, badge, badge);
    };

    // Ragt eine Nummer rechts heraus, wird das Bild um einen Rand verbreitert statt sie abzuschneiden.
    float rightmost = 0.0f;

    for (const auto& callout : callouts)
        rightmost = std::max (rightmost, circleFor (callout).getRight());

    const int width = std::max (snapshot.getWidth(), static_cast<int> (std::ceil (rightmost)) + 4);

    juce::Image image (juce::Image::ARGB, width, snapshot.getHeight() + top, true);
    juce::Graphics g (image);
    g.fillAll (ui::theme::backgroundBase);
    g.drawImageAt (snapshot, 0, top);

    for (const auto& callout : callouts)
    {
        const auto target = callout.target.translated (0, top);
        const auto circle = circleFor (callout);
        g.setColour (ui::theme::solo);

        if (callout.placement == Callout::Placement::above)
            g.drawLine (circle.getCentreX(), circle.getBottom(), circle.getCentreX(), static_cast<float> (target.getY()) + 2.0f, 1.5f);
        else
            g.drawRect (target, 2);

        g.fillEllipse (circle);
        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (callout.number), circle, juce::Justification::centred);
    }

    const auto file = imageFolder().getChildFile (name + ".png");
    REQUIRE (file.getParentDirectory().createDirectory());
    file.deleteFile(); // FileOutputStream hängt sonst an das alte Bild an
    juce::FileOutputStream stream (file);
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
}

} // namespace fk::tests::manual
