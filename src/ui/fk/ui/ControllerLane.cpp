#include "fk/ui/ControllerLane.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <cmath>

namespace fk::ui
{

namespace
{
// Feste Einträge der Artenliste; danach folgen alle übrigen CCs 0..127.
constexpr int presetTypes[] = { services::controllerTypes::sustain,   services::controllerTypes::sostenuto,
                                services::controllerTypes::soft,      services::controllerTypes::modWheel,
                                services::controllerTypes::pitchBend, services::controllerTypes::aftertouch };
constexpr int typeItemOffset = 1000; // ComboBox-IDs müssen > 0 sein: ID = Art + Offset
} // namespace

ControllerLane::ControllerLane (services::MidiControllerService& midiControllerService)
    : midiControllers (midiControllerService)
    , removeButton (Icon::minus, TRANS ("Remove this controller lane"))
{
    setTooltip (TRANS ("Controller lane: click sets a point, dragging draws, dragging a point moves it, right-click deletes"));
    rebuildTypeChoices();
    typeBox.setTooltip (TRANS ("Which controller data this lane shows"));
    typeBox.onChange = [this]
    {
        controllerType = typeBox.getSelectedId() - typeItemOffset;
        repaint();
    };
    removeButton.onClick = [this]
    {
        if (onRemoveRequested)
            onRemoveRequested();
    };
    addAndMakeVisible (typeBox);
    addAndMakeVisible (removeButton);
}

void ControllerLane::rebuildTypeChoices()
{
    typeBox.clear (juce::dontSendNotification);

    for (const int type : presetTypes)
        typeBox.addItem (services::controllerTypes::name (type), type + typeItemOffset);

    juce::PopupMenu others;

    for (int cc = 0; cc < 128; ++cc)
        if (std::find (std::begin (presetTypes), std::end (presetTypes), cc) == std::end (presetTypes))
            others.addItem (cc + typeItemOffset, services::controllerTypes::name (cc));

    typeBox.getRootMenu()->addSubMenu (TRANS ("More CCs"), others);
    typeBox.setSelectedId (controllerType + typeItemOffset, juce::dontSendNotification);
}

void ControllerLane::setClip (std::optional<services::MidiClipInfo> newClip)
{
    clip = std::move (newClip);
    drag.reset();
    repaint();
}

void ControllerLane::setPixelsPerBeat (double pixels)
{
    pixelsPerBeat = pixels;
    repaint();
}

void ControllerLane::setScrollX (int x)
{
    if (scrollX == x)
        return;

    scrollX = x;
    repaint();
}

void ControllerLane::setGridBeats (double beats)
{
    gridBeats = beats;
}

void ControllerLane::setControllerType (int type)
{
    controllerType = type;
    typeBox.setSelectedId (type + typeItemOffset, juce::dontSendNotification);
    repaint();
}

int ControllerLane::getControllerType() const noexcept
{
    return controllerType;
}

juce::Rectangle<int> ControllerLane::plotArea() const
{
    return getLocalBounds().withTrimmedLeft (headerWidth);
}

int ControllerLane::xForBeat (double beat) const
{
    return headerWidth + static_cast<int> (std::round (beat * pixelsPerBeat)) - scrollX;
}

double ControllerLane::beatForX (int x) const
{
    return juce::jmax (0.0, (x - headerWidth + scrollX) / pixelsPerBeat);
}

int ControllerLane::valueForY (int y) const
{
    const auto usable = static_cast<double> (getHeight() - topMargin);
    const auto maxValue = services::controllerTypes::maxValue (controllerType);
    return juce::jlimit (0, maxValue, static_cast<int> (std::round ((getHeight() - y) / usable * maxValue)));
}

int ControllerLane::yForValue (int value) const
{
    const auto usable = static_cast<double> (getHeight() - topMargin);
    const auto maxValue = services::controllerTypes::maxValue (controllerType);
    return getHeight() - static_cast<int> (std::round (static_cast<double> (value) / maxValue * usable));
}

const services::ControllerEvent* ControllerLane::findEventAt (juce::Point<int> position) const
{
    if (! clip)
        return nullptr;

    for (auto it = clip->controllers.rbegin(); it != clip->controllers.rend(); ++it)
    {
        if (it->type != controllerType)
            continue;

        const juce::Rectangle<int> hit (
            xForBeat (it->beats) - pointSize, yForValue (it->value) - pointSize, 2 * pointSize, 2 * pointSize);

        if (hit.contains (position))
            return &*it;
    }

    return nullptr;
}

void ControllerLane::resized()
{
    // Kopf so breit wie die Klaviatur: oben der Entfernen-Button; die Artenwahl liegt oben links
    // über der Kurve, damit die Zeitachse bündig unter dem Notenraster bleibt.
    removeButton.setBounds (getLocalBounds().withWidth (headerWidth).reduced (theme::spacingSmall).removeFromTop (22));
    typeBox.setBounds (headerWidth + theme::spacingSmall, theme::spacingSmall, typeBoxWidth, 20);
}

void ControllerLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundInput);
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));
    g.drawVerticalLine (headerWidth - 1, 0.0f, static_cast<float> (getHeight()));

    const auto area = plotArea();
    const auto maxValue = services::controllerTypes::maxValue (controllerType);
    g.setColour (theme::gridBeat);

    for (const int fraction : { 1, 2, 3 })
        g.drawHorizontalLine (yForValue (maxValue * fraction / 4),
                              static_cast<float> (area.getX()),
                              static_cast<float> (area.getRight()));

    if (! clip)
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (TRANS ("No clip"), area, juce::Justification::centred);
        return;
    }

    // Stufenlinie: Jeder Punkt gilt bis zum nächsten (so sendet MIDI Controller).
    g.saveState();
    g.reduceClipRegion (area);
    const int defaultValue = services::controllerTypes::defaultValue (controllerType);
    int lastY = yForValue (defaultValue);
    int lastX = area.getX();
    bool any = false;
    g.setColour (theme::accent);

    auto drawPoint = [&] (double beats, int value, bool highlighted)
    {
        const int x = xForBeat (beats);
        const int y = yForValue (value);
        g.setColour (theme::accent.withAlpha (0.8f));
        g.drawHorizontalLine (lastY, static_cast<float> (lastX), static_cast<float> (x));
        g.drawVerticalLine (x, static_cast<float> (juce::jmin (lastY, y)), static_cast<float> (juce::jmax (lastY, y)));
        g.setColour (highlighted ? theme::textPrimary : theme::accent);
        g.fillEllipse (static_cast<float> (x - pointSize / 2),
                       static_cast<float> (y - pointSize / 2),
                       static_cast<float> (pointSize),
                       static_cast<float> (pointSize));
        lastX = x;
        lastY = y;
        any = true;
    };

    for (const auto& event : clip->controllers)
    {
        if (event.type != controllerType)
            continue;

        // Beim Zeichnen ist der neue Bereich schon ohne alte Punkte zu sehen.
        if (drag && drag->drawing && event.beats >= drag->drawFromBeats && event.beats < drag->drawToBeats)
            continue;

        const bool moving = drag && ! drag->drawing && drag->eventIndex == event.index;
        drawPoint (moving ? drag->beats : event.beats, moving ? drag->value : event.value, moving);
    }

    if (drag && drag->drawing)
        for (const auto& [beats, value] : drag->drawn)
            drawPoint (beats, value, true);

    g.setColour (theme::accent.withAlpha (0.8f));
    g.drawHorizontalLine (lastY, static_cast<float> (lastX), static_cast<float> (area.getRight()));
    g.restoreState();

    if (! any && ! drag)
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (TRANS ("No data: click or drag to set points"),
                    area.reduced (theme::spacingMedium, 0),
                    juce::Justification::centredLeft);
    }

    if (drag)
    {
        g.setColour (theme::textPrimary);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        const auto value = drag->drawing && ! drag->drawn.empty() ? drag->drawn.back().second : drag->value;
        g.drawText (juce::String (value),
                    theme::spacingSmall,
                    getHeight() - 16,
                    headerWidth - 2 * theme::spacingSmall,
                    14,
                    juce::Justification::centredLeft);
    }
}

void ControllerLane::mouseDown (const juce::MouseEvent& event)
{
    if (! clip || event.x < headerWidth)
        return;

    const auto* hit = findEventAt (event.getPosition());

    if (event.mods.isPopupMenu())
    {
        if (hit != nullptr)
            if (const auto result = midiControllers.removeControllerEvent (clip->id, hit->index); ! result)
                reportError (result.error());

        return;
    }

    if (hit != nullptr)
    {
        drag = Drag { false, hit->index, hit->beats, hit->value, 0.0, 0.0, {} };
        repaint();
        return;
    }

    // Zeichnen: erster Punkt am Rasterschritt unter der Maus.
    const auto beats = std::floor (beatForX (event.x) / gridBeats) * gridBeats;
    drag = Drag { true, -1, beats, valueForY (event.y), beats, beats + gridBeats, { { beats, valueForY (event.y) } } };
    repaint();
}

void ControllerLane::mouseDrag (const juce::MouseEvent& event)
{
    if (! drag)
        return;

    if (! drag->drawing)
    {
        drag->beats = std::round (beatForX (event.x) / gridBeats) * gridBeats;
        drag->value = valueForY (event.y);
        repaint();
        return;
    }

    // Freihand nach rechts: je Rasterschritt ein Punkt mit dem aktuellen Wert; nach links wird nicht gezeichnet.
    const auto beats = std::floor (beatForX (event.x) / gridBeats) * gridBeats;
    const auto value = valueForY (event.y);

    if (beats >= drag->drawToBeats)
    {
        for (double b = drag->drawToBeats; b <= beats + 1.0e-9; b += gridBeats)
            drag->drawn.emplace_back (b, value);

        drag->drawToBeats = beats + gridBeats;
    }
    else if (! drag->drawn.empty() && std::abs (drag->drawn.back().first - beats) < 1.0e-9)
    {
        drag->drawn.back().second = value;
    }

    repaint();
}

void ControllerLane::mouseUp (const juce::MouseEvent&)
{
    if (! drag || ! clip)
    {
        drag.reset();
        return;
    }

    const auto finished = *drag;
    drag.reset();

    if (! finished.drawing)
    {
        if (const auto result =
                midiControllers.setControllerEvent (clip->id, finished.eventIndex, finished.beats, finished.value);
            ! result)
            reportError (result.error());

        return;
    }

    // Bereich räumen und die gezeichneten Punkte setzen: ein Undo-Schritt.
    if (const auto result = midiControllers.replaceControllerEvents (
            clip->id, controllerType, finished.drawFromBeats, finished.drawToBeats, finished.drawn);
        ! result)
        reportError (result.error());
}

void ControllerLane::reportError (const core::Error& error)
{
    juce::Logger::writeToLog ("Controller-Lane: " + services::errorText (error));

    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
