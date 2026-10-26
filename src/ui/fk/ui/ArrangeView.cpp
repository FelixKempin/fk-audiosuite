#include "fk/ui/ArrangeView.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <algorithm>
#include <limits>

namespace fk::ui
{

namespace
{
constexpr int playheadRefreshHz = 30;
constexpr double minPixelsPerSecond = 2.0;
constexpr double maxPixelsPerSecond = 2000.0;
constexpr double defaultPixelsPerSecond = 20.0;
constexpr double minTimelineSeconds = 60.0;
constexpr double timelinePaddingSeconds = 30.0;
constexpr int scrollBarHeight = 12;

/// Passt eine Datei der Art wanted auf diese Spur? MIDI-Dateien passen auf Instrument- und MIDI-Spuren.
bool fitsKind (services::TrackKind trackKind, services::TrackKind wanted)
{
    return wanted == services::TrackKind::instrument ? services::playsMidi (trackKind) : trackKind == wanted;
}
} // namespace

ArrangeView::ArrangeView (Services services)
    : tracks (services.tracks)
    , clips (services.clips)
    , midiClips (services.midiClips)
    , transport (services.transport)
    , timeline (services.timeline)
    , waveforms (services.waveforms)
    , mixer (services.mixer)
    , automation (services.automation)
    , signature (services.signature)
    , ruler (services.transport, services.timeline)
    , signatureTrack (services.signature, services.timeline)
    , markerTrack (services.markers, services.transport, services.timeline)
{
    settings.mapping.pixelsPerSecond = defaultPixelsPerSecond;

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);

    horizontalScrollBar.setAutoHide (false);
    horizontalScrollBar.addListener (this);

    addChildComponent (overview);
    overview.onRangeChanged = [this] (double from, double to) { showTimeRange (from, to); };
    addAndMakeVisible (ruler);
    addAndMakeVisible (signatureTrack);
    addAndMakeVisible (markerTrack);
    markerTrack.onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };
    addAndMakeVisible (viewport);
    addAndMakeVisible (horizontalScrollBar);

    tracks.addListener (*this);
    timeline.addListener (*this);
    automation.addListener (*this);
    rebuildRows();
    startTimerHz (playheadRefreshHz);
}

ArrangeView::~ArrangeView()
{
    horizontalScrollBar.removeListener (this);
    automation.removeListener (*this);
    timeline.removeListener (*this);
    tracks.removeListener (*this);
}

TimeMapping ArrangeView::getTimeMapping() const noexcept
{
    return settings.mapping;
}

void ArrangeView::setTool (Tool tool)
{
    settings.tool = tool;

    if (tool != Tool::range)
        rangeSelection.reset();

    applySettings();
}

const std::optional<ArrangeView::RangeSelection>& ArrangeView::getRangeSelection() const noexcept
{
    return rangeSelection;
}

double ArrangeView::secondsAtLocalX (int x, bool snap) const
{
    const auto seconds = juce::jmax (0.0, settings.mapping.xToSeconds (x - theme::trackHeaderWidth));
    return snap && settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;
}

std::vector<services::TrackHandle*> ArrangeView::tracksBetween (int localY1, int localY2) const
{
    const auto top = std::min (localY1, localY2);
    const auto bottom = std::max (localY1, localY2);
    std::vector<services::TrackHandle*> covered;

    for (const auto& row : rows)
    {
        const auto lane = getLocalArea (&rowsContainer, row.lane->getBounds());

        if (lane.getBottom() > top && lane.getY() <= bottom)
            covered.push_back (row.track);
    }

    return covered;
}

void ArrangeView::marqueeStarted (juce::Point<int> screenPosition)
{
    const auto local = getLocalPoint (nullptr, screenPosition);
    marquee = Marquee { local, local, settings.tool == Tool::range };
    repaint();
}

void ArrangeView::marqueeMoved (juce::Point<int> screenPosition, juce::ModifierKeys mods)
{
    if (! marquee)
        return;

    marquee->current = getLocalPoint (nullptr, screenPosition);

    // Bereich: schon beim Ziehen als Auswahl zeigen, am Raster (Shift: frei).
    if (marquee->isRange)
    {
        const auto a = secondsAtLocalX (marquee->start.x, ! mods.isShiftDown());
        const auto b = secondsAtLocalX (marquee->current.x, ! mods.isShiftDown());
        rangeSelection =
            RangeSelection { std::min (a, b), std::max (a, b), tracksBetween (marquee->start.y, marquee->current.y) };
    }

    repaint();
}

void ArrangeView::marqueeEnded (juce::Point<int> screenPosition, juce::ModifierKeys mods)
{
    if (! marquee)
        return;

    marqueeMoved (screenPosition, mods);
    const auto finished = *marquee;
    marquee.reset();
    const bool moved = finished.start.getDistanceFrom (finished.current) > 3;

    if (finished.isRange)
    {
        // Klick ohne Ziehen hebt die Bereichsauswahl auf, wie in Cubase.
        if (! moved || (rangeSelection && (rangeSelection->tracks.empty() ||
                                           rangeSelection->toSeconds - rangeSelection->fromSeconds < 0.0001)))
            rangeSelection.reset();

        repaint();
        return;
    }

    if (moved)
    {
        // Lasso: alle Clips, die das Rechteck berühren; Strg/Shift nimmt sie zur Auswahl dazu.
        const auto from = secondsAtLocalX (std::min (finished.start.x, finished.current.x), false);
        const auto to = secondsAtLocalX (std::max (finished.start.x, finished.current.x), false);
        const auto found = clips.getClipsInRange (tracksBetween (finished.start.y, finished.current.y), from, to);

        if (! mods.isCtrlDown() && ! mods.isShiftDown())
            selectedClips.clear();

        selectedClips.insert (found.begin(), found.end());
        pushClipSelectionToRows();

        if (onClipSelectionChanged)
            onClipSelectionChanged();
    }

    repaint();
}

void ArrangeView::performRangeAction (RangeAction action)
{
    if (! rangeSelection)
        return;

    const auto range = *rangeSelection;
    core::Result<void> result;

    switch (action)
    {
        case RangeAction::remove:
            result = clips.deleteRange (range.tracks, range.fromSeconds, range.toSeconds, false);
            break;
        case RangeAction::removeAndClose:
            result = clips.deleteRange (range.tracks, range.fromSeconds, range.toSeconds, true);
            break;
        case RangeAction::insertSilence:
            result = clips.insertSilence (range.tracks, range.fromSeconds, range.toSeconds);
            break;
        case RangeAction::split:
            result = clips.splitAtRange (range.tracks, range.fromSeconds, range.toSeconds);
            break;
        case RangeAction::loop:
            timeline.setLoopRange ({ range.fromSeconds, range.toSeconds });
            timeline.setLooping (true);
            break;
        case RangeAction::clear:
            rangeSelection.reset();
            break;
    }

    if (! result)
        reportError (result.error());

    repaint();
}

void ArrangeView::showRangeMenu (juce::Point<int> screenPosition)
{
    const bool hasRange = rangeSelection.has_value();
    juce::PopupMenu menu;
    menu.addSectionHeader (
        hasRange ? plugins::i18n::format (TRANS ("Range: {from} to {to}"),
                                          { { "from", timeline.formatBarsBeats (rangeSelection->fromSeconds) },
                                            { "to", timeline.formatBarsBeats (rangeSelection->toSeconds) } })
                 : TRANS ("No range: drag one with the range tool"));
    menu.addItem (1, TRANS ("Delete (leave gap)\tDel"), hasRange);
    menu.addItem (2, TRANS ("Remove (close gap)\tShift+Del"), hasRange);
    menu.addItem (3, TRANS ("Insert silence"), hasRange);
    menu.addItem (4, TRANS ("Split at the boundaries"), hasRange);
    menu.addItem (5, TRANS ("Loop to the range"), hasRange);
    menu.addSeparator();
    menu.addItem (6, TRANS ("Clear selection"), hasRange);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [safeThis = juce::Component::SafePointer (this)] (int chosen)
                        {
                            static constexpr RangeAction actions[] = {
                                RangeAction::remove, RangeAction::removeAndClose, RangeAction::insertSilence,
                                RangeAction::split,  RangeAction::loop,           RangeAction::clear
                            };

                            if (safeThis != nullptr && chosen >= 1 && chosen <= 6)
                                safeThis->performRangeAction (actions[chosen - 1]);
                        });
}

void ArrangeView::setGridResolution (services::GridResolution resolution)
{
    settings.gridResolution = resolution;
    applySettings();
}

void ArrangeView::setSnapEnabled (bool shouldSnap)
{
    settings.snapEnabled = shouldSnap;
    applySettings();
}

void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);
}

void ArrangeView::paintOverChildren (juce::Graphics& g)
{
    // Positionszeiger über allen Spuren, nur im Zeitachsen-Bereich rechts der Spurköpfe.
    const auto x = theme::trackHeaderWidth + settings.mapping.secondsToX (transport.getPositionSeconds());

    if (x >= theme::trackHeaderWidth && x < getWidth())
    {
        g.setColour (theme::accent);
        g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight() - scrollBarHeight));
    }

    // Bereichsauswahl: halbtransparent über den betroffenen Spuren.
    if (rangeSelection)
    {
        const auto x1 = theme::trackHeaderWidth + settings.mapping.secondsToX (rangeSelection->fromSeconds);
        const auto x2 = theme::trackHeaderWidth + settings.mapping.secondsToX (rangeSelection->toSeconds);

        for (const auto& row : rows)
            if (std::ranges::find (rangeSelection->tracks, row.track) != rangeSelection->tracks.end())
            {
                const auto lane =
                    getLocalArea (&rowsContainer, row.lane->getBounds()).getIntersection (viewport.getBounds());
                const auto area = juce::Rectangle<int> (x1, lane.getY(), juce::jmax (1, x2 - x1), lane.getHeight())
                                      .getIntersection (lane);
                g.setColour (theme::accent.withAlpha (0.22f));
                g.fillRect (area);
                g.setColour (theme::accent);
                g.drawRect (area, 1);
            }
    }

    // Lasso: Rechteck beim Aufziehen.
    if (marquee && ! marquee->isRange)
    {
        const auto area = juce::Rectangle<int> (marquee->start, marquee->current);
        g.setColour (theme::accent.withAlpha (0.12f));
        g.fillRect (area);
        g.setColour (theme::accent);
        g.drawRect (area, 1);
    }

    // Spur oder Ordner ziehen: Einfügelinie zwischen den Köpfen, oder der Zielordner leuchtet.
    if (reorderTarget)
    {
        g.setColour (theme::accent);

        if (reorderTarget->into != nullptr)
            g.drawRect (reorderTarget->folderArea, 2);
        else
            g.fillRect (0, reorderTarget->lineY - 1, getWidth(), 3);
    }

    // Abwurf-Vorschau: Zielspur hervorheben (oder "neue Spur" unten), Einfügezeit als Linie.
    if (dropPreview)
    {
        const auto dropX = theme::trackHeaderWidth + settings.mapping.secondsToX (dropPreview->seconds);
        juce::Rectangle<int> rowArea;

        if (dropPreview->rowIndex >= 0 && dropPreview->rowIndex < static_cast<int> (rows.size()))
            rowArea =
                getLocalArea (&rowsContainer, rows[static_cast<std::size_t> (dropPreview->rowIndex)].lane->getBounds());
        else
            rowArea =
                viewport.getBounds().removeFromBottom (theme::trackHeight).withTrimmedLeft (theme::trackHeaderWidth);

        g.setColour (theme::meterLow.withAlpha (0.18f));
        g.fillRect (rowArea.getIntersection (viewport.getBounds()));
        g.setColour (theme::meterLow);
        g.drawVerticalLine (dropX, static_cast<float> (viewport.getY()), static_cast<float> (viewport.getBottom()));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (dropPreview->label.isNotEmpty() ? dropPreview->label
                    : dropPreview->rowIndex >= 0    ? TRANS ("Insert here")
                                                    : TRANS ("New audio track"),
                    dropX + theme::spacingSmall,
                    rowArea.getY() + theme::spacingSmall,
                    140,
                    16,
                    juce::Justification::centredLeft);
    }
}

bool ArrangeView::isAudioFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

bool ArrangeView::isMidiFile (const juce::File& file)
{
    return file.hasFileExtension ("mid;midi");
}

bool ArrangeView::isImportableFile (const juce::File& file)
{
    return isAudioFile (file) || isMidiFile (file);
}

bool ArrangeView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isImportableFile (juce::File (path)))
            return true;

    return false;
}

ArrangeView::DropPreview ArrangeView::previewFor (int x, int y) const
{
    return previewFor (x, y, services::TrackKind::audio);
}

ArrangeView::DropPreview ArrangeView::previewFor (int x, int y, services::TrackKind wantedKind) const
{
    DropPreview preview;
    const auto laneX = juce::jmax (0, x - theme::trackHeaderWidth);
    const auto seconds = juce::jmax (0.0, settings.mapping.xToSeconds (laneX));
    preview.seconds = settings.snapEnabled ? timeline.snapSeconds (seconds, settings.gridResolution) : seconds;

    // Zeile unter dem Zeiger: Koordinaten in den gerollten Zeilencontainer übersetzen.
    const auto inRows = rowsContainer.getLocalPoint (this, juce::Point<int> (x, y));
    const auto currentTracks = displayedTracks();

    for (std::size_t i = 0; i < rows.size() && i < currentTracks.size(); ++i)
        if (rows[i].header->getBounds().getY() <= inRows.y &&
            inRows.y < rows[i].header->getBottom() +
                           (rows[i].automationShown
                                ? static_cast<int> (rows[i].automationLanes.size()) * theme::automationLaneHeight
                                : 0) &&
            fitsKind (tracks.getTrackInfo (*currentTracks[i]).kind, wantedKind))
            preview.rowIndex = static_cast<int> (i);

    return preview;
}

void ArrangeView::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    fileDragMove (files, x, y);
}

void ArrangeView::fileDragMove (const juce::StringArray& files, int x, int y)
{
    // Vorschau für die erste Datei: MIDI sucht eine Instrumentenspur, Audio eine Audiospur.
    const bool midi = ! files.isEmpty() && isMidiFile (juce::File (files[0]));
    dropPreview = previewFor (x, y, midi ? services::TrackKind::instrument : services::TrackKind::audio);

    if (dropPreview->rowIndex < 0)
        dropPreview->label = midi ? TRANS ("New instrument track") : TRANS ("New audio track");

    repaint();
}

void ArrangeView::fileDragExit (const juce::StringArray&)
{
    dropPreview.reset();
    repaint();
}

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropPreview.reset();
    importFiles (files, x, y);
}

namespace
{
constexpr auto mediaDragPrefix = "fk-media:";

juce::StringArray filesFromDragDescription (const juce::var& description)
{
    const auto text = description.toString();

    if (! text.startsWith (mediaDragPrefix))
        return {};

    juce::StringArray files;
    files.addLines (text.fromFirstOccurrenceOf (mediaDragPrefix, false, false));
    files.removeEmptyStrings();
    return files;
}
} // namespace

bool ArrangeView::isInterestedInDragSource (const SourceDetails& details)
{
    return isInterestedInFileDrag (filesFromDragDescription (details.description));
}

void ArrangeView::itemDragEnter (const SourceDetails& details)
{
    dropPreview = previewFor (details.localPosition.x, details.localPosition.y);
    repaint();
}

void ArrangeView::itemDragMove (const SourceDetails& details)
{
    dropPreview = previewFor (details.localPosition.x, details.localPosition.y);
    repaint();
}

void ArrangeView::itemDragExit (const SourceDetails&)
{
    dropPreview.reset();
    repaint();
}

void ArrangeView::itemDropped (const SourceDetails& details)
{
    dropPreview.reset();
    importFiles (filesFromDragDescription (details.description), details.localPosition.x, details.localPosition.y);
}

void ArrangeView::importFilesAtPlayhead (const juce::StringArray& files)
{
    const auto currentTracks = displayedTracks();
    int y = viewport.getBottom() + 1; // außerhalb der Zeilen = neue Spur

    for (std::size_t i = 0; i < rows.size() && i < currentTracks.size(); ++i)
        if (currentTracks[i] == selectedTrack)
            y = getLocalPoint (&rowsContainer, rows[i].header->getBounds().getCentre()).y;

    importFiles (files, theme::trackHeaderWidth + settings.mapping.secondsToX (transport.getPositionSeconds()), y);
}

void ArrangeView::importFiles (const juce::StringArray& files, int x, int y, MidiTempo midiTempo)
{
    // Jede Audiodatei ein Eintrag, jede Spur einer MIDI-Datei ein Eintrag (Index in der Datei).
    struct Item
    {
        juce::File file;
        int midiTrack = -1; // -1 = Audiodatei
    };
    std::vector<Item> items;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (isAudioFile (file))
            items.push_back ({ file, -1 });
        else if (isMidiFile (file))
            for (int t = 0; t < juce::jmax (1, services::ClipService::countMidiTracks (file)); ++t)
                items.push_back ({ file, t });
    }

    if (items.empty())
        return;

    const auto wantedKind = items.front().midiTrack >= 0 ? services::TrackKind::instrument : services::TrackKind::audio;
    const auto preview = previewFor (x, y, wantedKind);

    // Eigenes Tempo der (ersten) MIDI-Datei: erst fragen, dann das Tempo vor den Noten setzen,
    // damit Clip-Länge und Notenpositionen schon zum neuen Tempo passen.
    const auto midiItem = std::ranges::find_if (items, [] (const Item& item) { return item.midiTrack >= 0; });

    if (midiItem != items.end() && midiTempo != MidiTempo::keep &&
        signature.midiFileTempoDiffers (midiItem->file, preview.seconds))
    {
        if (midiTempo == MidiTempo::ask)
        {
            if (onAskMidiTempo)
            {
                const auto changes = services::SignatureService::readMidiFileTempo (midiItem->file);
                onAskMidiTempo (midiItem->file.getFileName(),
                                changes.front().quarterBpm,
                                [safeThis = juce::Component::SafePointer<ArrangeView> (this), files, x, y] (bool adopt)
                                {
                                    if (safeThis != nullptr)
                                        safeThis->importFiles (files, x, y, adopt ? MidiTempo::adopt : MidiTempo::keep);
                                });
                return;
            }
        }
        else if (const auto applied = signature.applyMidiFileTempo (midiItem->file, preview.seconds); ! applied)
        {
            reportError (applied.error());
        }
    }
    auto currentTracks = displayedTracks();
    int rowIndex = preview.rowIndex;
    bool first = true;

    for (const auto& item : items)
    {
        const auto kind = item.midiTrack >= 0 ? services::TrackKind::instrument : services::TrackKind::audio;

        // Erster Eintrag auf die Zielspur; weitere auf die Spuren darunter, neue bei Bedarf.
        services::TrackHandle* target = nullptr;

        if (rowIndex >= 0 && rowIndex < static_cast<int> (currentTracks.size()) &&
            fitsKind (tracks.getTrackInfo (*currentTracks[static_cast<std::size_t> (rowIndex)]).kind, kind) &&
            (first || rowIndex > preview.rowIndex))
            target = currentTracks[static_cast<std::size_t> (rowIndex)];

        if (target == nullptr)
        {
            const auto name = item.file.getFileNameWithoutExtension() +
                              (item.midiTrack > 0 ? " " + juce::String (item.midiTrack + 1) : juce::String());

            if (kind == services::TrackKind::instrument)
            {
                target = createInstrumentTrack ? createInstrumentTrack (name) : nullptr;

                if (target == nullptr)
                {
                    reportError (core::Error { core::ErrorCode::unsupported,
                                               TRANS ("MIDI file: no instrument track it fits on.").toStdString() });
                    return;
                }
            }
            else
            {
                const auto created = tracks.createAudioTrack (name, 2);

                if (! created)
                {
                    reportError (created.error());
                    return;
                }

                target = *created;
            }

            currentTracks = displayedTracks();
            rowIndex = static_cast<int> (currentTracks.size()) - 1;
        }

        const auto result = item.midiTrack >= 0
                                ? clips.importMidiFile (*target, item.file, preview.seconds, item.midiTrack)
                                : clips.importAudioFile (*target, item.file, preview.seconds);

        if (! result)
            reportError (result.error());

        selectTrack (target);
        ++rowIndex;
        first = false;
    }
}

void ArrangeView::resized()
{
    auto area = getLocalBounds();

    if (overview.isVisible())
    {
        auto overviewRow = area.removeFromTop (OverviewBar::height);
        overviewRow.removeFromLeft (theme::trackHeaderWidth);
        overview.setBounds (overviewRow);
    }

    auto rulerRow = area.removeFromTop (theme::rulerHeight);
    rulerRow.removeFromLeft (theme::trackHeaderWidth);
    ruler.setBounds (rulerRow);

    signatureTrack.setBounds (area.removeFromTop (SignatureTrack::totalHeight));
    markerTrack.setBounds (area.removeFromTop (MarkerTrack::height));

    auto scrollRow = area.removeFromBottom (scrollBarHeight);
    scrollRow.removeFromLeft (theme::trackHeaderWidth);
    horizontalScrollBar.setBounds (scrollRow);

    viewport.setBounds (area);
    layoutRows();
    applySettings();
}

void ArrangeView::zoomAt (double seconds, bool zoomIn)
{
    const auto laneX = settings.mapping.secondsToX (seconds);
    const auto factor = zoomIn ? 1.6 : 1.0 / 1.6;
    settings.mapping.pixelsPerSecond =
        juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, settings.mapping.pixelsPerSecond * factor);
    settings.mapping.firstVisibleSecond = juce::jmax (0.0, seconds - laneX / settings.mapping.pixelsPerSecond);
    applySettings();
}

void ArrangeView::showTimeRange (double fromSeconds, double toSeconds)
{
    const auto from = juce::jmax (0.0, fromSeconds);
    const auto length = juce::jmax (0.01, toSeconds - from);
    settings.mapping.pixelsPerSecond =
        juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, juce::jmax (1, getLaneWidth()) / length);
    settings.mapping.firstVisibleSecond = from;
    applySettings();
}

std::pair<double, double> ArrangeView::getVisibleRange() const
{
    const auto from = settings.mapping.firstVisibleSecond;
    return { from, from + getLaneWidth() / settings.mapping.pixelsPerSecond };
}

void ArrangeView::zoomToProject()
{
    auto length = getContentEndSeconds();

    if (length <= 0.0)
        length = 16.0 * timeline.barLengthSeconds (0.0);

    showTimeRange (0.0, length * 1.02); // etwas Luft hinter dem letzten Clip
}

bool ArrangeView::zoomToSelection()
{
    double from = std::numeric_limits<double>::max();
    double to = 0.0;

    if (rangeSelection && rangeSelection->toSeconds > rangeSelection->fromSeconds)
    {
        from = rangeSelection->fromSeconds;
        to = rangeSelection->toSeconds;
    }
    else
    {
        for (auto* track : tracks.getAudioTracks())
            for (const auto& clip : tracks.getClips (*track))
                if (selectedClips.contains (clip.id))
                {
                    from = std::min (from, clip.startSeconds);
                    to = std::max (to, clip.startSeconds + clip.lengthSeconds);
                }
    }

    if (to <= from)
        return false;

    const auto margin = (to - from) * 0.05; // Ränder nicht an die Kante kleben
    showTimeRange (from - margin, to + margin);
    return true;
}

void ArrangeView::zoomStep (bool zoomIn)
{
    // Um den Positionszeiger, wenn er sichtbar ist, sonst um die Mitte.
    const auto [from, to] = getVisibleRange();
    const auto playhead = transport.getPositionSeconds();
    zoomAt (playhead >= from && playhead <= to ? playhead : (from + to) / 2.0, zoomIn);
}

void ArrangeView::setOverviewShown (bool shouldShow)
{
    overview.setVisible (shouldShow);
    updateOverviewContent();
    resized();
}

void ArrangeView::updateOverviewContent()
{
    if (! overview.isVisible())
        return;

    std::vector<OverviewBar::Row> overviewRows;

    for (auto* track : displayedTracks())
    {
        OverviewBar::Row row;
        row.colour = tracks.getTrackInfo (*track).colour;

        for (const auto& clip : tracks.getClips (*track))
            row.clips.emplace_back (clip.startSeconds, clip.lengthSeconds);

        overviewRows.push_back (std::move (row));
    }

    const auto [from, to] = getVisibleRange();
    overview.setContent (std::move (overviewRows), juce::jmax (getContentEndSeconds() * 1.1, to));
}

void ArrangeView::nudgeSelectedClips (int direction)
{
    const auto ids = getSelectedClipIds();

    if (ids.empty())
        return;

    // Schrittweite am Anfang des frühesten ausgewählten Clips (bei Tempowechseln zählt die Stelle).
    double earliest = std::numeric_limits<double>::max();

    for (auto* track : tracks.getAudioTracks())
        for (const auto& clip : tracks.getClips (*track))
            if (selectedClips.contains (clip.id))
                earliest = std::min (earliest, clip.startSeconds);

    const auto step = timeline.gridStepSeconds (earliest, settings.gridResolution);
    const auto delta = direction < 0 ? -std::min (step, earliest) : step;

    if (delta == 0.0)
        return;

    if (const auto result = clips.moveClips (ids, delta, 0); ! result)
        reportError (result.error());
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

    if (event.mods.isCtrlDown())
    {
        // Zoom um die Mausposition: Die Sekunde unter dem Zeiger bleibt, wo sie ist.
        const auto laneX = event.x - theme::trackHeaderWidth;
        const auto anchorSeconds = settings.mapping.xToSeconds (laneX);
        const auto factor = delta > 0.0f ? 1.2 : 1.0 / 1.2;

        settings.mapping.pixelsPerSecond =
            juce::jlimit (minPixelsPerSecond, maxPixelsPerSecond, settings.mapping.pixelsPerSecond * factor);
        settings.mapping.firstVisibleSecond =
            juce::jmax (0.0, anchorSeconds - laneX / settings.mapping.pixelsPerSecond);
        applySettings();
    }
    else if (event.mods.isShiftDown() || wheel.deltaX != 0.0f)
    {
        const auto visibleSeconds = getLaneWidth() / settings.mapping.pixelsPerSecond;
        settings.mapping.firstVisibleSecond =
            juce::jmax (0.0, settings.mapping.firstVisibleSecond - delta * visibleSeconds * 0.5);
        applySettings();
    }
    else
    {
        viewport.mouseWheelMove (event, wheel);
    }
}

void ArrangeView::tracksChanged()
{
    rebuildRows();
}

void ArrangeView::contentChanged()
{
    // Dieselben Spuren, neuer Inhalt: Köpfe und Clips an Ort und Stelle auffrischen. Ein
    // Neuaufbau würde Wellenformen neu anlegen und laufende Mausaktionen abbrechen.
    for (auto& row : rows)
    {
        row.header->refresh();
        row.lane->refresh();
    }

    for (auto& folderRow : folderRows)
    {
        folderRow.header->refresh();
        folderRow.lane->refresh();
    }

    if (onTrackSelected)
        onTrackSelected (selectedTrack); // Inspector: Name, Eingang, Mute/Solo

    pruneClipSelection();
    layoutRows(); // aufgeklappte Takes ändern die Zeilenhöhe
    updateOverviewContent();
    updateScrollBar();
}

void ArrangeView::timelineChanged()
{
    ruler.repaint();

    for (auto& row : rows)
        row.lane->repaint();
}

void ArrangeView::scrollBarMoved (juce::ScrollBar* scrollBar, double newRangeStart)
{
    if (scrollBar == &horizontalScrollBar && std::abs (newRangeStart - settings.mapping.firstVisibleSecond) > 1.0e-6)
    {
        settings.mapping.firstVisibleSecond = juce::jmax (0.0, newRangeStart);
        applySettings();
    }
}

void ArrangeView::timerCallback()
{
    const bool recording = transport.isRecording();
    const auto currentTracks = displayedTracks();

    for (std::size_t i = 0; i < rows.size() && i < currentTracks.size(); ++i)
    {
        const bool armed = recording && tracks.isArmed (*currentTracks[i]);
        rows[i].lane->setRecordingPreview (
            armed ? std::optional (std::pair (transport.getRecordStartSeconds(), transport.getPositionSeconds()))
                  : std::nullopt);
    }

    if (isShowing())
        for (auto& row : rows)
            row.header->updateMeter();

    // Nur die beiden schmalen Streifen alter und neuer Position neu zeichnen, nicht die ganze
    // Projektzone: Bei 30 Bildern je Sekunde wären das sonst alle Spuren mit allen Clips.
    const auto x = settings.mapping.secondsToX (transport.getPositionSeconds());

    if (x != lastPlayheadX)
    {
        repaintPlayheadAt (lastPlayheadX);
        repaintPlayheadAt (x);
        lastPlayheadX = x;
    }
}

void ArrangeView::repaintPlayheadAt (int laneX)
{
    repaint (theme::trackHeaderWidth + laneX - 2, 0, 5, getHeight());
}

services::TrackHandle* ArrangeView::getSelectedTrack() const noexcept
{
    return selectedTrack;
}

std::vector<services::TrackHandle*> ArrangeView::displayedTracks() const
{
    std::vector<services::TrackHandle*> shown;

    for (const auto& entry : tracks.getTrackList())
        if (entry.track != nullptr)
            shown.push_back (entry.track);

    return shown;
}

juce::Component& ArrangeView::headerOf (const RowSlot& slot) const
{
    if (slot.isFolder)
        return *folderRows[slot.index].header;

    return *rows[slot.index].header;
}

void ArrangeView::addFolderRow (services::FolderHandle& folder, int depth)
{
    FolderRow row;
    row.folder = &folder;
    row.depth = depth;
    row.header = std::make_unique<FolderHeader> (tracks, folder);
    row.lane = std::make_unique<FolderLane> (tracks, folder);
    row.header->onReorderDrag = [this] (int screenY)
    {
        reorderTarget = reorderTargetAt (screenY);
        repaint();
    };
    row.header->onReorderDrop = [this, handle = &folder] (int screenY)
    {
        const auto target = reorderTargetAt (screenY);
        reorderTarget.reset();
        repaint();
        reorderTrack (*handle, target); // kann diese Zeile neu aufbauen: zuletzt
    };
    row.header->onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };

    rowsContainer.addAndMakeVisible (*row.header);
    rowsContainer.addAndMakeVisible (*row.lane);
    rowOrder.push_back ({ true, folderRows.size() });
    folderRows.push_back (std::move (row));
}

void ArrangeView::rebuildRows()
{
    rows.clear();
    folderRows.clear();
    rowOrder.clear();

    const auto currentTracks = displayedTracks();

    // Aufgeklappte Automation nur für Spuren merken, die es noch gibt (sonst zeigt das Set ins Leere).
    std::erase_if (tracksWithAutomationShown,
                   [&currentTracks] (services::TrackHandle* track)
                   { return std::find (currentTracks.begin(), currentTracks.end(), track) == currentTracks.end(); });
    bool selectionStillExists = false;
    int index = 0;

    for (const auto& entry : tracks.getTrackList())
    {
        if (entry.folder != nullptr)
        {
            addFolderRow (*entry.folder, entry.depth);
            continue;
        }

        auto* track = entry.track;
        const auto colour = tracks.getTrackInfo (*track).colour;
        TrackRow row;
        row.track = track;
        row.header = std::make_unique<TrackHeader> (tracks, mixer, *track);
        row.lane = std::make_unique<TrackLane> (tracks, clips, midiClips, timeline, waveforms, *track, colour);
        row.lane->setComponentID ("lane" + juce::String (index)); // für Tests: Spur-Zeitachse finden
        row.automationShown = tracksWithAutomationShown.contains (track);

        for (int laneIndex = 0; laneIndex < automation.getNumLanes (*track); ++laneIndex)
        {
            TrackRow::Lane lane { std::make_unique<AutomationHeader> (automation, *track, laneIndex),
                                  std::make_unique<AutomationLane> (automation, timeline, *track, laneIndex, colour) };
            lane.header->onSelected = [this, track] { selectTrack (track); };
            lane.lane->onSelected = [this, track] { selectTrack (track); };
            rowsContainer.addChildComponent (*lane.header);
            rowsContainer.addChildComponent (*lane.lane);
            lane.header->setVisible (row.automationShown);
            lane.lane->setVisible (row.automationShown);
            row.automationLanes.push_back (std::move (lane));
        }

        row.header->onSelected = [this, track] { selectTrack (track); };
        row.header->onAutomationToggled = [this, track] (bool shown) { setAutomationShown (track, shown); };
        row.header->onOpenInstrument = [this, track]
        {
            if (onOpenInstrument)
                onOpenInstrument (track);
        };
        row.header->onBakeRequested = [this, track] (BakeAction action)
        {
            if (onBakeRequested)
                onBakeRequested (track, action);
        };
        row.header->onReorderDrag = [this] (int screenY)
        {
            reorderTarget = reorderTargetAt (screenY);
            repaint();
        };
        row.header->onReorderDrop = [this, track] (int screenY)
        {
            const auto target = reorderTargetAt (screenY);
            reorderTarget.reset();
            repaint();
            reorderTrack (*track, target); // kann diese Zeile neu aufbauen: zuletzt
        };
        row.header->onError = [this] (const juce::String& message)
        {
            if (onError)
                onError (message);
        };
        row.header->setAutomationShown (row.automationShown);
        row.lane->onSelected = [this, track] { selectTrack (track); };
        row.lane->onOpenAudioClip = [this, track] (const juce::String& clipId)
        {
            if (onOpenAudioClip)
                onOpenAudioClip (track, clipId);
        };
        row.lane->onOpenMidiClip = [this] (const juce::String& clipId)
        {
            if (onOpenMidiClip)
                onOpenMidiClip (clipId);
        };
        row.lane->onClipClicked = [this] (const juce::String& clipId, juce::ModifierKeys mods)
        { selectClip (clipId, mods); };
        row.lane->onEmptyClicked = [this] { clearClipSelection(); };
        row.lane->onClipAction = [this] (ClipAction action) { performClipAction (action); };
        row.lane->onZoomRequested = [this] (double seconds, bool zoomIn) { zoomAt (seconds, zoomIn); };
        row.lane->onMarqueeStart = [this] (juce::Point<int> screen, juce::ModifierKeys) { marqueeStarted (screen); };
        row.lane->onMarqueeDrag = [this] (juce::Point<int> screen, juce::ModifierKeys mods)
        { marqueeMoved (screen, mods); };
        row.lane->onMarqueeEnd = [this] (juce::Point<int> screen, juce::ModifierKeys mods)
        { marqueeEnded (screen, mods); };
        row.lane->onRangeMenu = [this] (juce::Point<int> screen) { showRangeMenu (screen); };
        row.lane->onError = [this] (const juce::String& message)
        {
            if (onError)
                onError (message);
        };
        row.lane->onClipDragPreview = [this, rowIndex = static_cast<int> (index)] (double seconds, int screenY)
        {
            const auto target = rowIndexAtScreenY (screenY);
            dropPreview.reset();

            if (target >= 0 && target != rowIndex)
                dropPreview = DropPreview { target, seconds, TRANS ("Move here") };

            repaint();
        };
        row.lane->onClipMoveRequested =
            [this, rowIndex = static_cast<std::size_t> (index)] (const juce::String& clipId, double delta, int screenY)
        { moveClips (clipId, delta, screenY, rowIndex); };
        row.header->setSelected (track == selectedTrack);
        selectionStillExists = selectionStillExists || track == selectedTrack;

        rowsContainer.addAndMakeVisible (*row.header);
        rowsContainer.addAndMakeVisible (*row.lane);
        row.depth = entry.depth;
        rowOrder.push_back ({ false, rows.size() });
        rows.push_back (std::move (row));
        ++index;
    }

    // Bereichsauswahl nur auf Spuren, die es noch gibt.
    if (rangeSelection)
    {
        std::erase_if (rangeSelection->tracks,
                       [&currentTracks] (services::TrackHandle* track)
                       { return std::ranges::find (currentTracks, track) == currentTracks.end(); });

        if (rangeSelection->tracks.empty())
            rangeSelection.reset();
    }

    // Die ausgewählte Spur kann gelöscht worden sein: dann Auswahl aufheben, nie auf sie zeigen.
    // Ohne Auswahl die letzte Spur nehmen (die zuletzt angelegte), damit der Inspector nie leer bleibt.
    if (! selectionStillExists)
        selectTrack (currentTracks.empty() ? nullptr : currentTracks.back());
    else if (onTrackSelected)
        onTrackSelected (selectedTrack); // Inspector aktualisieren (Name, Eingänge, Inserts)

    pruneClipSelection();
    layoutRows();
    updateOverviewContent();
    applySettings();
}

void ArrangeView::pruneClipSelection()
{
    // Auswahl auf Clips beschränken, die es noch gibt (gelöschte fallen heraus).
    std::set<juce::String> existing;

    for (auto* track : tracks.getAudioTracks())
        for (const auto& clip : tracks.getClips (*track))
            existing.insert (clip.id);

    const auto before = selectedClips.size();
    std::erase_if (selectedClips, [&existing] (const juce::String& id) { return ! existing.contains (id); });
    pushClipSelectionToRows();

    if (before != selectedClips.size() && onClipSelectionChanged)
        onClipSelectionChanged();
}

std::vector<juce::String> ArrangeView::getSelectedClipIds() const
{
    return { selectedClips.begin(), selectedClips.end() };
}

void ArrangeView::selectClip (const juce::String& clipId, juce::ModifierKeys mods)
{
    if (mods.isCtrlDown())
    {
        if (! selectedClips.erase (clipId))
            selectedClips.insert (clipId);
    }
    else if (mods.isShiftDown())
    {
        selectedClips.insert (clipId);
    }
    else
    {
        selectedClips.clear();
        selectedClips.insert (clipId);
    }

    pushClipSelectionToRows();

    if (onClipSelectionChanged)
        onClipSelectionChanged();
}

void ArrangeView::selectAllClips()
{
    selectedClips.clear();

    for (auto* track : displayedTracks())
        for (const auto& clip : tracks.getClips (*track))
            selectedClips.insert (clip.id);

    pushClipSelectionToRows();

    if (onClipSelectionChanged)
        onClipSelectionChanged();
}

void ArrangeView::clearClipSelection()
{
    if (selectedClips.empty())
        return;

    selectedClips.clear();
    pushClipSelectionToRows();

    if (onClipSelectionChanged)
        onClipSelectionChanged();
}

void ArrangeView::pushClipSelectionToRows()
{
    for (auto& row : rows)
        row.lane->setSelectedClips (selectedClips);
}

bool ArrangeView::canPerformClipAction (ClipAction action) const
{
    switch (action)
    {
        case ClipAction::paste:
            return clips.hasClipboardContent() && selectedTrack != nullptr;
        case ClipAction::merge:
            return selectedClips.size() >= 2;
        case ClipAction::cut:
        case ClipAction::copy:
        case ClipAction::duplicate:
        case ClipAction::remove:
        case ClipAction::toggleMute:
        case ClipAction::normalize:
        case ClipAction::reverse:
        case ClipAction::invertPhase:
            return ! selectedClips.empty();
    }

    return false;
}

void ArrangeView::performClipAction (ClipAction action)
{
    const auto ids = getSelectedClipIds();

    switch (action)
    {
        case ClipAction::cut:
            if (const auto result = clips.cutClips (ids); ! result)
                reportError (result.error());
            break;

        case ClipAction::copy:
            if (const auto result = clips.copyClips (ids); ! result)
                reportError (result.error());
            break;

        case ClipAction::paste:
        {
            if (selectedTrack == nullptr)
            {
                if (onError)
                    onError (TRANS ("Select a track first to paste."));
                break;
            }

            const auto pasted = clips.pasteClips (*selectedTrack, transport.getPositionSeconds());

            if (! pasted)
            {
                reportError (pasted.error());
                break;
            }

            selectedClips = { pasted->begin(), pasted->end() }; // die Kopien sind jetzt die Auswahl
            break;
        }

        case ClipAction::duplicate:
        {
            const auto duplicated = clips.duplicateClips (ids);

            if (! duplicated)
            {
                reportError (duplicated.error());
                break;
            }

            selectedClips = { duplicated->begin(), duplicated->end() };
            break;
        }

        case ClipAction::merge:
        {
            const auto merged = clips.mergeClips (ids);

            if (! merged)
            {
                reportError (merged.error());
                break;
            }

            selectedClips = { *merged };
            break;
        }

        case ClipAction::remove:
            if (const auto result = clips.deleteClips (ids); ! result)
                reportError (result.error());
            break;

        case ClipAction::toggleMute:
        {
            // Einer hörbar: alle stumm. Alle stumm: alle hörbar. So wirkt die Taste wie ein Schalter.
            bool anyAudible = false;

            for (auto* track : tracks.getAudioTracks())
                for (const auto& clip : tracks.getClips (*track))
                    if (selectedClips.contains (clip.id) && ! clip.muted)
                        anyAudible = true;

            if (const auto result = clips.setClipsMuted (ids, anyAudible); ! result)
                reportError (result.error());

            break;
        }

        case ClipAction::normalize:
            if (const auto result = clips.normalizeClips (ids); ! result)
                reportError (result.error());
            break;

        case ClipAction::reverse:
        {
            // Wie Mute ein Schalter: Ist einer vorwärts, laufen danach alle rückwärts.
            bool anyForward = false;

            for (auto* track : tracks.getAudioTracks())
                for (const auto& clip : tracks.getClips (*track))
                    if (selectedClips.contains (clip.id) && ! clip.isMidi && ! clip.reversed)
                        anyForward = true;

            if (const auto result = clips.setClipsReversed (ids, anyForward); ! result)
                reportError (result.error());

            break;
        }

        case ClipAction::invertPhase:
            if (const auto result = clips.invertPhase (ids); ! result)
                reportError (result.error());
            break;
    }

    // Die Zeilen bauen sich nach der Änderung asynchron neu; bis dahin die Auswahl schon zeigen.
    pushClipSelectionToRows();

    if (onClipSelectionChanged)
        onClipSelectionChanged();
}

ArrangeView::ReorderTarget ArrangeView::reorderTargetAt (int screenY) const
{
    ReorderTarget target;

    for (const auto& slot : rowOrder)
    {
        const auto& header = headerOf (slot);
        const auto bounds = header.getScreenBounds();

        // Mittlere Hälfte eines Ordnerkopfs: hinein (als letzte Spur).
        if (slot.isFolder && screenY >= bounds.getY() + bounds.getHeight() / 4 &&
            screenY < bounds.getBottom() - bounds.getHeight() / 4)
        {
            target.into = folderRows[slot.index].folder;
            target.folderArea = getLocalArea (&rowsContainer, header.getBounds());
            return target;
        }

        // Über der Mitte eines Kopfs: davor (im selben Ordner wie er).
        if (screenY < bounds.getCentreY())
        {
            target.before = slot.isFolder ? static_cast<services::TrackOrFolderHandle*> (folderRows[slot.index].folder)
                                          : static_cast<services::TrackOrFolderHandle*> (rows[slot.index].track);
            target.lineY = getLocalArea (&rowsContainer, header.getBounds()).getY();
            return target;
        }
    }

    // Unter allem: ans Ende der obersten Ebene.
    if (! rowOrder.empty())
        target.lineY = getLocalArea (&rowsContainer, headerOf (rowOrder.back()).getBounds()).getBottom();

    return target;
}

void ArrangeView::reorderTrack (services::TrackOrFolderHandle& track, const ReorderTarget& target)
{
    const auto result = target.into != nullptr ? tracks.moveIntoFolder (track, target.into)
                                               : tracks.moveTrackBefore (track, target.before);

    if (! result)
        reportError (result.error());
}

int ArrangeView::rowIndexAtScreenY (int screenY) const
{
    const auto y = rowsContainer.getLocalPoint (nullptr, juce::Point<int> (0, screenY)).y;

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        // Bis zum Ende der eigenen Automationslagen; Ordnerzeilen dazwischen gehören zu keiner Spur.
        const auto top = rows[i].lane->getY();
        const auto bottom =
            rows[i].lane->getBottom() +
            (rows[i].automationShown ? static_cast<int> (rows[i].automationLanes.size()) * theme::automationLaneHeight
                                     : 0);

        if (y >= top && y < bottom)
            return static_cast<int> (i);
    }

    return -1;
}

void ArrangeView::moveClips (const juce::String& draggedClipId, double deltaSeconds, int screenY, std::size_t sourceRow)
{
    dropPreview.reset();
    repaint();

    const auto targetRow = rowIndexAtScreenY (screenY);
    const auto trackDelta = targetRow < 0 ? 0 : targetRow - static_cast<int> (sourceRow);

    // Gehört der gezogene Clip zur Auswahl, zieht die ganze Auswahl mit.
    const auto ids =
        selectedClips.contains (draggedClipId) ? getSelectedClipIds() : std::vector<juce::String> { draggedClipId };

    if (const auto result = clips.moveClips (ids, deltaSeconds, trackDelta); ! result)
        reportError (result.error());
}

void ArrangeView::reportError (const core::Error& error)
{
    const auto text = services::errorText (error);
    juce::Logger::writeToLog ("Clip-Aktion fehlgeschlagen: " + text);

    if (onError)
        onError (text);
}

void ArrangeView::selectTrack (services::TrackHandle* track)
{
    selectedTrack = track;
    const auto currentTracks = displayedTracks();

    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i].header->setSelected (i < currentTracks.size() && currentTracks[i] == track);

    if (onTrackSelected)
        onTrackSelected (selectedTrack);
}

void ArrangeView::setAutomationShown (services::TrackHandle* track, bool shown)
{
    if (shown)
        tracksWithAutomationShown.insert (track);
    else
        tracksWithAutomationShown.erase (track);

    // Aufklappen ohne Lage: eine leere Lage anlegen, damit nie ein leerer Bereich erscheint. Das
    // ändert das Projekt und baut die Zeilen über automationChanged() neu.
    if (shown && track != nullptr && automation.getNumLanes (*track) == 0)
    {
        automation.addLane (*track);
        return;
    }

    const auto currentTracks = displayedTracks();

    for (std::size_t i = 0; i < rows.size() && i < currentTracks.size(); ++i)
    {
        if (currentTracks[i] != track)
            continue;

        rows[i].automationShown = shown;

        for (auto& lane : rows[i].automationLanes)
        {
            lane.header->setVisible (shown);
            lane.lane->setVisible (shown);
            lane.lane->setSettings (settings);
            lane.lane->refresh();
        }
    }

    layoutRows();
}

void ArrangeView::automationChanged()
{
    // Lagen hinzugefügt oder entfernt (auch per Undo): Zeilen neu bauen. Sonst reicht Neuzeichnen,
    // das die Lagen selbst erledigen.
    const auto currentTracks = displayedTracks();

    for (std::size_t i = 0; i < rows.size() && i < currentTracks.size(); ++i)
    {
        if (static_cast<int> (rows[i].automationLanes.size()) != automation.getNumLanes (*currentTracks[i]))
        {
            if (automation.getNumLanes (*currentTracks[i]) == 0)
                tracksWithAutomationShown.erase (currentTracks[i]);

            rebuildRows();
            return;
        }
    }
}

void ArrangeView::setTrackHeight (int height)
{
    trackHeight = juce::jlimit (minTrackHeight, maxTrackHeight, height);
    layoutRows();
}

int ArrangeView::getTrackHeight() const noexcept
{
    return trackHeight;
}

void ArrangeView::layoutRows()
{
    const auto width = juce::jmax (viewport.getMaximumVisibleWidth(), 1);
    const auto trackRowHeight = [this] (const TrackRow& row)
    {
        // Take-Lanes (Comping) machen die Spur nach unten höher; Kopf und Spur bleiben bündig.
        return trackHeight + row.lane->getExtraHeight() +
               (row.automationShown ? static_cast<int> (row.automationLanes.size()) * theme::automationLaneHeight : 0);
    };

    int height = 0;

    for (const auto& slot : rowOrder)
        height += slot.isFolder ? theme::folderRowHeight : trackRowHeight (rows[slot.index]);

    rowsContainer.setSize (width, height);

    int y = 0;

    for (const auto& slot : rowOrder)
    {
        if (slot.isFolder)
        {
            auto& folderRow = folderRows[slot.index];
            const int indent = folderRow.depth * theme::folderIndent;
            folderRow.header->setBounds (indent, y, theme::trackHeaderWidth - indent, theme::folderRowHeight);
            folderRow.lane->setBounds (
                theme::trackHeaderWidth, y, width - theme::trackHeaderWidth, theme::folderRowHeight);
            y += theme::folderRowHeight;
            continue;
        }

        auto& row = rows[slot.index];
        const int indent = row.depth * theme::folderIndent;
        const int rowHeight = trackHeight + row.lane->getExtraHeight();
        row.header->setBounds (indent, y, theme::trackHeaderWidth - indent, rowHeight);
        row.lane->setBounds (theme::trackHeaderWidth, y, width - theme::trackHeaderWidth, rowHeight);
        y += rowHeight;

        if (! row.automationShown)
            continue;

        for (auto& lane : row.automationLanes)
        {
            lane.header->setBounds (indent, y, theme::trackHeaderWidth - indent, theme::automationLaneHeight);
            lane.lane->setBounds (
                theme::trackHeaderWidth, y, width - theme::trackHeaderWidth, theme::automationLaneHeight);
            y += theme::automationLaneHeight;
        }
    }
}

void ArrangeView::applySettings()
{
    ruler.setTimeMapping (settings.mapping);
    signatureTrack.setSettings (settings);
    markerTrack.setSettings (settings);
    ruler.setGridResolution (settings.gridResolution);
    ruler.setSnapEnabled (settings.snapEnabled);

    for (auto& row : rows)
    {
        row.lane->setSettings (settings);

        for (auto& lane : row.automationLanes)
            lane.lane->setSettings (settings);
    }

    for (auto& folderRow : folderRows)
        folderRow.lane->setMapping (settings.mapping);

    updateScrollBar();
    repaint();
}

void ArrangeView::updateScrollBar()
{
    const auto visibleSeconds = getLaneWidth() / settings.mapping.pixelsPerSecond;

    if (overview.isVisible())
    {
        // Ragt der Ausschnitt über die Übersicht hinaus, wächst sie mit.
        if (settings.mapping.firstVisibleSecond + visibleSeconds > overview.getLengthSeconds())
            updateOverviewContent();

        overview.setVisibleRange (settings.mapping.firstVisibleSecond,
                                  settings.mapping.firstVisibleSecond + visibleSeconds);
    }

    horizontalScrollBar.setRangeLimits (0.0, getTimelineLengthSeconds() + visibleSeconds, juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange (
        settings.mapping.firstVisibleSecond, visibleSeconds, juce::dontSendNotification);
}

double ArrangeView::getContentEndSeconds() const
{
    double lastClipEnd = 0.0;

    for (auto* track : tracks.getAudioTracks())
        for (const auto& clip : tracks.getClips (*track))
            lastClipEnd = juce::jmax (lastClipEnd, clip.startSeconds + clip.lengthSeconds);

    return lastClipEnd;
}

double ArrangeView::getTimelineLengthSeconds() const
{
    return juce::jmax (minTimelineSeconds,
                       getContentEndSeconds() + timelinePaddingSeconds,
                       transport.getPositionSeconds() + timelinePaddingSeconds);
}

int ArrangeView::getLaneWidth() const
{
    return juce::jmax (1, getWidth() - theme::trackHeaderWidth);
}

} // namespace fk::ui
