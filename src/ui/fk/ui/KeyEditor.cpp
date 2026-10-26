#include "fk/ui/KeyEditor.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <array>
#include <cmath>

namespace fk::ui
{

namespace
{
constexpr int keysWidth = 56;
constexpr int toolbarHeight = 30;
constexpr int linkedLengthItemId = 1;  // Länge "wie Raster"
constexpr int noteValueItemOffset = 2; // ComboBox-IDs > 0; Notenwert i hat ID i + Offset

juce::String noteName (int noteNumber)
{
    return juce::MidiMessage::getMidiNoteName (noteNumber, true, true, 4); // wissenschaftlich: mittleres C = C4
}
} // namespace

//==============================================================================
PianoKeys::PianoKeys()
{
    setTooltip (TRANS ("Click a key to audition it, drag across the keys to play each one"));
}

int PianoKeys::noteNumberAt (int y) const
{
    return juce::jlimit (0, NoteGrid::numNotes - 1, NoteGrid::numNotes - 1 - y / NoteGrid::rowHeight);
}

void PianoKeys::mouseDown (const juce::MouseEvent& event)
{
    pressedNote = noteNumberAt (event.y);

    if (onKeyDown)
        onKeyDown (pressedNote);
}

void PianoKeys::mouseDrag (const juce::MouseEvent& event)
{
    // Gleiten über die Tasten: jede neu erreichte Taste anschlagen.
    if (const int note = noteNumberAt (event.y); note != pressedNote)
    {
        pressedNote = note;

        if (onKeyDown)
            onKeyDown (note);
    }
}

void PianoKeys::mouseUp (const juce::MouseEvent&)
{
    pressedNote = -1;

    if (onKeysUp)
        onKeysUp();
}

void PianoKeys::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    for (int note = 0; note < NoteGrid::numNotes; ++note)
    {
        const int y = (NoteGrid::numNotes - 1 - note) * NoteGrid::rowHeight;
        const bool black = NoteGridBackground::isBlackKey (note);

        g.setColour (black ? theme::backgroundBase : theme::textPrimary.withAlpha (0.85f));
        g.fillRect (0, y, getWidth() - 1, NoteGrid::rowHeight - 1);

        if (note % 12 == 0)
        {
            g.setColour (theme::backgroundBase);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (noteName (note), 2, y, getWidth() - 4, NoteGrid::rowHeight, juce::Justification::centredLeft);
        }
    }
}

//==============================================================================
KeyEditor::KeyEditor (Services services)
    : midiClips (services.midiClips)
    , midiControllers (services.midiControllers)
    , preview (services.preview)
    , transport (services.transport)
    , timeline (services.timeline)
    , signature (services.signature)
    , selectToolButton (
          Icon::toolSelect,
          TRANS ("Select: pick notes (drag a rectangle), move them, trim at the edges"))
    , drawToolButton (Icon::toolDraw,
                      TRANS ("Pencil: click places a note at the chosen length, dragging extends it"))
    , eraseToolButton (Icon::toolErase, TRANS ("Eraser: click deletes the note (or the selection)"))
    , quantiseButton (Icon::snap,
                      TRANS ("Quantize: snap note starts of the selection to the grid (all notes without a selection)"),
                      TRANS ("Quantize"))
    , functionsButton (Icon::functions,
                       TRANS ("MIDI functions on the selection (all notes without a selection): length, legato, transpose, humanize, velocity, delete doubles"),
                       TRANS ("Functions"))
    , quantiseOptionsButton (Icon::settings,
                             TRANS ("Quantize options: strength, swing, groove from another clip"))
    , stepInputButton (Icon::keyboard,
                       TRANS ("Step input: each key places a note at the green cursor at the chosen length, keys held together make a chord"),
                       TRANS ("Step"))
    , stepBackButton (Icon::nudgeLeft, TRANS ("Step cursor back one step"))
    , stepPauseButton (Icon::nudgeRight, TRANS ("Rest: step cursor forward one step, without a note"))
    , chordButton (Icon::plus,
                   TRANS ("Insert the chord of the chosen degree at the end of the clip; Alt+click in the grid places it at the mouse position"),
                   TRANS ("Chord"))
    , addLaneButton (
          Icon::plus,
          TRANS ("Add controller lane (sustain, mod wheel, pitch bend, aftertouch, CC)"),
          TRANS ("Lane"))
    , previewButton (
          Icon::speaker,
          TRANS ("Audition: keys and placed or dragged notes sound through the instrument"))
    , grid (services.midiClips, services.timeline)
    , velocityLane (services.midiClips)
{
    viewport.onScrolled = [this] (int x)
    {
        velocityLane.setScrollX (x);
        articulationLane.setScrollX (x);

        for (auto& lane : controllerLanes)
            lane->setScrollX (x);
    };
    addAndMakeVisible (velocityLane);
    addLaneButton.onClick = [this]
    {
        // Erste Lane Sustain, dann Mod-Wheel, dann Pitch-Bend: die häufigsten zuerst.
        static constexpr int order[] = { services::controllerTypes::sustain,
                                         services::controllerTypes::modWheel,
                                         services::controllerTypes::pitchBend,
                                         services::controllerTypes::aftertouch };
        addControllerLane (order[juce::jmin (controllerLanes.size(), std::size (order) - 1)]);
    };
    addAndMakeVisible (addLaneButton);
    keyLabel.setFont (juce::FontOptions (12.0f));
    keyLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    keyLabel.setTooltip (
        TRANS ("Key at the clip start (signature track); rows outside the scale are dimmed"));

    chordKindBox.addItem (TRANS ("Triad"), 1);
    chordKindBox.addItem (TRANS ("Seventh chord"), 2);
    chordKindBox.setSelectedId (1, juce::dontSendNotification);
    chordKindBox.setTooltip (TRANS ("Chord type"));
    inversionBox.addItem (TRANS ("Root position"), 1);
    inversionBox.addItem (TRANS ("1st inversion"), 2);
    inversionBox.addItem (TRANS ("2nd inversion"), 3);
    inversionBox.setSelectedId (1, juce::dontSendNotification);
    inversionBox.setTooltip (TRANS ("Inversion"));
    degreeBox.setTooltip (
        TRANS ("Degree relative to the key (I-VII) with the resulting chord"));

    for (auto* box : { &degreeBox, &chordKindBox, &inversionBox })
    {
        box->onChange = [this] { grid.setChordChoice (currentChordChoice()); };
        addAndMakeVisible (box);
    }

    chordButton.onClick = [this] { insertChord(); };
    addAndMakeVisible (chordButton);
    addAndMakeVisible (keyLabel);
    signature.addListener (*this);
    quantiseButton.setTooltip (TRANS ("Quantize: snap all note starts to the grid"));
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme::textSecondary);

    for (auto* button : { &selectToolButton, &drawToolButton, &eraseToolButton })
    {
        button->setRadioGroupId (2);
        button->setClickingTogglesState (true);
        addAndMakeVisible (button);
    }

    selectToolButton.onClick = [this] { selectTool (Tool::select); };
    drawToolButton.onClick = [this] { selectTool (Tool::draw); };
    eraseToolButton.onClick = [this] { selectTool (Tool::erase); };
    drawToolButton.setToggleState (true, juce::dontSendNotification);

    // Raster und Länge wie in Cubase: 1/1 bis 1/128, gerade, punktiert, triolisch.
    fillNoteValueBox (gridBox, false);
    fillNoteValueBox (lengthBox, true);
    gridBox.setTooltip (TRANS ("Grid: snapping when placing and dragging, target when quantizing"));
    lengthBox.setTooltip (TRANS ("Length of new notes (\"Grid\" = as long as one grid step)"));
    gridBox.onChange = [this]
    {
        grid.setGridBeats (currentGridBeats());
        grid.setNoteLengthBeats (currentNoteLengthBeats());

        for (auto& lane : controllerLanes)
            lane->setGridBeats (currentGridBeats());
    };
    lengthBox.onChange = [this]
    {
        grid.setNoteLengthBeats (currentNoteLengthBeats());

        if (stepInput != nullptr)
            stepInput->setStepBeats (currentNoteLengthBeats());
    };
    addAndMakeVisible (lengthBox);

    grid.onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };
    velocityLane.onError = grid.onError;

    // Vorhören wie Cubase: Klaviatur und Notenraster spielen die Note über das Spurinstrument.
    previewButton.setClickingTogglesState (true);
    previewButton.setToggleState (true, juce::dontSendNotification);
    previewButton.setOnColour (theme::accent);
    previewButton.onClick = [this]
    {
        if (! previewButton.getToggleState())
            preview.stopAll();
    };
    addAndMakeVisible (previewButton);
    keys.onKeyDown = [this] (int noteNumber)
    {
        preview.stopAll();
        previewNote (noteNumber, 100);
    };
    keys.onKeysUp = [this] { preview.stopAll(); };
    grid.onPreviewNote = [this] (int noteNumber, int velocity)
    {
        preview.stopAll();
        previewNote (noteNumber, velocity);
    };
    grid.onPreviewStop = [this] { preview.stopAll(); };
    grid.onSelectionChanged = [this]
    {
        velocityLane.setSelectedNotes (grid.getSelectedNotes());
        articulationLane.setSelectedNotes (grid.getSelectedNotes());
    };

    // Spielweisen: Map der Spur wählen, Lane setzt die Spielweise je Note.
    expressionMaps = services.expressionMaps;
    expressionMapBox.setTooltip (TRANS ("Expression map of the track: which articulations exist and how they reach the instrument (channel, keyswitch, CC)"));
    expressionMapBox.onChange = [this]
    {
        const auto id = expressionMapBox.getSelectedId();
        const auto mapId = id > 1 ? mapChoices[static_cast<std::size_t> (id - 2)].id : juce::String();

        if (expressionMaps != nullptr && clipTrackId.isNotEmpty())
            if (const auto result = expressionMaps->setMapForTrack (clipTrackId, mapId); ! result)
                reportError (result.error());

        refreshExpressionMap();
        resized();
    };
    addChildComponent (expressionMapBox);
    articulationLane.setHeaderWidth (keysWidth);
    articulationLane.onSet = [this] (const std::vector<int>& notes, const juce::String& articulationId)
    {
        if (const auto result = midiClips.setArticulation (clipId, notes, articulationId); ! result)
            reportError (result.error());
    };
    addChildComponent (articulationLane);

    quantiseButton.onClick = [this]
    {
        quantiseSettings.gridBeats = currentGridBeats();

        if (clipId.isNotEmpty())
            if (const auto result = midiClips.quantise (clipId, quantiseSettings, grid.getSelectedNotes()); ! result)
                reportError (result.error());
    };
    content.addAndMakeVisible (keys);
    content.addAndMakeVisible (grid);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, true);

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (gridBox);
    addAndMakeVisible (quantiseButton);
    functionsButton.onClick = [this] { showFunctionsMenu(); };
    addAndMakeVisible (functionsButton);
    quantiseOptionsButton.onClick = [this] { showQuantiseOptions(); };
    addAndMakeVisible (quantiseOptionsButton);

    stepInput = services.stepInput;
    stepInputButton.setClickingTogglesState (false);
    stepInputButton.onClick = [this] { toggleStepInput(); };
    stepBackButton.onClick = [this]
    {
        if (stepInput != nullptr && stepInput->isActive())
            stepInput->advance (-1);
    };
    stepPauseButton.onClick = [this]
    {
        if (stepInput != nullptr && stepInput->isActive())
            stepInput->advance (1);
    };

    for (auto* button : { &stepInputButton, &stepBackButton, &stepPauseButton })
        addChildComponent (button);

    stepInputButton.setVisible (stepInput != nullptr);

    if (stepInput != nullptr)
        stepInput->onChanged = [this] { stepInputChanged(); };
    addAndMakeVisible (viewport);

    grid.setGridBeats (currentGridBeats());
    grid.setNoteLengthBeats (currentNoteLengthBeats());
    midiClips.addListener (*this);
    startTimerHz (30);
    reload();
}

KeyEditor::~KeyEditor()
{
    signature.removeListener (*this);
    midiClips.removeListener (*this);
}

void KeyEditor::setClip (const juce::String& newClipId)
{
    // Anderer Clip: eine laufende Step-Eingabe gehört zum alten und endet.
    if (stepInput != nullptr && stepInput->isActive() && stepInput->getClipId() != newClipId)
        stepInput->stop();

    clipId = newClipId;
    reload();
    stepInputChanged();

    // Beim Öffnen auf C3 scrollen, dort liegt das meiste.
    viewport.setViewPosition (0, (NoteGrid::numNotes - 1 - 72) * NoteGrid::rowHeight);
}

juce::String KeyEditor::getClipId() const
{
    return clipId;
}

void KeyEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);

    if (clipId.isEmpty())
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (
            TRANS ("Key Editor\n\nDouble-click a MIDI clip in the arrangement to open it here. Create an empty MIDI clip by double-clicking in an instrument track."),
            getLocalBounds().reduced (theme::spacingLarge),
            juce::Justification::centred,
            6);
    }
}

void KeyEditor::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (toolbarHeight).reduced (theme::spacingMedium, theme::spacingSmall);

    titleLabel.setBounds (toolbar.removeFromLeft (200));
    toolbar.removeFromLeft (theme::spacingMedium);

    for (auto* button : { &selectToolButton, &drawToolButton, &eraseToolButton, &previewButton })
    {
        button->setBounds (toolbar.removeFromLeft (28));
        toolbar.removeFromLeft (theme::spacingSmall);
    }

    toolbar.removeFromLeft (theme::spacingMedium);
    gridBox.setBounds (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (theme::spacingSmall);
    lengthBox.setBounds (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (theme::spacingSmall);
    quantiseButton.setBounds (toolbar.removeFromLeft (120));
    quantiseOptionsButton.setBounds (toolbar.removeFromLeft (28));
    toolbar.removeFromLeft (theme::spacingSmall);
    functionsButton.setBounds (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (theme::spacingSmall);
    stepInputButton.setBounds (toolbar.removeFromLeft (70));
    stepBackButton.setBounds (toolbar.removeFromLeft (28));
    stepPauseButton.setBounds (toolbar.removeFromLeft (28));
    toolbar.removeFromLeft (theme::spacingSmall);
    expressionMapBox.setBounds (toolbar.removeFromLeft (190));
    toolbar.removeFromLeft (theme::spacingMedium);
    keyLabel.setBounds (toolbar.removeFromLeft (110));
    degreeBox.setBounds (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (theme::spacingSmall);
    chordKindBox.setBounds (toolbar.removeFromLeft (100));
    toolbar.removeFromLeft (theme::spacingSmall);
    inversionBox.setBounds (toolbar.removeFromLeft (110));
    toolbar.removeFromLeft (theme::spacingSmall);
    chordButton.setBounds (toolbar.removeFromLeft (90));

    // Controller-Lanes ganz unten (die zuletzt hinzugefügte unten), darüber die Velocity-Lane,
    // alle bündig mit dem Raster: Die Lane-Köpfe liegen über der Klaviaturbreite.
    for (auto it = controllerLanes.rbegin(); it != controllerLanes.rend(); ++it)
    {
        auto laneRow = area.removeFromBottom (ControllerLane::preferredHeight);
        (*it)->setBounds (laneRow);
        (*it)->setVisible (clipId.isNotEmpty());
        (*it)->setPixelsPerBeat (grid.getPixelsPerBeat());
        (*it)->setGridBeats (currentGridBeats());
    }

    // Spielweisen über der Velocity-Lane, nur mit Expression Map.
    articulationLane.setVisible (clipId.isNotEmpty() && articulationLane.hasMap());

    if (articulationLane.isVisible())
    {
        articulationLane.setBounds (area.removeFromBottom (articulationLane.getPreferredHeight()));
        articulationLane.setPixelsPerBeat (grid.getPixelsPerBeat());
        articulationLane.setScrollX (viewport.getViewPositionX());
    }

    // Die Ecke unter der Klaviatur trägt den Button für neue Controller-Lanes: immer sichtbar,
    // auch wenn die Werkzeugleiste in schmalen Fenstern rechts abgeschnitten ist.
    auto velocityRow = area.removeFromBottom (VelocityLane::preferredHeight);
    addLaneButton.setBounds (
        velocityRow.removeFromLeft (keysWidth).reduced (theme::spacingSmall).removeFromBottom (24));
    addLaneButton.setVisible (clipId.isNotEmpty());
    velocityLane.setBounds (velocityRow);
    velocityLane.setVisible (clipId.isNotEmpty());
    velocityLane.setPixelsPerBeat (grid.getPixelsPerBeat());

    viewport.setBounds (area);
    viewport.setVisible (clipId.isNotEmpty());

    const int contentHeight = NoteGrid::numNotes * NoteGrid::rowHeight;
    content.setSize (keysWidth + grid.getWidth(), contentHeight);
    keys.setBounds (0, 0, keysWidth, contentHeight);
    grid.setTopLeftPosition (keysWidth, 0);
}

void KeyEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCtrlDown())
    {
        grid.setPixelsPerBeat (grid.getPixelsPerBeat() * (wheel.deltaY > 0.0f ? 1.2 : 1.0 / 1.2));
        resized();
        return;
    }

    viewport.mouseWheelMove (event, wheel);
}

void KeyEditor::signatureChanged()
{
    reload();
}

void KeyEditor::rebuildChordChoices()
{
    const int selected = juce::jmax (1, degreeBox.getSelectedId());
    degreeBox.clear (juce::dontSendNotification);

    for (int degree = 1; degree <= 7; ++degree)
        degreeBox.addItem (services::SignatureService::getDegreeName (
                               currentKey.rootNote, currentKey.scale, degree, currentChordChoice().kind) +
                               "  " +
                               services::SignatureService::getChordSymbol (
                                   currentKey.rootNote, currentKey.scale, degree, currentChordChoice().kind),
                           degree);

    degreeBox.setSelectedId (selected, juce::dontSendNotification);
    grid.setChordChoice (currentChordChoice());
}

NoteGrid::ChordChoice KeyEditor::currentChordChoice() const
{
    return { juce::jmax (1, degreeBox.getSelectedId()),
             chordKindBox.getSelectedId() == 2 ? services::ChordKind::seventh : services::ChordKind::triad,
             juce::jmax (0, inversionBox.getSelectedId() - 1) };
}

void KeyEditor::insertChord()
{
    if (clipId.isEmpty())
        return;

    const auto clip = midiClips.getClip (clipId);

    if (! clip)
        return;

    // Hinter die letzte Note, auf den nächsten Taktanfang; leerer Clip: an den Anfang.
    const auto signatureNow = timeline.getTimeSignature();
    const double barBeats = 4.0 / signatureNow.denominator * signatureNow.numerator;
    double lastEnd = 0.0;

    for (const auto& note : clip->notes)
        lastEnd = juce::jmax (lastEnd, note.startBeats + note.lengthBeats);

    const double start = std::ceil (lastEnd / barBeats - 1.0e-9) * barBeats;
    const auto choice = currentChordChoice();
    const auto notes = services::SignatureService::getChordNotes (
        currentKey.rootNote, currentKey.scale, choice.degree, choice.kind, choice.inversion, 4);

    if (const auto result = midiClips.addChord (clipId, notes, start, barBeats, 100); ! result)
        reportError (result.error());
}

void KeyEditor::timerCallback()
{
    if (clipId.isEmpty() || ! isShowing())
        return;

    const double beats = timeline.secondsToBeats (transport.getPositionSeconds()) - clipStartBeats;
    grid.setPlayheadBeats (beats >= 0.0 && beats <= clipLengthBeats ? std::optional (beats) : std::nullopt);
}

void KeyEditor::midiClipChanged()
{
    reload();
}

void KeyEditor::reload()
{
    if (clipId.isEmpty())
    {
        grid.setClip (std::nullopt);
        velocityLane.setClip (std::nullopt);

        for (auto& lane : controllerLanes)
            lane->setClip (std::nullopt);

        titleLabel.setText ({}, juce::dontSendNotification);
        resized();
        return;
    }

    const auto clip = midiClips.getClip (clipId);

    if (! clip)
    {
        // Clip gelöscht (oder Projekt gewechselt): Editor leeren, nie auf den alten Clip zeigen.
        clipId.clear();
        reload();
        return;
    }

    const auto noteCount = static_cast<int> (clip->notes.size());
    titleLabel.setText (plugins::i18n::format (TRANS ("{name}  ({notes})"),
                                               { { "name", clip->name },
                                                 { "notes", plugins::i18n::plural (noteCount, "{count} note", "{count} notes") } }),
                        juce::dontSendNotification);
    clipStartBeats = timeline.secondsToBeats (clip->startSeconds);
    clipLengthBeats = clip->lengthBeats;
    grid.setClip (*clip);
    velocityLane.setClip (*clip);
    articulationLane.setClip (*clip);
    clipTrackId = clip->trackId;
    refreshExpressionMap();

    for (auto& lane : controllerLanes)
        lane->setClip (*clip);

    currentKey = signature.getKeyAt (clip->startSeconds);
    grid.setKey (currentKey);
    keyLabel.setText (currentKey.name, juce::dontSendNotification);
    rebuildChordChoices();
    resized();
    repaint();
}

void KeyEditor::addControllerLane (int controllerType)
{
    auto lane = std::make_unique<ControllerLane> (midiControllers);
    lane->setGridBeats (currentGridBeats());
    lane->onError = [this] (const juce::String& message)
    {
        if (onError)
            onError (message);
    };
    lane->setControllerType (controllerType);
    lane->onRemoveRequested = [this, raw = lane.get()] { removeControllerLane (raw); };
    lane->setScrollX (viewport.getViewPositionX() - 0); // gleiche Rollposition wie das Raster
    addAndMakeVisible (*lane);
    controllerLanes.push_back (std::move (lane));
    reload();
}

void KeyEditor::removeControllerLane (ControllerLane* lane)
{
    // Nicht aus dem onClick der Lane heraus zerstören: erst nach dem Aufruf (asynchron).
    juce::MessageManager::callAsync (
        [safe = juce::Component::SafePointer<KeyEditor> (this), lane]
        {
            if (safe == nullptr)
                return;

            std::erase_if (safe->controllerLanes,
                           [lane] (const std::unique_ptr<ControllerLane>& candidate)
                           { return candidate.get() == lane; });
            safe->resized();
        });
}

void KeyEditor::selectTool (Tool tool)
{
    grid.setTool (tool);
}

double KeyEditor::currentGridBeats() const
{
    const auto index = gridBox.getSelectedId() - noteValueItemOffset;
    return index >= 0 && index < static_cast<int> (noteValues.size())
               ? noteValues[static_cast<std::size_t> (index)].beats()
               : 0.25;
}

double KeyEditor::currentNoteLengthBeats() const
{
    const auto index = lengthBox.getSelectedId() - noteValueItemOffset;
    return index >= 0 && index < static_cast<int> (noteValues.size())
               ? noteValues[static_cast<std::size_t> (index)].beats()
               : currentGridBeats();
}

void KeyEditor::fillNoteValueBox (juce::ComboBox& box, bool withLinkItem)
{
    // Gerade Werte oben, punktierte und triolische als Untermenüs je Art: kurze Liste, alle Werte.
    if (withLinkItem)
        box.addItem (TRANS ("Grid"), linkedLengthItemId);

    juce::PopupMenu dotted;
    juce::PopupMenu triplets;

    for (std::size_t i = 0; i < noteValues.size(); ++i)
    {
        const auto& value = noteValues[i];
        const int itemId = static_cast<int> (i) + noteValueItemOffset;

        switch (value.feel)
        {
            case services::NoteValue::Feel::straight:
                box.addItem (value.name(), itemId);
                break;
            case services::NoteValue::Feel::dotted:
                dotted.addItem (itemId, value.name());
                break;
            case services::NoteValue::Feel::triplet:
                triplets.addItem (itemId, value.name());
                break;
        }
    }

    box.getRootMenu()->addSubMenu (TRANS ("Dotted"), dotted);
    box.getRootMenu()->addSubMenu (TRANS ("Triplet"), triplets);

    const auto sixteenth = std::find (noteValues.begin(), noteValues.end(), services::NoteValue { 16 });
    box.setSelectedId (withLinkItem ? linkedLengthItemId
                                    : static_cast<int> (sixteenth - noteValues.begin()) + noteValueItemOffset,
                       juce::dontSendNotification);
}

void KeyEditor::previewNote (int noteNumber, int velocity)
{
    if (previewButton.getToggleState() && clipId.isNotEmpty())
        preview.startNote (clipId, noteNumber, velocity);
}

void KeyEditor::showQuantiseOptions()
{
    enum
    {
        strengthBase = 100,
        swingBase = 200,
        noGroove = 300,
        grooveBase = 400
    };

    constexpr std::array<int, 4> strengths { 100, 75, 50, 25 };
    constexpr std::array<int, 5> swings { 0, 25, 50, 75, 100 };
    const auto clips = midiClips.listClips();

    juce::PopupMenu menu;
    menu.addSectionHeader (TRANS ("Strength"));

    for (std::size_t i = 0; i < strengths.size(); ++i)
        menu.addItem (strengthBase + static_cast<int> (i), juce::String (strengths[i]) + " %", true,
                      juce::roundToInt (quantiseSettings.strength * 100.0) == strengths[i]);

    menu.addSectionHeader (TRANS ("Swing"));

    for (std::size_t i = 0; i < swings.size(); ++i)
        menu.addItem (swingBase + static_cast<int> (i), juce::String (swings[i]) + " %", true,
                      juce::roundToInt (quantiseSettings.swing * 100.0) == swings[i]);

    juce::PopupMenu grooves;
    grooves.addItem (noGroove, TRANS ("No groove"), true, grooveName.isEmpty());

    for (std::size_t i = 0; i < clips.size(); ++i)
        if (clips[i].first != clipId)
            grooves.addItem (grooveBase + static_cast<int> (i), clips[i].second, true, grooveName == clips[i].second);

    menu.addSubMenu (grooveName.isEmpty() ? TRANS ("Groove from clip")
                                                        : plugins::i18n::format (TRANS ("Groove: {name}"), { { "name", grooveName } }), grooves);

    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetComponent (&quantiseOptionsButton),
        [safeThis = juce::Component::SafePointer<KeyEditor> (this), clips, strengths, swings] (int chosen)
        {
            if (safeThis == nullptr || chosen <= 0)
                return;

            auto& settings = safeThis->quantiseSettings;

            if (chosen >= grooveBase)
            {
                // Groove eines Takts im aktuellen Raster aus dem gewählten Clip.
                const auto& source = clips[static_cast<std::size_t> (chosen - grooveBase)];
                const auto gridBeats = safeThis->currentGridBeats();
                const auto steps = std::max (1, juce::roundToInt (4.0 / gridBeats));

                if (const auto groove = safeThis->midiClips.extractGroove (source.first, gridBeats, steps))
                {
                    settings.groove = *groove;
                    safeThis->grooveName = source.second;
                }
                else
                {
                    safeThis->reportError (groove.error());
                }
            }
            else if (chosen == noGroove)
            {
                settings.groove.clear();
                safeThis->grooveName.clear();
            }
            else if (chosen >= swingBase)
            {
                settings.swing = swings[static_cast<std::size_t> (chosen - swingBase)] / 100.0;
            }
            else if (chosen >= strengthBase)
            {
                settings.strength = strengths[static_cast<std::size_t> (chosen - strengthBase)] / 100.0;
            }
        });
}

void KeyEditor::toggleStepInput()
{
    if (stepInput == nullptr || clipId.isEmpty())
        return;

    if (stepInput->isActive())
    {
        stepInput->stop();
        return;
    }

    // Start am Positionszeiger, wenn er im Clip steht, sonst am Clip-Anfang; auf das Raster gerundet.
    const double playhead = timeline.secondsToBeats (transport.getPositionSeconds()) - clipStartBeats;
    const double gridBeats = currentGridBeats();
    const double start =
        playhead >= 0.0 && playhead < clipLengthBeats ? std::round (playhead / gridBeats) * gridBeats : 0.0;

    if (const auto result = stepInput->start (clipId, start, currentNoteLengthBeats()); ! result)
        reportError (result.error());
}

void KeyEditor::refreshExpressionMap()
{
    expressionMapBox.setVisible (expressionMaps != nullptr && clipId.isNotEmpty());

    if (expressionMaps == nullptr)
        return;

    const auto current = expressionMaps->getMapForTrack (clipTrackId);
    mapChoices = expressionMaps->getMaps();
    expressionMapBox.clear (juce::dontSendNotification);
    expressionMapBox.addItem (TRANS ("No articulations"), 1);

    for (std::size_t i = 0; i < mapChoices.size(); ++i)
        expressionMapBox.addItem (mapChoices[i].name, static_cast<int> (i) + 2);

    expressionMapBox.setSelectedId (1, juce::dontSendNotification);

    // Die Spur trägt eine Kopie; gewählt wird der Eintrag mit derselben Kennung.
    if (current)
        for (std::size_t i = 0; i < mapChoices.size(); ++i)
            if (mapChoices[i].id == current->id)
                expressionMapBox.setSelectedId (static_cast<int> (i) + 2, juce::dontSendNotification);

    std::map<juce::String, juce::String> symbols;

    if (current)
        for (const auto& articulation : current->articulations)
            symbols[articulation.id] = articulation.symbol;

    grid.setArticulationSymbols (symbols);
    articulationLane.setMap (current);
}

void KeyEditor::stepInputChanged()
{
    const bool active = stepInput != nullptr && stepInput->isActive() && stepInput->getClipId() == clipId;
    stepInputButton.setToggleState (active, juce::dontSendNotification);
    stepBackButton.setVisible (active);
    stepPauseButton.setVisible (active);
    grid.setStepCursorBeats (active ? std::optional (stepInput->getPositionBeats()) : std::nullopt);
}

void KeyEditor::showFunctionsMenu()
{
    if (clipId.isEmpty())
        return;

    using services::MidiFunction;
    std::vector<services::MidiFunctionParams> actions;
    const auto add = [&actions] (juce::PopupMenu& menu, const juce::String& text, services::MidiFunctionParams params)
    {
        actions.push_back (params);
        menu.addItem (static_cast<int> (actions.size()), text);
    };

    juce::PopupMenu menu;
    const auto selected = grid.getSelectedNotes();
    const auto selectedCount = static_cast<int> (selected.size());
    menu.addSectionHeader (selected.empty()
                               ? TRANS ("All notes in the clip")
                               : plugins::i18n::plural (selectedCount, "{count} selected note", "{count} selected notes"));

    juce::PopupMenu transpose;
    for (const int semitones : { 12, 7, 5, 1, -1, -5, -7, -12 })
    {
        const auto value = (semitones > 0 ? "+" : "") + juce::String (semitones);
        add (transpose,
             std::abs (semitones) == 12 ? plugins::i18n::format (TRANS ("{value} (octave)"), { { "value", value } }) : value,
             { .function = MidiFunction::transpose, .semitones = semitones });
    }

    menu.addSubMenu (TRANS ("Transpose"), transpose);

    juce::PopupMenu length;
    add (length, TRANS ("Length = grid"), { .function = MidiFunction::fixedLength, .beats = currentGridBeats() });
    add (length, TRANS ("Length = note length"), { .function = MidiFunction::fixedLength, .beats = currentNoteLengthBeats() });
    add (length, TRANS ("Legato (up to the next note)"), { .function = MidiFunction::legato });
    menu.addSubMenu (TRANS ("Length"), length);

    juce::PopupMenu humanize;
    add (humanize, TRANS ("Timing subtle (+/-1/64)"), { .function = MidiFunction::humanizeTiming, .amount = 1.0 / 16.0, .randomSeed = juce::Random::getSystemRandom().nextInt() });
    add (humanize, TRANS ("Timing strong (+/-1/32)"), { .function = MidiFunction::humanizeTiming, .amount = 1.0 / 8.0, .randomSeed = juce::Random::getSystemRandom().nextInt() });
    add (humanize, TRANS ("Velocity subtle (+/-8)"), { .function = MidiFunction::humanizeVelocity, .amount = 8.0, .randomSeed = juce::Random::getSystemRandom().nextInt() });
    add (humanize, TRANS ("Velocity strong (+/-20)"), { .function = MidiFunction::humanizeVelocity, .amount = 20.0, .randomSeed = juce::Random::getSystemRandom().nextInt() });
    menu.addSubMenu (TRANS ("Humanize"), humanize);

    juce::PopupMenu velocity;
    for (const int value : { 127, 100, 80, 64, 40 })
        add (velocity, plugins::i18n::format (TRANS ("Fixed {value}"), { { "value", juce::String (value) } }), { .function = MidiFunction::fixedVelocity, .velocity = value });
    add (velocity, TRANS ("Louder (+10 %)"), { .function = MidiFunction::scaleVelocity, .amount = 1.1 });
    add (velocity, TRANS ("Softer (-10 %)"), { .function = MidiFunction::scaleVelocity, .amount = 0.9 });
    menu.addSubMenu (TRANS ("Velocity"), velocity);

    menu.addSeparator();
    add (menu, TRANS ("Delete duplicate notes"), { .function = MidiFunction::deleteDoubles });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&functionsButton),
                        [safeThis = juce::Component::SafePointer<KeyEditor> (this), actions, selected] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            if (const auto result = safeThis->midiClips.applyFunction (
                                    safeThis->clipId, actions[static_cast<std::size_t> (chosen - 1)], selected);
                                ! result)
                                safeThis->reportError (result.error());
                        });
}

void KeyEditor::reportError (const core::Error& error)
{
    juce::Logger::writeToLog ("Key Editor: " + services::errorText (error));

    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
