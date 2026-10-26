#include "fk/ui/DrumEditor.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/MediaBrowser.h"
#include "fk/ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace fk::ui
{

namespace
{
constexpr int headerHeight = 34;
constexpr int hitRadius = 7;     ///< halbe Breite einer Raute in Pixeln
constexpr int refreshHz = 30;
constexpr int mapAutomaticId = 1; ///< ComboBox: automatisch nach Instrument
constexpr int mapNoneId = 2;      ///< ComboBox: keine Map (Key Editor)
constexpr int mapFirstId = 10;    ///< ComboBox: Maps ab hier, Index = ID - mapFirstId

const std::array<std::pair<const char*, int>, 4> velocityChoices { { { NEEDS_TRANS ("Soft (40)"), 40 },
                                                                      { NEEDS_TRANS ("Medium (80)"), 80 },
                                                                      { NEEDS_TRANS ("Loud (110)"), 110 },
                                                                      { NEEDS_TRANS ("Full (127)"), 127 } } };
} // namespace

// ---------------------------------------------------------------------------------------------
// DrumGrid

DrumGrid::DrumGrid()
{
    setTooltip (TRANS ("Click: add or delete a hit. Drag a diamond vertically: change velocity."));
}

void DrumGrid::setRows (std::vector<DrumRow> newRows)
{
    rows = std::move (newRows);
    updateSize();
    repaint();
}

void DrumGrid::setNotes (std::vector<services::EditableNote> newNotes)
{
    notes = std::move (newNotes);
    repaint();
}

void DrumGrid::setLengthBeats (double beats)
{
    lengthBeats = std::max (beats, 0.25);
    updateSize();
}

void DrumGrid::setGridBeats (double beats)
{
    gridBeats = std::max (beats, 1.0 / 32.0);
    repaint();
}

void DrumGrid::setPixelsPerBeat (double pixels)
{
    pixelsPerBeat = std::clamp (pixels, 10.0, 800.0);
    updateSize();
}

void DrumGrid::setPlayheadBeats (std::optional<double> beats)
{
    if (playheadBeats != beats)
    {
        playheadBeats = beats;
        repaint();
    }
}

void DrumGrid::updateSize()
{
    setSize (std::max (1, juce::roundToInt (lengthBeats * pixelsPerBeat) + 1),
             std::max (rowHeight, static_cast<int> (rows.size()) * rowHeight));
    repaint();
}

int DrumGrid::xForBeats (double beats) const
{
    return juce::roundToInt (beats * pixelsPerBeat);
}

int DrumGrid::rowForNote (int note) const
{
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].note == note)
            return static_cast<int> (i);

    return -1;
}

double DrumGrid::snapped (double beats) const
{
    return std::clamp (std::floor (beats / gridBeats + 0.5) * gridBeats, 0.0, std::max (0.0, lengthBeats - gridBeats));
}

std::optional<int> DrumGrid::hitAt (juce::Point<int> position) const
{
    const int row = position.y / rowHeight;

    if (row < 0 || row >= static_cast<int> (rows.size()))
        return std::nullopt;

    std::optional<int> best;
    int bestDistance = hitRadius + 2;

    for (const auto& note : notes)
        if (note.noteNumber == rows[static_cast<std::size_t> (row)].note)
            if (const int distance = std::abs (xForBeats (note.startBeats) - position.x); distance < bestDistance)
            {
                bestDistance = distance;
                best = note.index;
            }

    return best;
}

void DrumGrid::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);

    // Zeilen: abwechselnd hinterlegt, Linie zwischen Gruppen (Snare, Toms, Becken ...).
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const int y = static_cast<int> (i) * rowHeight;

        if (i % 2 == 1)
        {
            g.setColour (theme::backgroundPanel.withAlpha (0.5f));
            g.fillRect (0, y, getWidth(), rowHeight);
        }

        if (i > 0 && rows[i].group != rows[i - 1].group)
        {
            g.setColour (theme::borderStrong);
            g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));
        }
    }

    // Raster: Schläge kräftig, Rasterschritte fein.
    for (double beat = 0.0; beat <= lengthBeats + 1.0e-9; beat += gridBeats)
    {
        const bool onBeat = std::abs (beat - std::round (beat)) < 1.0e-6;
        g.setColour (onBeat ? theme::gridBar : theme::gridBeat.withAlpha (0.5f));
        g.drawVerticalLine (xForBeats (beat), 0.0f, static_cast<float> (getHeight()));
    }

    // Schläge als Rauten; Farbe und Größe zeigen den Anschlag.
    for (const auto& note : notes)
    {
        const int row = rowForNote (note.noteNumber);

        if (row < 0)
            continue;

        const int velocity = drag && drag->noteIndex == note.index ? drag->currentVelocity : note.velocity;
        const float size = static_cast<float> (hitRadius) * (0.6f + 0.4f * static_cast<float> (velocity) / 127.0f);
        const auto centre = juce::Point<float> (static_cast<float> (xForBeats (note.startBeats)), static_cast<float> (yForRow (row)));

        juce::Path diamond;
        diamond.startNewSubPath (centre.x, centre.y - size);
        diamond.lineTo (centre.x + size, centre.y);
        diamond.lineTo (centre.x, centre.y + size);
        diamond.lineTo (centre.x - size, centre.y);
        diamond.closeSubPath();

        g.setColour (theme::accent.withAlpha (0.35f + 0.65f * static_cast<float> (velocity) / 127.0f));
        g.fillPath (diamond);
        g.setColour (theme::textPrimary.withAlpha (0.8f));
        g.strokePath (diamond, juce::PathStrokeType (1.0f));
    }

    if (playheadBeats)
    {
        g.setColour (theme::record);
        g.drawVerticalLine (xForBeats (*playheadBeats), 0.0f, static_cast<float> (getHeight()));
    }
}

void DrumGrid::mouseDown (const juce::MouseEvent& event)
{
    if (const auto hit = hitAt (event.getPosition()))
    {
        const auto found = std::ranges::find (notes, *hit, &services::EditableNote::index);
        const int velocity = found != notes.end() ? found->velocity : 100;
        drag = VelocityDrag { *hit, velocity, velocity, false };
        return;
    }

    const int row = event.y / rowHeight;

    if (row < 0 || row >= static_cast<int> (rows.size()))
        return;

    const auto note = rows[static_cast<std::size_t> (row)].note;

    if (onPreview)
        onPreview (note, newVelocity);

    if (onAddHit)
        onAddHit (note, snapped (static_cast<double> (event.x) / pixelsPerBeat));
}

void DrumGrid::mouseDrag (const juce::MouseEvent& event)
{
    if (! drag)
        return;

    const int delta = -event.getDistanceFromDragStartY();

    if (std::abs (delta) < 3 && ! drag->moved)
        return;

    drag->moved = true;
    drag->currentVelocity = std::clamp (drag->startVelocity + delta, 1, 127);
    repaint();
}

void DrumGrid::mouseUp (const juce::MouseEvent&)
{
    if (! drag)
        return;

    const auto finished = *drag;
    drag.reset();

    if (finished.moved)
    {
        if (onVelocityChanged)
            onVelocityChanged (finished.noteIndex, finished.currentVelocity);
    }
    else if (onRemoveHit)
    {
        onRemoveHit (finished.noteIndex);
    }

    repaint();
}

// ---------------------------------------------------------------------------------------------
// DrumNames

void DrumNames::setRows (std::vector<DrumRow> newRows)
{
    rows = std::move (newRows);
    setSize (width, std::max (DrumGrid::rowHeight, static_cast<int> (rows.size()) * DrumGrid::rowHeight));
    setTooltip (TRANS ("Click plays the instrument"));
    repaint();
}

void DrumNames::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const int y = static_cast<int> (i) * DrumGrid::rowHeight;

        if (i > 0 && rows[i].group != rows[i - 1].group)
        {
            g.setColour (theme::borderStrong);
            g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));
        }

        g.setColour (rows[i].inMap ? theme::textPrimary : theme::textSecondary);
        g.setFont (juce::FontOptions (12.0f));

        if (rows[i].sampleName.isEmpty())
        {
            g.drawText (rows[i].name, 6, y, getWidth() - 40, DrumGrid::rowHeight, juce::Justification::centredLeft, true);
        }
        else
        {
            // Sampler-Spur: Name oben, das Sample dieser Note klein darunter.
            g.drawText (rows[i].name, 6, y, getWidth() - 40, DrumGrid::rowHeight / 2 + 3, juce::Justification::centredLeft, true);
            g.setColour (theme::accent);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (rows[i].sampleName, 6, y + DrumGrid::rowHeight / 2, getWidth() - 40, DrumGrid::rowHeight / 2,
                        juce::Justification::centredLeft, true);
        }
        g.setColour (theme::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (rows[i].note, true, true, 3),
                    getWidth() - 36,
                    y,
                    32,
                    DrumGrid::rowHeight,
                    juce::Justification::centredRight);
    }

    g.setColour (theme::borderStrong);
    g.drawVerticalLine (getWidth() - 1, 0.0f, static_cast<float> (getHeight()));
}

void DrumNames::mouseDown (const juce::MouseEvent& event)
{
    const int row = event.y / DrumGrid::rowHeight;

    if (row >= 0 && row < static_cast<int> (rows.size()) && onPreview)
        onPreview (rows[static_cast<std::size_t> (row)].note);
}

// ---------------------------------------------------------------------------------------------
// DrumEditor

DrumEditor::DrumEditor (Services services)
    : midiClips (services.midiClips)
    , preview (services.preview)
    , transport (services.transport)
    , timeline (services.timeline)
    , drumMaps (services.drumMaps)
    , tracks (services.tracks)
    , sampler (services.sampler)
    , usedOnlyButton (Icon::eye, TRANS ("Show only instruments that occur in the clip"))
    , quantiseButton (Icon::snap, TRANS ("Quantize: snap all hits to the grid"))
    , previewButton (Icon::keyboard, TRANS ("Audition: placing a hit or clicking a name plays the instrument"))
{
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    mapBox.setTooltip (TRANS ("Drum map of the track: names and order of the instruments. \"None\" opens clips of this track in the Key Editor again."));
    mapBox.onChange = [this]
    {
        auto* track = tracks.findTrack (trackId);

        if (track == nullptr)
            return;

        const auto id = mapBox.getSelectedId();
        const auto mapId = id == mapAutomaticId ? juce::String()
                           : id == mapNoneId    ? juce::String (services::DrumMapService::noMapId)
                                                : mapChoices[static_cast<std::size_t> (id - mapFirstId)].id;

        if (const auto result = drumMaps.setMapForTrack (*track, mapId); ! result)
            reportError (result.error());

        reload();
    };
    addAndMakeVisible (mapBox);

    usedOnlyButton.setClickingTogglesState (true);
    usedOnlyButton.onClick = [this] { reload(); };
    addAndMakeVisible (usedOnlyButton);

    for (std::size_t i = 0; i < noteValues.size(); ++i)
        gridBox.addItem (noteValues[i].name(), static_cast<int> (i) + 1);

    // Vorgabe Sechzehntel, wie im Schlagzeug-Editor üblich.
    for (std::size_t i = 0; i < noteValues.size(); ++i)
        if (noteValues[i].feel == services::NoteValue::Feel::straight && std::abs (noteValues[i].beats() - 0.25) < 1.0e-9)
            gridBox.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);

    gridBox.setTooltip (TRANS ("Grid: where new hits snap to, target when quantizing"));
    gridBox.onChange = [this] { grid.setGridBeats (currentGridBeats()); };
    addAndMakeVisible (gridBox);

    for (std::size_t i = 0; i < velocityChoices.size(); ++i)
        velocityBox.addItem (juce::translate (velocityChoices[i].first), velocityChoices[i].second);

    velocityBox.setSelectedId (110, juce::dontSendNotification);
    velocityBox.setTooltip (TRANS ("Velocity of new hits"));
    velocityBox.onChange = [this] { grid.setNewVelocity (velocityBox.getSelectedId()); };
    addAndMakeVisible (velocityBox);

    quantiseButton.onClick = [this]
    {
        if (clipId.isNotEmpty())
            if (const auto result = midiClips.quantise (clipId, currentGridBeats()); ! result)
                reportError (result.error());
    };
    addAndMakeVisible (quantiseButton);

    previewButton.setClickingTogglesState (true);
    previewButton.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (previewButton);

    grid.setGridBeats (currentGridBeats());
    grid.setNewVelocity (velocityBox.getSelectedId());
    grid.onPreview = [this] (int note, int velocity)
    {
        if (previewButton.getToggleState() && clipId.isNotEmpty())
            preview.startNote (clipId, note, velocity);
    };
    grid.onAddHit = [this] (int note, double beats)
    {
        if (const auto result = midiClips.addNote (clipId, note, beats, currentGridBeats(), velocityBox.getSelectedId());
            ! result)
            reportError (result.error());
    };
    grid.onRemoveHit = [this] (int noteIndex)
    {
        if (const auto result = midiClips.removeNote (clipId, noteIndex); ! result)
            reportError (result.error());
    };
    grid.onVelocityChanged = [this] (int noteIndex, int velocity)
    {
        if (const auto result = midiClips.setNoteVelocity (clipId, noteIndex, velocity); ! result)
            reportError (result.error());
    };

    names.onPreview = [this] (int note)
    {
        if (previewButton.getToggleState() && clipId.isNotEmpty())
            preview.startNote (clipId, note, velocityBox.getSelectedId());
    };

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, true);
    viewport.onScrolled = [this] (int y) { names.setTopLeftPosition (0, -y); };
    addAndMakeVisible (viewport);
    namesHolder.addAndMakeVisible (names);
    addAndMakeVisible (namesHolder);

    emptyHint.setText (TRANS ("Drum Editor: double-click a MIDI clip on a drum track to open it here. The track needs a drum map (automatic with FK Drums and drum plugins, otherwise choose one here). Your own kit: Track > Sampler Track (empty drum kit), then drag samples onto the rows."),
                       juce::dontSendNotification);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (emptyHint);

    midiClips.addListener (*this);
    startTimerHz (refreshHz);
    reload();
}

DrumEditor::~DrumEditor()
{
    midiClips.removeListener (*this);
}

void DrumEditor::setClip (const juce::String& newClipId)
{
    clipId = newClipId;
    reload();
}

void DrumEditor::setShowUsedOnly (bool usedOnly)
{
    usedOnlyButton.setToggleState (usedOnly, juce::dontSendNotification);
    reload();
}

double DrumEditor::currentGridBeats() const
{
    const auto index = gridBox.getSelectedId() - 1;
    return index >= 0 && index < static_cast<int> (noteValues.size()) ? noteValues[static_cast<std::size_t> (index)].beats()
                                                                       : 0.25;
}

void DrumEditor::rebuildMapChoices()
{
    mapChoices = drumMaps.getMaps();
    mapBox.clear (juce::dontSendNotification);
    mapBox.addItem (TRANS ("Automatic (by instrument)"), mapAutomaticId);
    mapBox.addItem (TRANS ("None (Key Editor)"), mapNoneId);

    for (std::size_t i = 0; i < mapChoices.size(); ++i)
        mapBox.addItem (mapChoices[i].name, mapFirstId + static_cast<int> (i));

    const auto assigned = drumMaps.getAssignedMapId (trackId);

    if (assigned.isEmpty())
        mapBox.setSelectedId (mapAutomaticId, juce::dontSendNotification);
    else if (assigned == services::DrumMapService::noMapId)
        mapBox.setSelectedId (mapNoneId, juce::dontSendNotification);
    else
        for (std::size_t i = 0; i < mapChoices.size(); ++i)
            if (mapChoices[i].id == assigned)
                mapBox.setSelectedId (mapFirstId + static_cast<int> (i), juce::dontSendNotification);
}

void DrumEditor::reload()
{
    const auto clip = clipId.isNotEmpty() ? midiClips.getClip (clipId) : core::Result<services::MidiClipInfo> (
                                                                           core::fail (core::ErrorCode::notFound, TRANS ("No clip").toStdString()));
    const bool hasClip = clip.has_value();

    for (auto* component : std::initializer_list<juce::Component*> {
             &titleLabel, &mapBox, &usedOnlyButton, &gridBox, &velocityBox, &quantiseButton, &previewButton, &viewport, &namesHolder })
        component->setVisible (hasClip);

    emptyHint.setVisible (! hasClip);

    if (! hasClip)
    {
        rows.clear();
        return;
    }

    trackId = clip->trackId;
    clipLengthBeats = clip->lengthBeats;
    clipStartBeats = timeline.secondsToBeats (clip->startSeconds);
    rebuildMapChoices();

    const auto map = drumMaps.getMapForTrack (trackId);
    titleLabel.setText (map ? plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{clip} · {map}")), { { "clip", clip->name }, { "map", map->name } })
                            : plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{clip} · no drum map")), { { "clip", clip->name } }),
                        juce::dontSendNotification);

    std::set<int> used;

    for (const auto& note : clip->notes)
        used.insert (note.noteNumber);

    rows.clear();
    std::set<int> listed;

    if (map)
        for (const auto& entry : map->entries)
            if (! usedOnlyButton.getToggleState() || used.contains (entry.note))
            {
                rows.push_back ({ entry.note, entry.name, entry.group, true });
                listed.insert (entry.note);
            }

    // Noten, die die Map nicht kennt, kommen unten dazu: Nichts im Clip bleibt unsichtbar.
    for (const auto note : used)
        if (! listed.contains (note))
            rows.push_back ({ note,
                              plugins::i18n::format (TRANS ("{note} (not in the map)"),
                                                      { { "note", juce::MidiMessage::getMidiNoteName (note, true, true, 3) } }),
                              "?",
                              false });

    // Sampler-Spur: Samples an ihre Zeilen schreiben (Namensspalte zeigt sie).
    if (auto* track = tracks.findTrack (trackId); track != nullptr && sampler.isSamplerTrack (*track))
        for (const auto& region : sampler.getRegions (*track))
            for (auto& row : rows)
                if (row.note == region.note && ! region.chromatic)
                    row.sampleName = region.sample.getFileNameWithoutExtension();

    grid.setRows (rows);
    grid.setNotes (clip->notes);
    grid.setLengthBeats (clipLengthBeats);
    names.setRows (rows);
    resized();
}

void DrumEditor::resized()
{
    auto area = getLocalBounds();
    emptyHint.setBounds (area.reduced (theme::spacingLarge));

    auto header = area.removeFromTop (headerHeight).reduced (theme::spacingSmall);
    titleLabel.setBounds (header.removeFromLeft (220));
    mapBox.setBounds (header.removeFromLeft (200).reduced (2, 0));
    usedOnlyButton.setBounds (header.removeFromLeft (header.getHeight()));
    header.removeFromLeft (theme::spacingMedium);
    gridBox.setBounds (header.removeFromLeft (110).reduced (2, 0));
    velocityBox.setBounds (header.removeFromLeft (110).reduced (2, 0));
    quantiseButton.setBounds (header.removeFromLeft (header.getHeight()));
    previewButton.setBounds (header.removeFromLeft (header.getHeight()));

    namesHolder.setBounds (area.removeFromLeft (DrumNames::width).withTrimmedBottom (viewport.getScrollBarThickness()));
    viewport.setBounds (area);
    names.setTopLeftPosition (0, -viewport.getViewPositionY());
}

void DrumEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
}

void DrumEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCtrlDown())
    {
        grid.setPixelsPerBeat (grid.getPixelsPerBeat() * (wheel.deltaY > 0.0f ? 1.2 : 1.0 / 1.2));
        return;
    }

    Component::mouseWheelMove (event, wheel);
}

namespace
{
juce::StringArray filesFromMediaDrag (const juce::var& description)
{
    const auto text = description.toString();

    if (! text.startsWith (MediaBrowser::dragDescriptionPrefix))
        return {};

    juce::StringArray files;
    files.addLines (text.fromFirstOccurrenceOf (MediaBrowser::dragDescriptionPrefix, false, false));
    files.removeEmptyStrings();
    return files;
}

bool isAudio (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}
} // namespace

void DrumEditor::dropSample (const juce::File& file, int y)
{
    auto* track = tracks.findTrack (trackId);
    const int row = (y - viewport.getY() + viewport.getViewPositionY()) / DrumGrid::rowHeight;

    if (track == nullptr || row < 0 || row >= static_cast<int> (rows.size()))
        return;

    if (const auto result = sampler.assignSample (*track, file, rows[static_cast<std::size_t> (row)].note); ! result)
        reportError (result.error());

    reload();
}

bool DrumEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return clipId.isNotEmpty() && files.size() == 1 && isAudio (files[0]);
}

void DrumEditor::filesDropped (const juce::StringArray& files, int, int y)
{
    if (! files.isEmpty())
        dropSample (juce::File (files[0]), y);
}

bool DrumEditor::isInterestedInDragSource (const SourceDetails& details)
{
    return isInterestedInFileDrag (filesFromMediaDrag (details.description));
}

void DrumEditor::itemDropped (const SourceDetails& details)
{
    const auto files = filesFromMediaDrag (details.description);

    if (! files.isEmpty())
        dropSample (juce::File (files[0]), details.localPosition.y);
}

void DrumEditor::midiClipChanged()
{
    reload();
}

void DrumEditor::timerCallback()
{
    if (clipId.isEmpty() || ! isShowing())
        return;

    const double beats = timeline.secondsToBeats (transport.getPositionSeconds()) - clipStartBeats;
    grid.setPlayheadBeats (beats >= 0.0 && beats <= clipLengthBeats ? std::optional (beats) : std::nullopt);
}

void DrumEditor::reportError (const core::Error& error)
{
    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
