#include "fk/ui/TrackLane.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/ClipStretchPanel.h"
#include "fk/ui/Icons.h"
#include "fk/ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace fk::ui
{

TrackLane::TrackLane (services::TrackService& trackService,
                      services::ClipService& clipService,
                      services::MidiClipService& midiClipService,
                      services::TimelineService& timelineService,
                      services::WaveformService& waveformService,
                      services::TrackHandle& audioTrack,
                      juce::Colour trackColour)
    : tracks (trackService)
    , clips (clipService)
    , midiClips (midiClipService)
    , timeline (timelineService)
    , waveforms (waveformService)
    , track (audioTrack)
    , colour (trackColour)
{
    refresh();
}

void TrackLane::setSettings (LaneSettings newSettings)
{
    settings = newSettings;
    layoutClips();
    repaint();
}

void TrackLane::setRecordingPreview (std::optional<std::pair<double, double>> rangeSeconds)
{
    if (recordingPreview != rangeSeconds)
    {
        recordingPreview = rangeSeconds;
        repaint();
    }
}

void TrackLane::refresh()
{
    // Ansichten über ihre Kennung wiederverwenden: Ein verschobener Clip behält Wellenform und
    // Auswahl, nur neue Clips bekommen eine neue Ansicht, verschwundene werden entfernt.
    drag.reset();
    std::vector<std::unique_ptr<ClipView>> previous;
    previous.swap (clipViews);

    for (auto& clipInfo : tracks.getClips (track))
    {
        const auto existing = std::find_if (previous.begin(),
                                            previous.end(),
                                            [&clipInfo] (const std::unique_ptr<ClipView>& view)
                                            { return view != nullptr && view->getClipInfo().id == clipInfo.id; });

        if (existing != previous.end())
        {
            (*existing)->setClipInfo (std::move (clipInfo));
            clipViews.push_back (std::move (*existing));
            continue;
        }

        auto view = std::make_unique<ClipView> (waveforms, std::move (clipInfo), colour);
        addAndMakeVisible (*view);
        clipViews.push_back (std::move (view));
    }

    const auto info = tracks.getTrackInfo (track);
    bakedHint = info.bakedIntoGroup.isNotEmpty() ? plugins::i18n::format (TRANS ("Baked into group \"{group}\""), { { "group", info.bakedIntoGroup } })
                : info.baked ? TRANS ("Baked: unbake to edit")
                             : juce::String();

    layoutClips();
    repaint(); // Take-Lanes und Aufnahmevorschau liegen im Hintergrund der Spur
}

void TrackLane::paintOverChildren (juce::Graphics& g)
{
    if (bakedHint.isEmpty())
        return;

    // Gebackene Clips: abgedunkelt, damit sofort klar ist, dass hier nichts bearbeitbar ist.
    g.setColour (theme::backgroundBase.withAlpha (0.45f));
    g.fillRect (getLocalBounds().withHeight (clipAreaHeight()));
    g.setColour (theme::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (bakedHint,
                getLocalBounds().withHeight (clipAreaHeight()).reduced (theme::spacingMedium, theme::spacingSmall),
                juce::Justification::bottomLeft,
                true);
}

void TrackLane::setSelectedClips (const std::set<juce::String>& clipIds)
{
    for (auto& view : clipViews)
        view->setSelected (clipIds.contains (view->getClipInfo().id));
}

juce::String TrackLane::getClipIdAt (int x) const
{
    if (const auto* clip = findClipAt (x))
        return clip->getClipInfo().id;

    return {};
}

TrackLane::DragState::Mode TrackLane::modeAt (const ClipView& clip, juce::Point<int> local) const
{
    if (clip.getFadeInHandle().expanded (3).contains (local))
        return DragState::Mode::fadeIn;

    if (clip.getFadeOutHandle().expanded (3).contains (local))
        return DragState::Mode::fadeOut;

    // Trim-Zonen an beiden Rändern, bei schmalen Clips höchstens ein Drittel der Breite.
    const int zone = juce::jmin (edgeZone, clip.getWidth() / 3);

    if (local.x < zone)
        return DragState::Mode::trimLeft;

    if (local.x >= clip.getWidth() - zone)
        return DragState::Mode::trimRight;

    return DragState::Mode::move;
}

std::pair<double, double> TrackLane::trimmedRange (const DragState& state, double deltaSeconds) const
{
    const auto end = state.originalStartSeconds + state.originalLengthSeconds;

    if (state.mode == DragState::Mode::trimLeft)
    {
        // Nie vor den Anfang des Materials (Versatz 0) und nie hinter das Ende.
        const auto earliest = juce::jmax (0.0, state.originalStartSeconds - state.originalOffsetSeconds);
        const auto start = juce::jlimit (earliest, end - 0.05, snapped (state.originalStartSeconds + deltaSeconds));
        return { start, end };
    }

    const auto newEnd = juce::jmax (state.originalStartSeconds + 0.05, snapped (end + deltaSeconds));
    return { state.originalStartSeconds, newEnd };
}

void TrackLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundInput);

    const auto& mapping = settings.mapping;

    for (const auto& line : timeline.getGridLines (mapping.xToSeconds (0),
                                                   mapping.xToSeconds (getWidth()),
                                                   settings.gridResolution,
                                                   theme::minGridSpacingPixels / mapping.pixelsPerSecond))
    {
        g.setColour (line.isBarStart ? theme::gridBar : theme::gridBeat);
        g.drawVerticalLine (mapping.secondsToX (line.seconds), 0.0f, static_cast<float> (getHeight()));
    }

    // Laufende Aufnahme: roter Block vom Aufnahmestart bis zur aktuellen Position, damit man
    // sieht, dass etwas aufgenommen wird (der Clip selbst entsteht erst beim Stopp).
    if (recordingPreview.has_value())
    {
        const auto x1 = mapping.secondsToX (recordingPreview->first);
        const auto x2 = mapping.secondsToX (recordingPreview->second);
        g.setColour (theme::record.withAlpha (0.35f));
        g.fillRoundedRectangle (static_cast<float> (x1),
                                static_cast<float> (theme::spacingSmall),
                                static_cast<float> (juce::jmax (2, x2 - x1)),
                                static_cast<float> (getHeight() - 2 * theme::spacingSmall),
                                6.0f);
        g.setColour (theme::record);
        g.drawText (
            TRANS ("Recording..."), x1 + theme::spacingSmall, theme::spacingSmall, 120, 16, juce::Justification::centredLeft);
    }

    paintTakeLanes (g);

    if (drawPreview)
    {
        const auto x1 = mapping.secondsToX (juce::jmin (drawPreview->first, drawPreview->second));
        const auto x2 = mapping.secondsToX (juce::jmax (drawPreview->first, drawPreview->second));
        const auto area =
            juce::Rectangle<int> (
                x1, theme::spacingSmall, juce::jmax (4, x2 - x1), clipAreaHeight() - 2 * theme::spacingSmall)
                .toFloat();
        g.setColour (colour.withAlpha (0.45f));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (theme::textPrimary);
        g.drawRoundedRectangle (area, 6.0f, 1.0f);
    }

    g.setColour (theme::borderSubtle);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TrackLane::resized()
{
    layoutClips();
}

void TrackLane::paintTakeLanes (juce::Graphics& g)
{
    const int top = clipAreaHeight();
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (const auto& view : clipViews)
    {
        const auto& info = view->getClipInfo();

        if (! info.showingTakes || info.numTakes < 2)
            continue;

        const auto x1 = view->getX();
        const auto x2 = view->getRight();

        for (int take = 0; take < info.numTakes; ++take)
        {
            const int y = top + take * takeLaneHeight;
            const bool current = take == info.currentTake;
            g.setColour (current ? colour.withAlpha (0.35f) : theme::backgroundPanel);
            g.fillRect (x1, y, x2 - x1, takeLaneHeight - 1);

            // Laufende Bereichsauswahl in dieser Lane hervorheben.
            if (takeDrag && takeDrag->clipId == info.id && takeDrag->takeIndex == take)
            {
                const auto from = settings.mapping.secondsToX (juce::jmin (takeDrag->fromSeconds, takeDrag->toSeconds));
                const auto to = settings.mapping.secondsToX (juce::jmax (takeDrag->fromSeconds, takeDrag->toSeconds));
                g.setColour (theme::accent.withAlpha (0.6f));
                g.fillRect (from, y, juce::jmax (2, to - from), takeLaneHeight - 1);
            }

            g.setColour (current ? theme::textPrimary : theme::textSecondary);
            const auto name = take < static_cast<int> (info.takeNames.size())
                                  ? info.takeNames[static_cast<std::size_t> (take)]
                                  : plugins::i18n::format (TRANS ("Take {number}"), { { "number", juce::String (take + 1) } });
            g.drawText (name,
                        x1 + theme::spacingSmall,
                        y,
                        juce::jmax (20, x2 - x1 - theme::spacingSmall),
                        takeLaneHeight - 1,
                        juce::Justification::centredLeft);
        }
    }
}

void TrackLane::mouseDown (const juce::MouseEvent& event)
{
    if (onSelected)
        onSelected();

    // Klick in einer Take-Lane: Bereich für Comping aufziehen (links) oder Take wählen (Doppelklick).
    if (event.y >= clipAreaHeight() && ! event.mods.isPopupMenu())
    {
        for (const auto& view : clipViews)
        {
            const auto& info = view->getClipInfo();

            if (! info.showingTakes || info.numTakes < 2 || event.x < view->getX() || event.x >= view->getRight())
                continue;

            const int take = juce::jlimit (0, info.numTakes - 1, (event.y - clipAreaHeight()) / takeLaneHeight);
            const auto seconds = snapped (settings.mapping.xToSeconds (event.x));
            takeDrag = TakeDrag { info.id, take, seconds, seconds };
            repaint();
            return;
        }

        return;
    }

    // Bereichswerkzeug: überall aufziehen, auch über Clips; Rechtsklick öffnet das Bereichsmenü.
    if (settings.tool == Tool::range)
    {
        if (event.mods.isPopupMenu())
        {
            if (onRangeMenu)
                onRangeMenu (event.getScreenPosition());
        }
        else if (onMarqueeStart)
        {
            marqueeActive = true;
            onMarqueeStart (event.getScreenPosition(), event.mods);
        }

        return;
    }

    // Lupe wirkt überall, auch über Clips: Klick vergrößert, Alt+Klick verkleinert.
    if (settings.tool == Tool::zoom && ! event.mods.isPopupMenu())
    {
        if (onZoomRequested)
            onZoomRequested (settings.mapping.xToSeconds (event.x), ! event.mods.isAltDown());

        return;
    }

    auto* clip = findClipAt (event.x);

    if (clip == nullptr)
    {
        // Stift auf leerer Fläche: MIDI-Clip aufziehen (Klick ohne Ziehen = ein Takt).
        if (settings.tool == Tool::draw && ! event.mods.isPopupMenu())
        {
            if (! services::playsMidi (tracks.getTrackInfo (track).kind))
            {
                if (onError)
                    onError (TRANS ("The pencil creates MIDI clips and needs an instrument track for that. Create an \"Instrument\" track above."));
                return;
            }

            const auto start = snapped (juce::jmax (0.0, settings.mapping.xToSeconds (event.x)));
            drawPreview = std::pair (start, start);
            repaint();
        }
        else if (settings.tool == Tool::select && ! event.mods.isPopupMenu())
        {
            // Leere Fläche: Auswahl aufheben (außer mit Strg/Shift) und ein Lasso beginnen.
            if (onEmptyClicked && ! event.mods.isCtrlDown() && ! event.mods.isShiftDown())
                onEmptyClicked();

            if (onMarqueeStart)
            {
                marqueeActive = true;
                onMarqueeStart (event.getScreenPosition(), event.mods);
            }
        }

        return;
    }

    if (event.mods.isPopupMenu())
    {
        // Rechtsklick auf einen nicht ausgewählten Clip wählt ihn allein aus (wie Cubase).
        if (! clip->isSelected() && onClipClicked)
            onClipClicked (clip->getClipInfo().id, juce::ModifierKeys());

        showClipMenu (*clip, event.getScreenPosition());
        return;
    }

    switch (settings.tool)
    {
        case Tool::select:
        case Tool::draw:
        {
            const auto& info = clip->getClipInfo();
            const auto local = event.getEventRelativeTo (clip).getPosition();
            const auto mode = settings.tool == Tool::select ? modeAt (*clip, local) : DragState::Mode::move;

            // Auswahl vor dem Ziehen: Ein bereits ausgewählter Clip bleibt es, damit die Gruppe mitzieht.
            if (settings.tool == Tool::select && onClipClicked &&
                (! clip->isSelected() || event.mods.isCtrlDown() || event.mods.isShiftDown()))
                onClipClicked (info.id, event.mods);

            drag = DragState { clip,
                               mode,
                               info.startSeconds,
                               info.lengthSeconds,
                               info.offsetSeconds,
                               info.fadeInSeconds,
                               info.fadeOutSeconds,
                               false };
            // Alt an der rechten Kante eines Audio-Clips: Länge mit Time-Stretch ändern, Material bleibt ganz.
            drag->stretch = mode == DragState::Mode::trimRight && event.mods.isAltDown() && ! info.isMidi;
            break;
        }

        case Tool::split:
            if (const auto result =
                    clips.splitClip (clip->getClipInfo().id, snapped (settings.mapping.xToSeconds (event.x)));
                ! result)
                reportError (result.error());
            break;

        case Tool::erase:
            if (const auto result = clips.deleteClip (clip->getClipInfo().id); ! result)
                reportError (result.error());
            break;

        case Tool::mute:
        {
            const auto& info = clip->getClipInfo();

            if (const auto result = clips.setClipsMuted ({ info.id }, ! info.muted); ! result)
                reportError (result.error());

            break;
        }

        case Tool::glue:
        {
            // Wie Cubase: den geklickten Clip mit dem nächsten auf derselben Spur verbinden.
            const auto info = clip->getClipInfo();
            const services::ClipInfo* next = nullptr;
            const auto all = tracks.getClips (track);

            for (const auto& candidate : all)
                if (candidate.id != info.id && candidate.startSeconds >= info.startSeconds + 0.001 &&
                    (next == nullptr || candidate.startSeconds < next->startSeconds))
                    next = &candidate;

            if (next == nullptr)
            {
                if (onError)
                    onError (
                        TRANS ("There is no clip to the right of this one that it can be glued to."));
                break;
            }

            if (const auto result = clips.mergeClips ({ info.id, next->id }); ! result)
                reportError (result.error());

            break;
        }

        case Tool::zoom:
        case Tool::range:
            break;
    }
}

void TrackLane::mouseDrag (const juce::MouseEvent& event)
{
    if (marqueeActive)
    {
        if (onMarqueeDrag)
            onMarqueeDrag (event.getScreenPosition(), event.mods);

        return;
    }

    if (drawPreview)
    {
        drawPreview->second = snapped (juce::jmax (0.0, settings.mapping.xToSeconds (event.x)));
        repaint();
        return;
    }

    if (takeDrag)
    {
        takeDrag->toSeconds = snapped (juce::jmax (0.0, settings.mapping.xToSeconds (event.x)));
        repaint();
        return;
    }

    if (! drag.has_value())
        return;

    // Nur die Ansicht verschieben; das Modell ändert sich erst beim Loslassen (ein Undo-Schritt).
    // Ein Umbau der Clip-Ansichten während des Ziehens ließe drag->clip ins Leere zeigen.
    const auto deltaSeconds = event.getDistanceFromDragStartX() / settings.mapping.pixelsPerSecond;
    drag->moved = drag->moved || std::abs (event.getDistanceFromDragStartX()) > 2 ||
                  std::abs (event.getDistanceFromDragStartY()) > 2;

    if (drag->mode == DragState::Mode::trimLeft || drag->mode == DragState::Mode::trimRight)
    {
        const auto [start, end] = trimmedRange (*drag, deltaSeconds);
        const auto offset = drag->originalOffsetSeconds + (start - drag->originalStartSeconds);
        drag->clip->setPreviewPosition (start, end - start, offset);
        const auto x = settings.mapping.secondsToX (start);
        const auto width = juce::jmax (2, settings.mapping.secondsToX (end) - x);
        drag->clip->setBounds (x, theme::spacingSmall, width, clipAreaHeight() - 2 * theme::spacingSmall);
        return;
    }

    if (drag->mode != DragState::Mode::move)
    {
        const auto length = drag->clip->getClipInfo().lengthSeconds;

        if (drag->mode == DragState::Mode::fadeIn)
            drag->fadeInSeconds = juce::jlimit (0.0, length - drag->fadeOutSeconds, drag->fadeInSeconds + deltaSeconds);
        else
            drag->fadeOutSeconds =
                juce::jlimit (0.0, length - drag->fadeInSeconds, drag->fadeOutSeconds - deltaSeconds);

        drag->clip->setPreviewFades (drag->fadeInSeconds, drag->fadeOutSeconds);
        return;
    }
    const auto newStart = snapped (juce::jmax (0.0, drag->originalStartSeconds + deltaSeconds));
    const auto& info = drag->clip->getClipInfo();
    const auto x = settings.mapping.secondsToX (newStart);
    const auto width = juce::jmax (2, settings.mapping.secondsToX (newStart + info.lengthSeconds) - x);
    drag->clip->setBounds (x, theme::spacingSmall, width, clipAreaHeight() - 2 * theme::spacingSmall);

    if (onClipDragPreview)
        onClipDragPreview (newStart, event.getScreenY());
}

void TrackLane::mouseMove (const juce::MouseEvent& event)
{
    // Zeiger zeigt, was ein Klick täte: Trimmen an den Rändern, sonst Standard.
    auto shape = juce::MouseCursor::NormalCursor;
    juce::String tip;

    if (settings.tool == Tool::select && event.y < clipAreaHeight())
        if (auto* clip = findClipAt (event.x))
        {
            const auto mode = modeAt (*clip, event.getEventRelativeTo (clip).getPosition());
            tip =
                TRANS ("Drag: move. Right-click: menu (tempo and pitch, normalize, ...)");

            if (mode == DragState::Mode::trimLeft || mode == DragState::Mode::trimRight)
            {
                shape = juce::MouseCursor::LeftRightResizeCursor;
                tip = TRANS ("Drag: trim. Alt+drag on the right edge: stretch (time stretch)");
            }
            else if (mode == DragState::Mode::move)
                shape = juce::MouseCursor::DraggingHandCursor;
        }

    setMouseCursor (shape);

    if (getTooltip() != tip)
        setTooltip (tip);
}

void TrackLane::mouseUp (const juce::MouseEvent& event)
{
    if (marqueeActive)
    {
        marqueeActive = false;

        if (onMarqueeEnd)
            onMarqueeEnd (event.getScreenPosition(), event.mods); // kann diese Spur neu aufbauen: zuletzt

        return;
    }

    if (drawPreview)
    {
        const auto [from, to] = *drawPreview;
        drawPreview.reset();
        repaint();
        createMidiClip (juce::jmin (from, to), juce::jmax (from, to));
        return;
    }

    if (takeDrag)
    {
        const auto finished = *takeDrag;
        takeDrag.reset();
        repaint();

        const auto from = juce::jmin (finished.fromSeconds, finished.toSeconds);
        const auto to = juce::jmax (finished.fromSeconds, finished.toSeconds);

        // Kurzer Klick ohne Bereich: ganzen Clip auf den Take; Bereich: nur diesen Teil (schneidet).
        const auto result = to - from < 0.01 ? clips.setCurrentTake (finished.clipId, finished.takeIndex)
                                             : clips.useTakeForRange (finished.clipId, finished.takeIndex, from, to)
                                                   .transform ([] (const juce::String&) {});

        if (! result)
            reportError (result.error());

        return;
    }

    if (! drag.has_value())
        return;

    const auto deltaSeconds = event.getDistanceFromDragStartX() / settings.mapping.pixelsPerSecond;
    const auto newStart = snapped (juce::jmax (0.0, drag->originalStartSeconds + deltaSeconds));
    const auto clipId = drag->clip->getClipInfo().id;
    const auto state = *drag;
    drag.reset();

    if (! state.moved)
    {
        layoutClips(); // Klick ohne Ziehen: Vorschau zurücknehmen, nichts ändern
        return;
    }

    if (state.mode == DragState::Mode::trimLeft || state.mode == DragState::Mode::trimRight)
    {
        const auto [start, end] = trimmedRange (state, deltaSeconds);

        if (const auto result =
                state.stretch ? clips.stretchClip (clipId, end - start) : clips.trimClip (clipId, start, end);
            ! result)
            reportError (result.error());

        refresh();
        return;
    }

    if (state.mode != DragState::Mode::move)
    {
        clips.beginChange (TRANS ("Drag fade"));

        if (const auto result = clips.setFades (clipId, state.fadeInSeconds, state.fadeOutSeconds); ! result)
            reportError (result.error());

        return;
    }

    if (onClipMoveRequested)
    {
        onClipMoveRequested (clipId, newStart - state.originalStartSeconds, event.getScreenY());
        layoutClips(); // falls der Zug abgelehnt wurde, zurück an die alte Stelle
        return;
    }

    if (const auto result = clips.moveClip (clipId, newStart); ! result)
        reportError (result.error());
}

void TrackLane::showClipMenu (ClipView& clip, juce::Point<int> screenPosition)
{
    const auto info = clip.getClipInfo();
    juce::PopupMenu menu;

    // Bearbeitung der Auswahl, mit Symbolen wie im Menü Bearbeiten und in der Werkzeugleiste.
    const auto addAction = [&menu] (int id, Icon icon, const juce::String& text, bool enabled = true)
    {
        juce::PopupMenu::Item item (text);
        item.itemID = id;
        item.isEnabled = enabled;
        item.image = makeIconDrawable (icon, theme::textPrimary);
        menu.addItem (std::move (item));
    };
    addAction (10, Icon::cut, TRANS ("Cut\tCtrl+X"));
    addAction (11, Icon::copy, TRANS ("Copy\tCtrl+C"));
    addAction (
        12, Icon::paste, TRANS ("Paste at cursor\tCtrl+V"), clips.hasClipboardContent());
    addAction (13, Icon::duplicate, TRANS ("Duplicate\tCtrl+D"));
    addAction (14, Icon::glue, TRANS ("Glue (selection into one clip)\tCtrl+J"));
    addAction (17, Icon::normalize, TRANS ("Normalize (peak 0 dBFS)"), ! info.isMidi);
    addAction (18,
               Icon::reverse,
               info.reversed ? TRANS ("Undo reverse") : TRANS ("Reverse"),
               ! info.isMidi);
    addAction (19, Icon::phase, TRANS ("Invert phase"), ! info.isMidi);
    addAction (16,
               Icon::toolMute,
               info.muted ? TRANS ("Unmute\tShift+M")
                          : TRANS ("Mute\tShift+M"));
    menu.addSeparator();
    menu.addItem (1, TRANS ("Clip Gain..."), ! info.isMidi);
    {
        juce::PopupMenu::Item item (TRANS ("Tempo and Pitch..."));
        item.itemID = 3;
        item.isEnabled = ! info.isMidi;
        item.image = makeIconDrawable (Icon::stretch, theme::textPrimary);
        menu.addItem (std::move (item));
    }
    menu.addItem (2,
                  TRANS ("Reset fades"),
                  ! info.isMidi && (info.fadeInSeconds > 0.0 || info.fadeOutSeconds > 0.0));
    menu.addSeparator();

    // Takes: wählen, Lanes aufklappen (Comping), verteilen, aufräumen, aus Datei ergänzen.
    const auto takes = clips.getTakes (info.id);
    juce::PopupMenu takeMenu;

    for (const auto& take : takes)
        takeMenu.addItem (100 + take.index, take.name, true, take.isCurrent);

    if (! takes.empty())
        takeMenu.addSeparator();

    takeMenu.addItem (4, TRANS ("Show take lanes (drag a range = comping)"), takes.size() > 1, info.showingTakes);
    takeMenu.addItem (5, TRANS ("Distribute takes to the tracks below"), takes.size() > 1);
    takeMenu.addItem (6, TRANS ("Delete unused takes"), takes.size() > 1 && ! info.isMidi);
    takeMenu.addItem (7, TRANS ("Add take from file..."), ! info.isMidi);
    menu.addSubMenu (TRANS ("Takes"), takeMenu);
    menu.addSeparator();
    addAction (15, Icon::trash, TRANS ("Delete selection\tDel"));

    const auto screenArea = juce::Rectangle<int> (screenPosition.x, screenPosition.y, 1, 1);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetScreenArea (screenArea),
        [this, clipId = info.id, screenArea] (int choice)
        {
            // Die Clip-Ansicht kann inzwischen neu gebaut sein: über die ID weiterarbeiten.
            if (choice == 1)
            {
                for (auto& view : clipViews)
                    if (view->getClipInfo().id == clipId)
                        showClipGainEditor (*view, screenArea);
            }
            else if (choice == 3)
            {
                showStretchEditor (clipId, screenArea);
            }
            else if (choice == 2)
            {
                clips.beginChange (TRANS ("Reset fades"));
                if (const auto result = clips.setFades (clipId, 0.0, 0.0); ! result)
                    reportError (result.error());
            }
            else if (choice >= 10 && choice <= 19)
            {
                static constexpr ClipAction actions[] = { ClipAction::cut,        ClipAction::copy,
                                                          ClipAction::paste,      ClipAction::duplicate,
                                                          ClipAction::merge,      ClipAction::remove,
                                                          ClipAction::toggleMute, ClipAction::normalize,
                                                          ClipAction::reverse,    ClipAction::invertPhase };
                if (onClipAction)
                    onClipAction (actions[choice - 10]);
            }
            else if (choice >= 100)
            {
                if (const auto result = clips.setCurrentTake (clipId, choice - 100); ! result)
                    reportError (result.error());
            }
            else if (choice == 4)
            {
                for (auto& view : clipViews)
                    if (view->getClipInfo().id == clipId)
                        if (const auto result = clips.setShowingTakes (clipId, ! view->getClipInfo().showingTakes);
                            ! result)
                            reportError (result.error());
            }
            else if (choice == 5)
            {
                if (const auto result = clips.unpackTakes (clipId); ! result)
                    reportError (result.error());
            }
            else if (choice == 6)
            {
                if (const auto result = clips.deleteUnusedTakes (clipId); ! result)
                    reportError (result.error());
            }
            else if (choice == 7)
            {
                addTakeFromFile (clipId);
            }
        });
}

void TrackLane::addTakeFromFile (const juce::String& clipId)
{
    // Dialog asynchron; der Chooser lebt in einem shared_ptr, bis der Callback gelaufen ist.
    auto chooser = std::make_shared<juce::FileChooser> (TRANS ("Add audio file as take"),
                                                        juce::File(),
                                                        "*.wav;*.flac;*.aif;*.aiff;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, clipId, chooser] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              if (const auto result = clips.addTakeFromFile (clipId, file); ! result)
                                  reportError (result.error());
                          });
}

void TrackLane::showClipGainEditor (ClipView& clip, juce::Rectangle<int> screenArea)
{
    // Kleines Sprechblasenfenster mit einem Regler; Änderungen gehen sofort ins Modell, ein
    // Undo-Schritt pro Öffnen. JUCE übernimmt den Besitz des Inhalts (Framework-Vertrag).
    auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    slider->setRange (-24.0, 24.0, 0.1);
    slider->setTextValueSuffix (" dB");
    slider->setDoubleClickReturnValue (true, 0.0);
    slider->setValue (clip.getClipInfo().gainDb, juce::dontSendNotification);
    slider->setTooltip (TRANS ("Clip gain, double-click: 0 dB"));
    slider->setSize (260, 28);

    clips.beginChange (TRANS ("Clip gain"));
    slider->onValueChange = [this, clipId = clip.getClipInfo().id, raw = slider.get()]
    {
        if (const auto result = clips.setGainDb (clipId, static_cast<float> (raw->getValue())); ! result)
            reportError (result.error());
    };

    juce::CallOutBox::launchAsynchronously (std::move (slider), screenArea, nullptr);
}

void TrackLane::showStretchEditor (const juce::String& clipId, juce::Rectangle<int> screenArea)
{
    // Wie beim Clip-Gain eine Sprechblase; JUCE übernimmt den Besitz des Inhalts.
    auto panel = std::make_unique<ClipStretchPanel> (clips, tracks, track, clipId);
    panel->onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), screenArea, nullptr);
}

void TrackLane::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (auto* clip = findClipAt (event.x))
    {
        if (clip->getClipInfo().isMidi && onOpenMidiClip)
            onOpenMidiClip (clip->getClipInfo().id);
        else if (! clip->getClipInfo().isMidi && onOpenAudioClip)
            onOpenAudioClip (clip->getClipInfo().id);

        return;
    }

    createMidiClipAt (event.x);
}

void TrackLane::createMidiClipAt (int x)
{
    if (! services::playsMidi (tracks.getTrackInfo (track).kind))
        return;

    const auto start = snapped (juce::jmax (0.0, settings.mapping.xToSeconds (x)));
    createMidiClip (start, start);
}

void TrackLane::createMidiClip (double startSeconds, double endSeconds)
{
    // Reiner Klick (oder auf den Anfang zurückgezogen): ein Takt, wie Cubase.
    constexpr double clickSeconds = 0.01;
    const auto length =
        endSeconds - startSeconds > clickSeconds ? endSeconds - startSeconds : timeline.barLengthSeconds (startSeconds);

    if (const auto result = midiClips.createEmptyClip (track, startSeconds, length); ! result)
    {
        reportError (result.error());

        if (onError)
            onError (services::errorText (result.error()));
    }
}

void TrackLane::layoutClips()
{
    for (auto& view : clipViews)
    {
        const auto& info = view->getClipInfo();
        const auto x = settings.mapping.secondsToX (info.startSeconds);
        const auto width = juce::jmax (2, settings.mapping.secondsToX (info.startSeconds + info.lengthSeconds) - x);
        view->setBounds (x, theme::spacingSmall, width, clipAreaHeight() - 2 * theme::spacingSmall);
    }
}

int TrackLane::clipAreaHeight() const
{
    return getHeight() - getExtraHeight();
}

int TrackLane::getExtraHeight() const
{
    int extra = 0;

    for (const auto& view : clipViews)
        if (const auto& info = view->getClipInfo(); info.showingTakes && info.numTakes > 1)
            extra = juce::jmax (extra, info.numTakes * takeLaneHeight + theme::spacingSmall);

    return extra;
}

ClipView* TrackLane::findClipAt (int x) const
{
    // Von hinten suchen: Der zuletzt gezeichnete Clip liegt oben.
    for (auto it = clipViews.rbegin(); it != clipViews.rend(); ++it)
        if ((*it)->getBounds().getX() <= x && x < (*it)->getBounds().getRight())
            return it->get();

    return nullptr;
}

double TrackLane::snapped (double seconds) const
{
    return settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;
}

void TrackLane::reportError (const core::Error& error) const
{
    juce::Logger::writeToLog ("Clip-Aktion fehlgeschlagen: " + services::errorText (error));
}

} // namespace fk::ui
