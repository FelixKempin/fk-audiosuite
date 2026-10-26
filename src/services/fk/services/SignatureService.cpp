#include "fk/services/SignatureService.h"
#include "fk/plugins/i18n/Text.h"

#include <cmath>

namespace fk::services
{

namespace
{
constexpr double minTempoBpm = 20.0;
constexpr double maxTempoBpm = 400.0;

// Tracktion: curve 0 = gerade Linie bis zum nächsten Punkt (Rampe), ±1 = Sprung, der hält.
constexpr float rampCurve = 0.0f;
constexpr float stepCurve = 1.0f;

te::Scale::ScaleType toTracktion (ScaleKind scale)
{
    switch (scale)
    {
        case ScaleKind::major:
            return te::Scale::major;
        case ScaleKind::minor:
            return te::Scale::minor;
        case ScaleKind::dorian:
            return te::Scale::dorian;
        case ScaleKind::phrygian:
            return te::Scale::phrygian;
        case ScaleKind::lydian:
            return te::Scale::lydian;
        case ScaleKind::mixolydian:
            return te::Scale::mixolydian;
        case ScaleKind::locrian:
            return te::Scale::locrian;
        case ScaleKind::harmonicMinor:
            return te::Scale::harmonicMinor;
        case ScaleKind::melodicMinor:
            return te::Scale::melodicMinor;
    }

    return te::Scale::major;
}

ScaleKind fromTracktion (te::Scale::ScaleType scale)
{
    switch (scale)
    {
        case te::Scale::major:
        case te::Scale::ionian:
            return ScaleKind::major;
        case te::Scale::minor:
        case te::Scale::aeolian:
            return ScaleKind::minor;
        case te::Scale::dorian:
            return ScaleKind::dorian;
        case te::Scale::phrygian:
            return ScaleKind::phrygian;
        case te::Scale::lydian:
            return ScaleKind::lydian;
        case te::Scale::mixolydian:
            return ScaleKind::mixolydian;
        case te::Scale::locrian:
            return ScaleKind::locrian;
        case te::Scale::harmonicMinor:
            return ScaleKind::harmonicMinor;
        case te::Scale::melodicMinor:
            return ScaleKind::melodicMinor;
    }

    return ScaleKind::major;
}

int normalisedPitchClass (int note)
{
    return ((note % 12) + 12) % 12;
}

/// Akkorde der Skala je Stufe (Index 0 = I): Dreiklänge oder Septakkorde aus Tracktions Scale.
juce::Array<te::Chord> chordsFor (ScaleKind scale, ChordKind kind)
{
    const te::Scale tracktionScale (toTracktion (scale));
    return kind == ChordKind::triad ? tracktionScale.getTriads() : tracktionScale.getSevenths();
}
} // namespace

SignatureService::SignatureService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

SignatureService::~SignatureService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

//==============================================================================
std::vector<TempoPointInfo> SignatureService::getTempoPoints() const
{
    std::vector<TempoPointInfo> points;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return points;

    for (int i = 0; i < edit->tempoSequence.getNumTempos(); ++i)
    {
        const auto* tempo = edit->tempoSequence.getTempo (i);
        points.push_back ({ i,
                            tempo->getStartBeat().inBeats(),
                            tempo->getStartTime().inSeconds(),
                            tempo->getBpm(),
                            std::abs (tempo->getCurve()) < 0.999f });
    }

    return points;
}

double SignatureService::getTempoAt (double seconds) const
{
    const auto* edit = projectService.getEdit();
    return edit != nullptr ? edit->tempoSequence.getBpmAt (te::TimePosition::fromSeconds (seconds)) : 120.0;
}

core::Result<int> SignatureService::insertTempoPoint (double seconds, double bpm, bool ramp)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Set tempo point"));
    auto tempo = edit->tempoSequence.insertTempo (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));

    if (tempo == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the tempo point.").toStdString());

    tempo->setBpm (juce::jlimit (minTempoBpm, maxTempoBpm, bpm));
    tempo->setCurve (ramp ? rampCurve : stepCurve);
    return edit->tempoSequence.indexOfTempo (tempo.get());
}

core::Result<void> SignatureService::setTempoPoint (int index, double bpm, bool ramp)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto* tempo = edit->tempoSequence.getTempo (index);

    if (tempo == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Tempo point does not exist.").toStdString());

    projectService.beginUndoStep (TRANS ("Change tempo"));
    tempo->setBpm (juce::jlimit (minTempoBpm, maxTempoBpm, bpm));
    tempo->setCurve (ramp ? rampCurve : stepCurve);
    return {};
}

core::Result<void> SignatureService::moveTempoPoint (int index, double seconds)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index <= 0 || index >= edit->tempoSequence.getNumTempos())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The first tempo point stays at the start.").toStdString());

    auto* tempo = edit->tempoSequence.getTempo (index);
    const auto targetBeat = edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
    projectService.beginUndoStep (TRANS ("Move tempo point"));
    edit->tempoSequence.moveTempoStart (index, targetBeat - tempo->getStartBeat(), false);
    return {};
}

core::Result<void> SignatureService::removeTempoPoint (int index)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index <= 0 || index >= edit->tempoSequence.getNumTempos())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The first tempo point cannot be removed.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove tempo point"));
    edit->tempoSequence.removeTempo (index, false);
    return {};
}

//==============================================================================
std::vector<TimeSignaturePointInfo> SignatureService::getTimeSignaturePoints() const
{
    std::vector<TimeSignaturePointInfo> points;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return points;

    for (int i = 0; i < edit->tempoSequence.getNumTimeSigs(); ++i)
    {
        const auto* timeSig = edit->tempoSequence.getTimeSig (i);
        const auto time = edit->tempoSequence.toTime (timeSig->getStartBeat());
        points.push_back ({ i,
                            timeSig->getStartBeat().inBeats(),
                            time.inSeconds(),
                            edit->tempoSequence.toBarsAndBeats (time).bars + 1,
                            { timeSig->numerator.get(), timeSig->denominator.get() } });
    }

    return points;
}

std::vector<MidiTempoChange> SignatureService::readMidiFileTempo (const juce::File& midiFile)
{
    std::vector<MidiTempoChange> changes;
    juce::MidiFile file;

    if (juce::FileInputStream in (midiFile); ! in.openedOk() || ! file.readFrom (in))
        return changes;

    const auto ticksPerQuarter = static_cast<double> (file.getTimeFormat());

    if (ticksPerQuarter <= 0.0) // SMPTE-Zeitbasis: kein Tempo im musikalischen Sinn
        return changes;

    juce::MidiMessageSequence events;
    file.findAllTempoEvents (events);

    if (events.getNumEvents() == 0)
        return changes;

    file.findAllTimeSigEvents (events);
    events.sort();

    // Jede Meldung ergibt einen Stand; mehrere an derselben Stelle werden zu einem zusammengefasst.
    MidiTempoChange current;

    for (const auto* event : events)
    {
        const auto& message = event->message;
        current.quarterNote = message.getTimeStamp() / ticksPerQuarter;

        if (message.isTempoMetaEvent())
            current.quarterBpm = 60.0 / message.getTempoSecondsPerQuarterNote();
        else
            message.getTimeSignatureInfo (current.signature.numerator, current.signature.denominator);

        if (! changes.empty() && std::abs (changes.back().quarterNote - current.quarterNote) < 1.0e-9)
            changes.back() = current;
        else
            changes.push_back (current);
    }

    return changes;
}

bool SignatureService::midiFileTempoDiffers (const juce::File& midiFile, double startSeconds) const
{
    const auto* edit = projectService.getEdit();
    const auto changes = readMidiFileTempo (midiFile);

    if (edit == nullptr || changes.empty())
        return false;

    if (changes.size() > 1)
        return true;

    const auto position = te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds));
    const auto& signature = edit->tempoSequence.getTimeSigAt (position);
    const bool bpmCountsQuarters = edit->engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();
    const double bpm = edit->tempoSequence.getTempoAt (position).getBpm();
    const double quarterBpm = bpmCountsQuarters ? bpm : bpm * 4.0 / signature.denominator.get();

    return std::abs (quarterBpm - changes.front().quarterBpm) > 0.01 ||
           signature.numerator.get() != changes.front().signature.numerator ||
           signature.denominator.get() != changes.front().signature.denominator;
}

core::Result<void> SignatureService::applyMidiFileTempo (const juce::File& midiFile, double startSeconds)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto changes = readMidiFileTempo (midiFile);

    if (changes.empty())
        return core::fail (core::ErrorCode::notFound, TRANS ("The MIDI file contains no tempo.").toStdString());

    projectService.beginUndoStep (TRANS ("Tempo from MIDI file"));
    auto& sequence = edit->tempoSequence;
    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds));
    const auto startBeat = sequence.toBeats (start).inBeats();
    const bool bpmCountsQuarters = edit->engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();

    // Bisherige Wechsel ab der Einfügestelle weg; der erste Punkt bleibt immer (Tracktion braucht ihn).
    for (int i = sequence.getNumTempos() - 1; i > 0; --i)
        if (sequence.getTempo (i)->getStartBeat().inBeats() >= startBeat - 1.0e-6)
            sequence.removeTempo (i, false);

    for (int i = sequence.getNumTimeSigs() - 1; i > 0; --i)
        if (sequence.getTimeSig (i)->getStartBeat().inBeats() >= startBeat - 1.0e-6)
            sequence.removeTimeSig (i);

    // Ein Tracktion-Schlag ist eine Note des Taktart-Nenners; die Datei zählt in Vierteln.
    double beat = startBeat;
    double previousQuarter = 0.0;
    int denominator = sequence.getTimeSigAt (start).denominator.get();

    for (const auto& change : changes)
    {
        beat += (change.quarterNote - previousQuarter) * denominator / 4.0;
        previousQuarter = change.quarterNote;
        const auto position = te::BeatPosition::fromBeats (beat);
        const bool atFirstPoint = beat < 1.0e-6;

        auto* signature = atFirstPoint ? sequence.getTimeSig (0) : nullptr;

        if (signature == nullptr && (sequence.getTimeSigAt (position).numerator.get() != change.signature.numerator ||
                                     sequence.getTimeSigAt (position).denominator.get() != change.signature.denominator))
            signature = sequence.insertTimeSig (position).get();

        if (signature != nullptr)
        {
            signature->numerator = change.signature.numerator;
            signature->denominator = change.signature.denominator;
        }

        denominator = change.signature.denominator;
        const double bpm = juce::jlimit (
            minTempoBpm, maxTempoBpm, bpmCountsQuarters ? change.quarterBpm : change.quarterBpm * denominator / 4.0);

        if (atFirstPoint)
            sequence.getTempo (0)->setBpm (bpm);
        else if (auto tempo = sequence.insertTempo (position, bpm, stepCurve))
            tempo->setBpm (bpm);
    }

    return {};
}

core::Result<int> SignatureService::insertTimeSignature (double seconds, TimeSignature signature)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (signature.numerator < 1 || signature.numerator > 32 || signature.denominator < 1 || signature.denominator > 32)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Time signature outside 1..32.").toStdString());

    // Auf den Taktanfang legen: Ein Taktartwechsel mitten im Takt ergäbe einen halben Takt.
    auto barsAndBeats = edit->tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
    barsAndBeats.beats = te::BeatDuration();
    const auto barStart = edit->tempoSequence.toBeats (barsAndBeats);

    projectService.beginUndoStep (TRANS ("Set time signature"));
    auto timeSig = edit->tempoSequence.insertTimeSig (barStart);

    if (timeSig == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the time signature.").toStdString());

    timeSig->numerator = signature.numerator;
    timeSig->denominator = signature.denominator;
    return edit->tempoSequence.indexOfTimeSig (timeSig.get());
}

core::Result<void> SignatureService::setTimeSignature (int index, TimeSignature signature)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto* timeSig = edit->tempoSequence.getTimeSig (index);

    if (timeSig == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Time signature change does not exist.").toStdString());

    if (signature.numerator < 1 || signature.numerator > 32 || signature.denominator < 1 || signature.denominator > 32)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Time signature outside 1..32.").toStdString());

    projectService.beginUndoStep (TRANS ("Change time signature"));
    timeSig->numerator = signature.numerator;
    timeSig->denominator = signature.denominator;
    return {};
}

core::Result<void> SignatureService::removeTimeSignature (int index)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index <= 0 || index >= edit->tempoSequence.getNumTimeSigs())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The first time signature cannot be removed.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove time signature change"));
    edit->tempoSequence.removeTimeSig (index);
    return {};
}

//==============================================================================
KeyPointInfo SignatureService::keyInfoFor (int index) const
{
    KeyPointInfo info;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return info;

    const auto* pitch = edit->pitchSequence.getPitch (index);

    if (pitch == nullptr)
        return info;

    info.index = index;
    info.beat = pitch->getStartBeatNumber().inBeats();
    info.seconds = edit->tempoSequence.toTime (pitch->getStartBeatNumber()).inSeconds();
    info.rootNote = normalisedPitchClass (pitch->getPitch());
    info.scale = fromTracktion (pitch->getScale());
    info.name = getKeyName (info.rootNote, info.scale);
    return info;
}

std::vector<KeyPointInfo> SignatureService::getKeyPoints() const
{
    std::vector<KeyPointInfo> points;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return points;

    for (int i = 0; i < edit->pitchSequence.getNumPitches(); ++i)
        points.push_back (keyInfoFor (i));

    return points;
}

KeyPointInfo SignatureService::getKeyAt (double seconds) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr || edit->pitchSequence.getNumPitches() == 0)
        return {};

    auto& pitch = edit->pitchSequence.getPitchAt (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
    return keyInfoFor (edit->pitchSequence.indexOfPitch (&pitch));
}

KeyPointInfo SignatureService::getProjectKey() const
{
    return keyInfoFor (0);
}

core::Result<int> SignatureService::insertKey (double seconds, int rootNote, ScaleKind scale)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    projectService.beginUndoStep (TRANS ("Set key"));
    const auto beat = edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds)));
    auto pitch = edit->pitchSequence.insertPitch (beat, normalisedPitchClass (rootNote));

    if (pitch == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the key.").toStdString());

    pitch->setScaleID (toTracktion (scale));
    return edit->pitchSequence.indexOfPitch (pitch.get());
}

core::Result<void> SignatureService::setKey (int index, int rootNote, ScaleKind scale)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto* pitch = edit->pitchSequence.getPitch (index);

    if (pitch == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Key change does not exist.").toStdString());

    projectService.beginUndoStep (TRANS ("Change key"));
    pitch->setPitch (normalisedPitchClass (rootNote));
    pitch->setScaleID (toTracktion (scale));
    return {};
}

core::Result<void> SignatureService::moveKey (int index, double seconds)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index <= 0 || index >= edit->pitchSequence.getNumPitches())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project key stays at the start.").toStdString());

    projectService.beginUndoStep (TRANS ("Move key change"));
    edit->pitchSequence.getPitch (index)->setStartBeat (
        edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds))));
    edit->pitchSequence.sortEvents();
    return {};
}

core::Result<void> SignatureService::removeKey (int index)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index <= 0 || index >= edit->pitchSequence.getNumPitches())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project key cannot be removed.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove key change"));
    edit->pitchSequence.getPitch (index)->removeFromEdit();
    return {};
}

//==============================================================================
juce::String SignatureService::getNoteName (int rootNote)
{
    // Nur B/H unterscheidet sich je Sprache (englisch Bb und B, deutsch B und H).
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", NEEDS_TRANS ("Bb"), NEEDS_TRANS ("B") };
    return juce::translate (names[normalisedPitchClass (rootNote)]);
}

juce::String SignatureService::getScaleName (ScaleKind scale)
{
    switch (scale)
    {
        case ScaleKind::major:
            return TRANS ("Major");
        case ScaleKind::minor:
            return TRANS ("Minor");
        case ScaleKind::dorian:
            return TRANS ("Dorian");
        case ScaleKind::phrygian:
            return TRANS ("Phrygian");
        case ScaleKind::lydian:
            return TRANS ("Lydian");
        case ScaleKind::mixolydian:
            return TRANS ("Mixolydian");
        case ScaleKind::locrian:
            return TRANS ("Locrian");
        case ScaleKind::harmonicMinor:
            return TRANS ("Harmonic minor");
        case ScaleKind::melodicMinor:
            return TRANS ("Melodic minor");
    }

    return {};
}

juce::String SignatureService::getKeyName (int rootNote, ScaleKind scale)
{
    return plugins::i18n::format (TRANS ("{note} {scale}"), { { "note", getNoteName (rootNote) }, { "scale", getScaleName (scale) } });
}

std::vector<int> SignatureService::getScalePitchClasses (int rootNote, ScaleKind scale)
{
    std::vector<int> classes;

    for (const int step : te::Scale (toTracktion (scale)).getSteps())
        classes.push_back (normalisedPitchClass (rootNote + step));

    return classes;
}

bool SignatureService::isInScale (int noteNumber, int rootNote, ScaleKind scale)
{
    const auto classes = getScalePitchClasses (rootNote, scale);
    return std::find (classes.begin(), classes.end(), normalisedPitchClass (noteNumber)) != classes.end();
}

std::vector<int>
SignatureService::getChordNotes (int rootNote, ScaleKind scale, int degree, ChordKind kind, int inversion, int octave)
{
    std::vector<int> notes;
    const auto chords = chordsFor (scale, kind);
    const auto steps = te::Scale (toTracktion (scale)).getSteps();
    const int degreeIndex = juce::jlimit (0, chords.size() - 1, degree - 1);

    if (chords.isEmpty() || degreeIndex >= steps.size())
        return notes;

    // Grundton des Akkords = Grundton der Tonart + Skalenschritt der Stufe; C4 = 60.
    const int chordRoot = (octave + 1) * 12 + normalisedPitchClass (rootNote) + steps[degreeIndex];

    for (const int interval : chords[degreeIndex].getSteps (inversion))
        notes.push_back (chordRoot + interval);

    return notes;
}

juce::String SignatureService::getChordSymbol (int rootNote, ScaleKind scale, int degree, ChordKind kind)
{
    const auto chords = chordsFor (scale, kind);
    const auto steps = te::Scale (toTracktion (scale)).getSteps();
    const int degreeIndex = juce::jlimit (0, chords.size() - 1, degree - 1);

    if (chords.isEmpty() || degreeIndex >= steps.size())
        return {};

    // Eigene Symbole statt Tracktions ("M" für Dur): Dur ohne Zusatz, wie auf Leadsheets üblich.
    juce::String symbol;

    switch (chords[degreeIndex].getType())
    {
        case te::Chord::majorTriad:
            symbol = "";
            break;
        case te::Chord::minorTriad:
            symbol = "m";
            break;
        case te::Chord::diminishedTriad:
            symbol = "dim";
            break;
        case te::Chord::augmentedTriad:
            symbol = "aug";
            break;
        case te::Chord::dominatSeventhChord:
            symbol = "7";
            break;
        case te::Chord::majorSeventhChord:
            symbol = "maj7";
            break;
        case te::Chord::minorSeventhChord:
            symbol = "m7";
            break;
        case te::Chord::halfDiminishedSeventhChord:
            symbol = "m7b5";
            break;
        case te::Chord::diminishedSeventhChord:
            symbol = "dim7";
            break;
        case te::Chord::augmentedSeventhChord:
            symbol = "aug7";
            break;
        case te::Chord::minorMajorSeventhChord:
            symbol = "mMaj7";
            break;
        case te::Chord::majorSixthChord:
            symbol = "6";
            break;
        case te::Chord::minorSixthChord:
            symbol = "m6";
            break;
        default:
            symbol = chords[degreeIndex].getSymbol(); // Sus, Power, None: Tracktions Symbol
            break;
    }

    return getNoteName (rootNote + steps[degreeIndex]) + symbol;
}

juce::String SignatureService::getDegreeName (int rootNote, ScaleKind scale, int degree, ChordKind kind)
{
    static const char* const roman[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    const int degreeIndex = juce::jlimit (0, 6, degree - 1);
    juce::String name (roman[degreeIndex]);

    const auto chords = chordsFor (scale, kind);

    if (degreeIndex < chords.size())
    {
        const auto type = chords[degreeIndex].getType();

        // Kleinbuchstaben für Moll, "°" für vermindert, "+" für übermäßig: wie in Harmonielehre-Büchern.
        if (type == te::Chord::minorTriad || type == te::Chord::minorSeventhChord ||
            type == te::Chord::diminishedTriad || type == te::Chord::diminishedSeventhChord ||
            type == te::Chord::halfDiminishedSeventhChord)
            name = name.toLowerCase();

        if (type == te::Chord::diminishedTriad || type == te::Chord::diminishedSeventhChord)
            name += juce::String::fromUTF8 ("°");
        else if (type == te::Chord::halfDiminishedSeventhChord)
            name += juce::String::fromUTF8 ("ø");
        else if (type == te::Chord::augmentedTriad)
            name += "+";
    }

    juce::ignoreUnused (rootNote);
    return name;
}

//==============================================================================
void SignatureService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void SignatureService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void SignatureService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
    }

    cancelPendingUpdate();
    handleAsyncUpdate();
}

void SignatureService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.signatureChanged(); });
}

bool SignatureService::isSignatureTree (const juce::ValueTree& tree)
{
    return tree.hasType (te::IDs::TEMPO) || tree.hasType (te::IDs::TIMESIG) || tree.hasType (te::IDs::PITCH);
}

void SignatureService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (isSignatureTree (tree))
        triggerAsyncUpdate();
}

void SignatureService::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (isSignatureTree (child))
        triggerAsyncUpdate();
}

void SignatureService::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (isSignatureTree (child))
        triggerAsyncUpdate();
}

} // namespace fk::services
