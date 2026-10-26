#include "fk/ui/MarkerTrack.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Icons.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int flagWidth = 110; // Name rechts neben der Fahne; Treffer-Breite für Klicks

} // namespace

MarkerTrack::MarkerTrack (services::MarkerService& markerService,
                          services::TransportService& transportService,
                          services::TimelineService& timelineService)
    : markers (markerService)
    , transport (transportService)
    , timeline (timelineService)
    , addButton (Icon::markerAdd, TRANS ("Add marker at the cursor (Insert)"))
{
    setTooltip (TRANS ("Marker: click jumps there, dragging moves, double-click renames or adds a new one, right-click: loop, rename, delete"));
    addButton.onClick = [this] { addMarkerAt (transport.getPositionSeconds()); };
    addAndMakeVisible (addButton);
    markers.addListener (*this);
    markersChanged();
}

MarkerTrack::~MarkerTrack()
{
    markers.removeListener (*this);
}

void MarkerTrack::setSettings (LaneSettings newSettings)
{
    settings = newSettings;
    repaint();
}

void MarkerTrack::markersChanged()
{
    markerList = markers.getMarkers();
    repaint();
}

int MarkerTrack::xForSeconds (double seconds) const
{
    return theme::trackHeaderWidth + settings.mapping.secondsToX (seconds);
}

double MarkerTrack::secondsForX (int x, bool snap) const
{
    const auto seconds = juce::jmax (0.0, settings.mapping.xToSeconds (x - theme::trackHeaderWidth));
    return snap && settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;
}

int MarkerTrack::findMarkerAt (int x) const
{
    // Von hinten suchen: Bei Überlappung liegt der spätere Marker oben (wird zuletzt gezeichnet).
    for (int i = static_cast<int> (markerList.size()); --i >= 0;)
    {
        const auto markerX = xForSeconds (markerList[static_cast<std::size_t> (i)].startSeconds);

        if (x >= markerX - 3 && x < markerX + flagWidth)
            return i;
    }

    return -1;
}

void MarkerTrack::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
    g.setColour (theme::backgroundInput);
    g.fillRect (getLocalBounds().withTrimmedLeft (theme::trackHeaderWidth));

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (theme::textSecondary);
    g.drawText (TRANS ("Marker"),
                juce::Rectangle<int> (theme::spacingMedium + theme::spacingSmall, 0, theme::trackHeaderWidth, height),
                juce::Justification::centredLeft);

    // Nur der Zeitachsen-Bereich: Marker links der Spurköpfe nicht in die Beschriftung zeichnen.
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedLeft (theme::trackHeaderWidth));

    for (const auto& marker : markerList)
    {
        const bool dragged = drag && drag->markerId == marker.id;
        const auto x = xForSeconds (dragged ? drag->seconds : marker.startSeconds);

        if (x > getWidth())
            continue;

        g.setColour (theme::accent);
        g.fillRect (x, 0, 2, height);
        juce::Path flag;
        flag.addTriangle (
            static_cast<float> (x + 2), 2.0f, static_cast<float> (x + 10), 6.0f, static_cast<float> (x + 2), 10.0f);
        g.fillPath (flag);
        g.setColour (dragged ? theme::accent : theme::textPrimary);
        g.drawText (marker.name, x + 12, 0, flagWidth - 14, height, juce::Justification::centredLeft, true);
    }

    g.restoreState();
    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    g.drawVerticalLine (theme::trackHeaderWidth - 1, 0.0f, static_cast<float> (getHeight()));
}

void MarkerTrack::resized()
{
    addButton.setBounds (theme::trackHeaderWidth - height - theme::spacingSmall, 0, height, height);
}

void MarkerTrack::mouseDown (const juce::MouseEvent& event)
{
    if (event.x < theme::trackHeaderWidth)
        return;

    const auto index = findMarkerAt (event.x);

    if (index < 0)
        return;

    const auto& marker = markerList[static_cast<std::size_t> (index)];

    if (event.mods.isPopupMenu())
    {
        showMarkerMenu (marker);
        return;
    }

    drag = Drag { marker.id, marker.startSeconds, false };
}

void MarkerTrack::mouseDrag (const juce::MouseEvent& event)
{
    if (! drag || (! drag->moved && event.getDistanceFromDragStart() < 4))
        return;

    drag->moved = true;
    drag->seconds = secondsForX (event.x, ! event.mods.isShiftDown()); // Shift: ohne Raster
    repaint();
}

void MarkerTrack::mouseUp (const juce::MouseEvent&)
{
    if (! drag)
        return;

    const auto finished = *drag;
    drag.reset();

    if (finished.moved)
    {
        if (const auto result = markers.moveMarker (finished.markerId, finished.seconds); ! result)
            report (result.error());
    }
    else
    {
        // Einfacher Klick: dorthin springen, wie in Cubase.
        for (const auto& marker : markerList)
            if (marker.id == finished.markerId)
                transport.setPositionSeconds (marker.startSeconds);
    }

    repaint();
}

void MarkerTrack::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.x < theme::trackHeaderWidth)
        return;

    if (const auto index = findMarkerAt (event.x); index >= 0)
        askForName (markerList[static_cast<std::size_t> (index)]);
    else
        addMarkerAt (secondsForX (event.x, true));
}

void MarkerTrack::addMarkerAt (double seconds)
{
    if (const auto result = markers.addMarker (seconds); ! result)
        report (result.error());
}

void MarkerTrack::showMarkerMenu (const services::MarkerInfo& marker)
{
    enum
    {
        jump = 1,
        loop,
        rename,
        remove
    };

    juce::PopupMenu menu;
    menu.addSectionHeader (marker.name);
    menu.addItem (jump, TRANS ("Jump here"));
    menu.addItem (loop, TRANS ("Loop this section (up to the next marker)"));
    menu.addItem (rename, TRANS ("Rename..."));
    menu.addItem (remove, TRANS ("Delete"));

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safeThis = juce::Component::SafePointer (this), marker] (int chosen)
                        {
                            if (safeThis == nullptr)
                                return;

                            if (chosen == jump)
                            {
                                safeThis->transport.setPositionSeconds (marker.startSeconds);
                            }
                            else if (chosen == loop)
                            {
                                if (const auto section = safeThis->markers.getSection (marker.id))
                                {
                                    safeThis->timeline.setLoopRange ({ section->first, section->second });
                                    safeThis->timeline.setLooping (true);
                                }
                            }
                            else if (chosen == rename)
                            {
                                safeThis->askForName (marker);
                            }
                            else if (chosen == remove)
                            {
                                if (const auto result = safeThis->markers.deleteMarker (marker.id); ! result)
                                    safeThis->report (result.error());
                            }
                        });
}

void MarkerTrack::askForName (const services::MarkerInfo& marker)
{
    auto* window = new juce::AlertWindow (TRANS ("Rename marker"), {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", marker.name, TRANS ("Name"));
    window->addButton (TRANS ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // deleteWhenDismissed: JUCE löscht das Fenster nach dem Rückruf; bis dahin ist es gültig.
    window->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safeThis = juce::Component::SafePointer (this), window, id = marker.id] (int choice)
            {
                if (safeThis == nullptr || choice != 1)
                    return;

                if (const auto result = safeThis->markers.renameMarker (id, window->getTextEditorContents ("name"));
                    ! result)
                    safeThis->report (result.error());
            }),
        true);
}

void MarkerTrack::report (const core::Error& error) const
{
    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
