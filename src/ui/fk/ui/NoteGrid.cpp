#include "fk/ui/NoteGrid.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <cmath>
#include <utility>

namespace fk::ui
{

namespace
{
constexpr int edgeGrabPixels = 6;
constexpr int defaultVelocity = 100;

} // namespace

NoteGrid::NoteGrid (services::MidiClipService& midiClipService, services::TimelineService& timelineService)
    : timeline (timelineService)
    , editing (midiClipService)
{
    editing.onSelectionChanged = [this]
    {
        repaint();

        if (onSelectionChanged)
            onSelectionChanged();
    };
    editing.onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };
    addAndMakeVisible (background);
    setWantsKeyboardFocus (true);
    setTooltip (
        TRANS ("Click selects, Ctrl/Shift extends, dragging moves (Alt: copies), edges trim, Ctrl+Shift+drag changes the velocity. Pencil: click places, dragging extends. Del deletes, arrows transpose and move."));
    updateSize();
}

void NoteGrid::setClip (std::optional<services::MidiClipInfo> newClip)
{
    drag = {};
    editing.setClip (std::move (newClip));
    updateSize();
    updateBackground();
    repaint();
}

void NoteGrid::setKey (services::KeyPointInfo newKey)
{
    key = newKey;
    background.setKey (newKey);
}

void NoteGrid::setChordChoice (ChordChoice choice)
{
    chordChoice = choice;
}

void NoteGrid::setTool (Tool newTool)
{
    tool = newTool;
}

void NoteGrid::setGridBeats (double beats)
{
    gridBeats = beats;
    updateBackground();
}

void NoteGrid::setNoteLengthBeats (double lengthBeats)
{
    noteLengthBeats = lengthBeats;
}

void NoteGrid::setPixelsPerBeat (double pixels)
{
    pixelsPerBeat = juce::jlimit (8.0, 400.0, pixels);
    updateSize();
    updateBackground();
    repaint();
}

double NoteGrid::getPixelsPerBeat() const noexcept
{
    return pixelsPerBeat;
}

std::vector<int> NoteGrid::getSelectedNotes() const
{
    return { editing.getSelection().begin(), editing.getSelection().end() };
}

void NoteGrid::selectAll()
{
    editing.selectAll();
}

void NoteGrid::setArticulationSymbols (std::map<juce::String, juce::String> symbols)
{
    articulationSymbols = std::move (symbols);
    repaint();
}

void NoteGrid::setStepCursorBeats (std::optional<double> beats)
{
    if (beats != stepCursorBeats)
    {
        stepCursorBeats = beats;
        repaint();
    }
}

void NoteGrid::setPlayheadBeats (std::optional<double> beats)
{
    // Nur die beiden Streifen alter und neuer Position neu zeichnen.
    const auto stripFor = [this] (std::optional<double> at)
    {
        return at.has_value()
                   ? juce::Rectangle<int> (static_cast<int> (std::round (*at * pixelsPerBeat)) - 2, 0, 5, getHeight())
                   : juce::Rectangle<int>();
    };

    if (beats == playheadBeats)
        return;

    repaint (stripFor (playheadBeats));
    playheadBeats = beats;
    repaint (stripFor (playheadBeats));
}

void NoteGrid::resized()
{
    background.setBounds (getLocalBounds());
}

void NoteGrid::paintOverChildren (juce::Graphics& g)
{
    // Noten über dem Hintergrund (Kind-Komponente); beim Ziehen ändert sich nur diese Lage.
    if (! editing.getClip().has_value())
        return;

    paintNotes (g);

    if (playheadBeats.has_value())
    {
        g.setColour (theme::accent);
        g.drawVerticalLine (
            static_cast<int> (std::round (*playheadBeats * pixelsPerBeat)), 0.0f, static_cast<float> (getHeight()));
    }

    // Step-Cursor: breiter Streifen in der Solo-Farbe, damit er sich vom Positionszeiger abhebt.
    if (stepCursorBeats.has_value())
    {
        const auto x = static_cast<float> (std::round (*stepCursorBeats * pixelsPerBeat));
        g.setColour (theme::solo.withAlpha (0.25f));
        g.fillRect (x, 0.0f, 6.0f, static_cast<float> (getHeight()));
        g.setColour (theme::solo);
        g.drawVerticalLine (static_cast<int> (x), 0.0f, static_cast<float> (getHeight()));
    }

    if (drag.getMode() == NoteDrag::Mode::lasso)
    {
        g.setColour (theme::accent.withAlpha (0.15f));
        g.fillRect (drag.getLasso());
        g.setColour (theme::accent);
        g.drawRect (drag.getLasso());
    }
}

void NoteGrid::updateBackground()
{
    const auto signature = timeline.getTimeSignature();
    const auto& clip = editing.getClip();
    background.setLayout ({ rowHeight,
                            numNotes,
                            gridBeats,
                            pixelsPerBeat,
                            4.0 / signature.denominator * signature.numerator,
                            clip.has_value() ? clip->lengthBeats : 0.0 });
}

void NoteGrid::paintNotes (juce::Graphics& g) const
{
    const auto paintNote = [&g, this] (const services::EditableNote& note, bool isSelected, float opacity)
    {
        const auto rect = rectForNote (note).toFloat().reduced (0.5f);
        const float alpha = (0.45f + 0.55f * static_cast<float> (note.velocity) / 127.0f) * opacity;
        g.setColour ((isSelected ? theme::accent.brighter (0.35f) : theme::accent).withAlpha (alpha));
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (isSelected ? theme::textPrimary : theme::textPrimary.withAlpha (0.5f * opacity));
        g.drawRoundedRectangle (rect, 2.0f, isSelected ? 1.5f : 1.0f);

        // Spielweise als Zeichen am Balken (Staccato-Punkt, Akzent, "pizz" ...).
        if (note.articulation.isNotEmpty() && rect.getWidth() > 8.0f)
        {
            const auto found = articulationSymbols.find (note.articulation);
            const auto symbol = found != articulationSymbols.end() && found->second.isNotEmpty() ? found->second
                                                                                                 : note.articulation.substring (0, 3);
            g.setColour (theme::textPrimary.withAlpha (opacity));
            g.setFont (juce::FontOptions (std::min (rect.getHeight() - 1.0f, 11.0f), juce::Font::bold));
            g.drawText (symbol, rect.reduced (2.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    };

    const auto mode = drag.getMode();
    const bool showsPreview = mode == NoteDrag::Mode::move || mode == NoteDrag::Mode::trimStart ||
                              mode == NoteDrag::Mode::trimEnd || mode == NoteDrag::Mode::velocity;
    const auto& previews = drag.getPreviews();

    for (const auto& note : editing.getClip()->notes)
    {
        // Beim Verschieben steht die Note an ihrer Vorschau-Stelle; beim Kopieren bleibt das Original.
        if (showsPreview && ! drag.isCopy() && previews.contains (note.index))
            continue;

        paintNote (note, editing.isSelected (note.index), 1.0f);
    }

    if (showsPreview)
        for (const auto& [index, note] : previews)
            paintNote (note, true, drag.isCopy() ? 0.7f : 1.0f);

    if (mode == NoteDrag::Mode::draw)
        paintNote (drag.getDrawn(), true, 0.8f);
}

void NoteGrid::mouseDown (const juce::MouseEvent& event)
{
    if (! editing.getClip().has_value())
        return;

    grabKeyboardFocus();
    const auto position = event.getPosition();
    const auto* note = findNoteAt (position);

    if (tool == Tool::erase)
    {
        if (note != nullptr)
            editing.isSelected (note->index) ? editing.removeSelection() : editing.removeNotes ({ note->index });

        return;
    }

    if (note == nullptr)
    {
        if (tool == Tool::draw)
        {
            if (event.mods.isAltDown())
                addChordAt (position);
            else
            {
                drag.beginDraw (
                    { -1, noteNumberAt (position.y), floorToGrid (position.x), noteLengthBeats, defaultVelocity });
                previewNote (noteNumberAt (position.y), defaultVelocity);
            }

            repaint();
            return;
        }

        // Auswahlwerkzeug ins Leere: Auswahlrechteck; ohne Strg/Shift ersetzt es die Auswahl.
        if (! event.mods.isCtrlDown() && ! event.mods.isShiftDown())
            editing.setSelection ({});

        drag.beginLasso (position);
        return;
    }

    const int index = note->index;
    const bool velocityDrag = event.mods.isCtrlDown() && event.mods.isShiftDown();
    const bool wasSelected = editing.isSelected (index);

    if (velocityDrag)
    {
        if (! wasSelected)
            editing.selectOnly (index);
    }
    else if (event.mods.isCtrlDown())
    {
        editing.toggle (index);

        if (! editing.isSelected (index))
            return; // abgewählt: nichts zu ziehen
    }
    else if (event.mods.isShiftDown())
    {
        editing.add (index);
    }
    else if (! wasSelected)
    {
        editing.selectOnly (index);
    }

    previewNote (note->noteNumber, note->velocity);
    const auto mode = velocityDrag ? NoteDrag::Mode::velocity : modeAt (*note, position);
    drag.beginEdit (mode, editing.getSelectedNotes(), index, mode == NoteDrag::Mode::move && event.mods.isAltDown());

    // Klick ohne Ziehen in eine bestehende Mehrfachauswahl wählt nur diese Note (wie Cubase).
    drag.setSelectOnlyOnClick (wasSelected && ! event.mods.isAnyModifierKeyDown() && editing.getSelection().size() > 1);
}

void NoteGrid::mouseDrag (const juce::MouseEvent& event)
{
    if (drag.getMode() == NoteDrag::Mode::none)
        return;

    drag.update (event.getOffsetFromDragStart(), event.getPosition(), getScale());
    repaint();

    // Beim Verschieben in eine andere Zeile die neue Tonhöhe hören (wie Cubase).
    if (drag.getMode() == NoteDrag::Mode::move)
        if (const auto anchor = drag.getPreviews().find (drag.getAnchorIndex()); anchor != drag.getPreviews().end())
            if (anchor->second.noteNumber != previewedNote)
                previewNote (anchor->second.noteNumber, anchor->second.velocity);
}

NoteDrag::Scale NoteGrid::getScale() const noexcept
{
    return { gridBeats, pixelsPerBeat, rowHeight, numNotes };
}

void NoteGrid::mouseUp (const juce::MouseEvent&)
{
    previewedNote = -1;

    if (onPreviewStop)
        onPreviewStop();

    if (drag.getMode() != NoteDrag::Mode::none && editing.getClip().has_value())
        commitDrag();
}

void NoteGrid::commitDrag()
{
    const auto finished = std::exchange (drag, {});
    const auto mode = finished.getMode();
    repaint();

    if (mode == NoteDrag::Mode::lasso)
    {
        auto next = editing.getSelection();

        for (const auto& note : editing.getClip()->notes)
            if (finished.getLasso().intersects (rectForNote (note)))
                next.insert (note.index);

        editing.setSelection (std::move (next));
        return;
    }

    if (mode == NoteDrag::Mode::draw)
    {
        editing.addNotes ({ finished.getDrawn() }, TRANS ("Insert note"));
        return;
    }

    if (! finished.hasMoved())
    {
        if (finished.shouldSelectOnlyOnClick())
            editing.selectOnly (finished.getAnchorIndex());

        return;
    }

    std::vector<services::EditableNote> changed;

    for (const auto& [index, preview] : finished.getPreviews())
        changed.push_back (preview);

    if (finished.isCopy())
        editing.addNotes (changed, TRANS ("Copy notes"));
    else
        editing.updateNotes (changed,
                             mode == NoteDrag::Mode::velocity ? TRANS ("Change velocity")
                             : mode == NoteDrag::Mode::move   ? TRANS ("Move notes")
                                                              : TRANS ("Trim notes"));
}

void NoteGrid::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Nur mit dem Auswahlwerkzeug: Doppelklick ins Leere setzt eine Note, auf eine Note löscht
    // sie. Mit dem Stift setzt schon der erste Klick, ein Doppelklick würde die Note gleich löschen.
    if (! editing.getClip().has_value() || tool != Tool::select)
        return;

    const auto position = event.getPosition();

    if (const auto* note = findNoteAt (position))
        editing.removeNotes ({ note->index });
    else
        editing.addNotes (
            { { -1, noteNumberAt (position.y), floorToGrid (position.x), noteLengthBeats, defaultVelocity } },
            TRANS ("Insert note"));
}

void NoteGrid::mouseMove (const juce::MouseEvent& event)
{
    const auto* note = findNoteAt (event.getPosition());
    auto shape = juce::MouseCursor::NormalCursor;

    if (note != nullptr && tool != Tool::erase)
    {
        const auto mode = modeAt (*note, event.getPosition());
        shape = mode == NoteDrag::Mode::move ? juce::MouseCursor::DraggingHandCursor
                                             : juce::MouseCursor::LeftRightResizeCursor;
    }

    setMouseCursor (shape);
}

bool NoteGrid::keyPressed (const juce::KeyPress& keyPress)
{
    if (! editing.getClip().has_value())
        return false;

    if (keyPress == juce::KeyPress::deleteKey || keyPress == juce::KeyPress::backspaceKey)
    {
        editing.removeSelection();
        return true; // nie an die Clip-Auswahl im Arrangement weiterreichen
    }

    if (keyPress == juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0))
    {
        editing.selectAll();
        return true;
    }

    const bool up = keyPress.isKeyCode (juce::KeyPress::upKey);

    if (up || keyPress.isKeyCode (juce::KeyPress::downKey))
    {
        const int semitones = (up ? 1 : -1) * (keyPress.getModifiers().isShiftDown() ? 12 : 1);
        editing.changeSelection ([semitones] (services::EditableNote& note)
                                 { note.noteNumber = juce::jlimit (0, numNotes - 1, note.noteNumber + semitones); },
                                 TRANS ("Transpose"));
        return true;
    }

    const bool right = keyPress.isKeyCode (juce::KeyPress::rightKey);

    if (right || keyPress.isKeyCode (juce::KeyPress::leftKey))
    {
        const double shift = right ? gridBeats : -gridBeats;
        editing.changeSelection ([shift] (services::EditableNote& note)
                                 { note.startBeats = juce::jmax (0.0, note.startBeats + shift); },
                                 TRANS ("Move notes"));
        return true;
    }

    return false;
}

void NoteGrid::updateSize()
{
    const auto& clip = editing.getClip();
    const double lengthBeats = clip.has_value() ? clip->lengthBeats : 16.0;
    setSize (juce::jmax (200, static_cast<int> (std::ceil ((lengthBeats + 4.0) * pixelsPerBeat))),
             numNotes * rowHeight);
}

juce::Rectangle<int> NoteGrid::rectForNote (const services::EditableNote& note) const
{
    const int x = static_cast<int> (std::round (note.startBeats * pixelsPerBeat));
    const int width = juce::jmax (3, static_cast<int> (std::round (note.lengthBeats * pixelsPerBeat)));
    const int y = (numNotes - 1 - note.noteNumber) * rowHeight;
    return { x, y, width, rowHeight };
}

const services::EditableNote* NoteGrid::findNoteAt (juce::Point<int> position) const
{
    const auto& clip = editing.getClip();

    if (! clip.has_value())
        return nullptr;

    for (auto it = clip->notes.rbegin(); it != clip->notes.rend(); ++it)
        if (rectForNote (*it).contains (position))
            return &*it;

    return nullptr;
}

NoteDrag::Mode NoteGrid::modeAt (const services::EditableNote& note, juce::Point<int> position) const
{
    // Greifzonen an beiden Rändern, bei kurzen Noten höchstens ein Drittel der Breite.
    const auto rect = rectForNote (note);
    const int zone = juce::jmin (edgeGrabPixels, rect.getWidth() / 3);

    if (position.x < rect.getX() + zone)
        return NoteDrag::Mode::trimStart;

    if (position.x >= rect.getRight() - zone)
        return NoteDrag::Mode::trimEnd;

    return NoteDrag::Mode::move;
}

double NoteGrid::beatAt (int x) const
{
    return static_cast<double> (x) / pixelsPerBeat;
}

double NoteGrid::snappedBeat (double beat) const
{
    return std::round (beat / gridBeats) * gridBeats;
}

double NoteGrid::floorToGrid (int x) const
{
    return juce::jmax (0.0, std::floor (beatAt (x) / gridBeats) * gridBeats);
}

int NoteGrid::noteNumberAt (int y) const
{
    return juce::jlimit (0, numNotes - 1, numNotes - 1 - y / rowHeight);
}

void NoteGrid::addChordAt (juce::Point<int> position)
{
    const int octave = noteNumberAt (position.y) / 12 - 1; // C4 = 60 → Oktave 4
    const auto notes = services::SignatureService::getChordNotes (
        key.rootNote, key.scale, chordChoice.degree, chordChoice.kind, chordChoice.inversion, octave);
    const auto signatureNow = timeline.getTimeSignature();
    const double barBeats = 4.0 / signatureNow.denominator * signatureNow.numerator;
    editing.addChord (notes, snappedBeat (beatAt (position.x)), barBeats, defaultVelocity);
}

void NoteGrid::previewNote (int noteNumber, int velocity)
{
    previewedNote = noteNumber;

    if (onPreviewNote)
        onPreviewNote (noteNumber, velocity);
}

} // namespace fk::ui
