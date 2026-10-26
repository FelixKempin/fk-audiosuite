#include "fk/services/ClipService.h"

#include "fk/services/TrackProperties.h"

#include "fk/dsp/analysis/TempoEstimator.h"
#include "fk/services/BakeLock.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <span>

namespace fk::services
{

ClipService::ClipService (ProjectService& service)
    : projectService (service)
{
}

core::Result<juce::String>
ClipService::importAudioFile (te::AudioTrack& track, const juce::File& audioFile, double startSeconds)
{
    if (auto unlocked = bakeLock::checkClipsUnlocked (track); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Import audio file"));
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto imported = ensureInProject (audioFile);

    if (! imported)
        return core::fail (imported.error().code, imported.error().message);

    const te::AudioFile file (edit->engine, *imported);

    if (! file.isValid())
        return core::fail (core::ErrorCode::unsupported,
                           plugins::i18n::format (TRANS ("The audio file is not readable: {path}"), { { "path", audioFile.getFullPathName() } }).toStdString());

    const te::ClipPosition position { { te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds)),
                                        te::TimeDuration::fromSeconds (file.getLength()) },
                                      {} };

    auto clip = track.insertWaveClip (imported->getFileNameWithoutExtension(), *imported, position, false);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the clip.").toStdString());

    return clip->itemID.toString();
}

namespace
{
/// Liest die Spuren einer MIDI-Datei als Tracktion-Listen (Schläge); nur Spuren mit Noten.
std::vector<std::unique_ptr<te::MidiList>> readMidiTracks (const juce::File& midiFile)
{
    std::vector<std::unique_ptr<te::MidiList>> result;
    juce::OwnedArray<te::MidiList> lists;
    juce::Array<te::BeatPosition> tempoBeats;
    juce::Array<double> bpms;
    juce::Array<int> numerators, denominators;
    te::BeatDuration songLength;

    if (! te::MidiList::readSeparateTracksFromFile (
            midiFile, lists, tempoBeats, bpms, numerators, denominators, songLength, false))
        return result;

    while (! lists.isEmpty())
    {
        std::unique_ptr<te::MidiList> list (lists.removeAndReturn (0));

        if (! list->getNotes().isEmpty())
            result.push_back (std::move (list));
    }

    return result;
}
} // namespace

int ClipService::countMidiTracks (const juce::File& midiFile)
{
    return static_cast<int> (readMidiTracks (midiFile).size());
}

core::Result<juce::String>
ClipService::importMidiFile (te::AudioTrack& track, const juce::File& midiFile, double startSeconds, int midiTrackIndex)
{
    if (auto unlocked = bakeLock::checkClipsUnlocked (track); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto lists = readMidiTracks (midiFile);

    if (lists.empty())
        return core::fail (core::ErrorCode::unsupported,
                           plugins::i18n::format (TRANS ("The MIDI file is not readable or has no notes: {path}"), { { "path", midiFile.getFullPathName() } }).toStdString());

    if (midiTrackIndex >= static_cast<int> (lists.size()))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The MIDI file does not have that many tracks.").toStdString());

    // Gewählte Spur oder alle zusammen; die Länge ist das Ende der letzten Note (mindestens ein Schlag).
    te::MidiList merged;
    double lengthBeats = 1.0;

    for (std::size_t i = 0; i < lists.size(); ++i)
    {
        if (midiTrackIndex >= 0 && static_cast<int> (i) != midiTrackIndex)
            continue;

        merged.addFrom (*lists[i], nullptr);

        for (auto* note : lists[i]->getNotes())
            lengthBeats = juce::jmax (lengthBeats, note->getEndBeat().inBeats());
    }

    projectService.beginUndoStep (TRANS ("Import MIDI file"));

    const auto start = te::TimePosition::fromSeconds (juce::jmax (0.0, startSeconds));
    const auto end =
        edit->tempoSequence.toTime (edit->tempoSequence.toBeats (start) + te::BeatDuration::fromBeats (lengthBeats));
    const auto name =
        midiFile.getFileNameWithoutExtension() +
        (midiTrackIndex >= 0 && lists.size() > 1 ? " " + juce::String (midiTrackIndex + 1) : juce::String());
    auto clip = track.insertMIDIClip (name, { start, end }, nullptr);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the MIDI clip.").toStdString());

    clip->getSequence().addFrom (merged, &edit->getUndoManager());
    return clip->itemID.toString();
}

core::Result<void> ClipService::moveClip (const juce::String& clipId, double newStartSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Move clip"));
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    // preserveSync = false: Der Clip nimmt sein Material mit. keepLength = true: nur verschieben, nicht trimmen.
    clip->setStart (te::TimePosition::fromSeconds (juce::jmax (0.0, newStartSeconds)), false, true);
    return {};
}

core::Result<void> ClipService::splitClip (const juce::String& clipId, double atSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Split clip"));
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    auto* track = clip->getClipTrack();

    if (track == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The clip is not on any track.").toStdString());

    const auto splitTime = te::TimePosition::fromSeconds (atSeconds);

    if (! clip->getEditTimeRange().contains (splitTime))
        return {};

    track->splitClip (*clip, splitTime);
    return {};
}

core::Result<void> ClipService::deleteClip (const juce::String& clipId)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Delete clip"));
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    clip->removeFromParent();
    return {};
}

core::Result<void> ClipService::deleteClips (const std::vector<juce::String>& clipIds)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Delete clips"));

    for (const auto& clipId : clipIds)
        if (auto* clip = findClip (clipId))
            clip->removeFromParent();

    return {};
}

namespace
{
/// Hörbarer Ausschnitt eines Audio-Clips aus seiner Quelldatei, in der Rate der Datei.
struct ClipAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
};

core::Result<ClipAudio> readClipAudio (te::WaveAudioClip& clip)
{
    auto& formats = clip.edit.engine.getAudioFileFormatManager().readFormatManager;
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (clip.getOriginalFile()));

    if (reader == nullptr)
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("The audio file is not readable: {path}"), { { "path", clip.getOriginalFile().getFullPathName() } }).toStdString());

    // Die Quelle kann schneller oder langsamer laufen als das Projekt (Speed-Ratio); dann ist der
    // Ausschnitt in der Datei entsprechend länger oder kürzer.
    const auto position = clip.getPosition();
    const auto speed = clip.getSpeedRatio() > 0.0 ? clip.getSpeedRatio() : 1.0;
    const auto first =
        static_cast<juce::int64> (std::llround (position.getOffset().inSeconds() * speed * reader->sampleRate));
    const auto wanted =
        static_cast<juce::int64> (std::llround (position.getLength().inSeconds() * speed * reader->sampleRate));
    const auto count = static_cast<int> (
        juce::jlimit<juce::int64> (0, std::max<juce::int64> (0, reader->lengthInSamples - first), wanted));

    ClipAudio audio;
    audio.sampleRate = reader->sampleRate;
    audio.buffer.setSize (static_cast<int> (reader->numChannels), count);

    if (count > 0 && ! reader->read (&audio.buffer, 0, count, first, true, true))
        return core::fail (core::ErrorCode::ioError, TRANS ("Could not read the audio file.").toStdString());

    return audio;
}
} // namespace

namespace
{
constexpr auto stretchMode = te::TimeStretcher::signalsmithDefault;

/// Time-Stretch braucht einen Algorithmus; ohne steht der Clip auf "disabled" und Pitch wirkt nicht.
void ensureStretchMode (te::AudioClipBase& clip)
{
    if (clip.getTimeStretchMode() == te::TimeStretcher::disabled)
        clip.setTimeStretchMode (stretchMode);
}
} // namespace

core::Result<double> ClipService::detectTempo (const juce::String& clipId) const
{
    auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

    if (clip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Tempo detection is only available for audio clips.").toStdString());

    auto& formats = clip->edit.engine.getAudioFileFormatManager().readFormatManager;
    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (clip->getOriginalFile()));

    if (reader == nullptr)
        return core::fail (core::ErrorCode::ioError, TRANS ("The audio file is not readable.").toStdString());

    // Die ersten 60 Sekunden reichen für eine Schätzung und halten den Speicher klein; Kanäle zu Mono mischen.
    const auto count = static_cast<int> (
        std::min<juce::int64> (reader->lengthInSamples, static_cast<juce::int64> (60.0 * reader->sampleRate)));
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), count);

    if (! reader->read (&buffer, 0, count, 0, true, true))
        return core::fail (core::ErrorCode::ioError, TRANS ("Could not read the audio file.").toStdString());

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.addFrom (0, 0, buffer, channel, 0, count);

    const auto bpm = dsp::analysis::TempoEstimator::estimate (
        std::span<const float> (buffer.getReadPointer (0), static_cast<std::size_t> (count)), reader->sampleRate);

    if (! bpm)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("No tempo detected (too short or no clear rhythm). Please enter the tempo.").toStdString());

    return std::round (*bpm * 100.0) / 100.0;
}

core::Result<void>
ClipService::setFollowsTempo (const std::vector<juce::String>& clipIds, bool follow, double sourceBpm)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    // Erst alle Tempi bestimmen, dann ändern: Ein Clip ohne erkennbares Tempo bricht alles vorher ab.
    std::vector<std::pair<te::WaveAudioClip*, double>> changes;

    for (const auto& clipId : clipIds)
    {
        auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

        if (clip == nullptr)
            continue;

        auto bpm = sourceBpm;

        if (follow && bpm <= 0.0)
        {
            const auto detected = detectTempo (clipId);

            if (! detected)
                return std::unexpected (detected.error());

            bpm = *detected;
        }

        changes.emplace_back (clip, bpm);
    }

    if (changes.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No audio clips in the selection.").toStdString());

    projectService.beginUndoStep (follow ? TRANS ("Follow project tempo") : TRANS ("Original tempo"));

    auto& tempo = projectService.getEdit()->tempoSequence;

    for (const auto& [clip, bpm] : changes)
    {
        const auto position = clip->getPosition();

        if (follow)
        {
            ensureStretchMode (*clip);
            auto loopInfo = clip->getLoopInfo();
            loopInfo.setBpm (bpm, clip->getWaveInfo());
            clip->setLoopInfo (loopInfo);

            // Länge so, dass genau das bisherige Material hineinpasst: Schläge des Materials an der
            // Clip-Position in Projektzeit umrechnen (Cubase: Musikmodus passt die Länge an).
            const auto materialSeconds = position.getLength().inSeconds() * clip->getSpeedRatio();
            const auto beats = materialSeconds * bpm / 60.0;
            const auto startBeat = tempo.toBeats (position.getStart());
            const auto end = tempo.toTime (startBeat + te::BeatDuration::fromBeats (beats));
            clip->setAutoTempo (true);
            clip->setLength (end - position.getStart(), true);
        }
        else
        {
            // Zurück auf Originalgeschwindigkeit: Material, das bisher im Clip lag, behält seine Dauer in Schlägen.
            const auto beats = tempo.toBeats (position.getEnd()) - tempo.toBeats (position.getStart());
            const auto materialBpm = clip->getLoopInfo().getBpm (clip->getWaveInfo());
            clip->setAutoTempo (false);

            if (materialBpm > 0.0)
                clip->setLength (
                    te::TimeDuration::fromSeconds (beats.inBeats() * 60.0 / materialBpm / clip->getSpeedRatio()), true);
        }
    }

    return {};
}

core::Result<void> ClipService::setPitch (const std::vector<juce::String>& clipIds, float semitones)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Pitch"));
    bool any = false;

    for (const auto& clipId : clipIds)
        if (auto* clip = dynamic_cast<te::AudioClipBase*> (findClip (clipId)))
        {
            ensureStretchMode (*clip);
            clip->setPitchChange (juce::jlimit (-24.0f, 24.0f, semitones));
            any = true;
        }

    if (! any)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Pitch is only available for audio clips.").toStdString());

    return {};
}

core::Result<void> ClipService::stretchClip (const juce::String& clipId, double newLengthSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = dynamic_cast<te::AudioClipBase*> (findClip (clipId));

    if (clip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Stretching is only available for audio clips.").toStdString());

    if (clip->getAutoTempo())
        return core::fail (
            core::ErrorCode::invalidArgument,
            TRANS ("The clip follows the project tempo, so the tempo determines its length. Turn off \"Follow tempo\" first.").toStdString());

    const auto oldLength = clip->getPosition().getLength().inSeconds();

    if (newLengthSeconds < 0.01 || oldLength <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Too short.").toStdString());

    // Gleiches Material in neuer Länge: Geschwindigkeit im umgekehrten Verhältnis (doppelt so lang = halbe
    // Geschwindigkeit).
    const auto newSpeed = clip->getSpeedRatio() * oldLength / newLengthSeconds;

    if (newSpeed < 0.25 || newSpeed > 4.0)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Stretching works from a quarter up to four times the length.").toStdString());

    projectService.beginUndoStep (TRANS ("Stretch clip"));
    ensureStretchMode (*clip);
    clip->setSpeedRatio (newSpeed);
    clip->setLength (te::TimeDuration::fromSeconds (newLengthSeconds), true);
    return {};
}

core::Result<void> ClipService::normalizeClips (const std::vector<juce::String>& clipIds, float targetDb)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    // Erst alles messen, dann ändern: Ein Fehler mittendrin lässt keine halb normalisierte Auswahl zurück.
    std::vector<std::pair<te::WaveAudioClip*, float>> changes;

    for (const auto& clipId : clipIds)
    {
        auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

        if (clip == nullptr)
            continue;

        const auto audio = readClipAudio (*clip);

        if (! audio)
            return std::unexpected (audio.error());

        const auto peak = audio->buffer.getMagnitude (0, audio->buffer.getNumSamples());

        if (peak <= 0.0f)
            continue; // still: nichts zu normalisieren

        changes.emplace_back (clip, juce::jlimit (-24.0f, 24.0f, targetDb - juce::Decibels::gainToDecibels (peak)));
    }

    if (changes.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No audible audio clips in the selection.").toStdString());

    projectService.beginUndoStep (TRANS ("Normalize"));

    for (const auto& [clip, gainDb] : changes)
        clip->setGainDB (gainDb);

    return {};
}

core::Result<void> ClipService::setClipsReversed (const std::vector<juce::String>& clipIds, bool shouldBeReversed)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (shouldBeReversed ? TRANS ("Reverse") : TRANS ("Unreverse"));

    for (const auto& clipId : clipIds)
        if (auto* clip = dynamic_cast<te::AudioClipBase*> (findClip (clipId)))
            clip->setIsReversed (shouldBeReversed);

    return {};
}

core::Result<void> ClipService::invertPhase (const std::vector<juce::String>& clipIds)
{
    return processAudio (clipIds, AudioProcess::invertPhase);
}

namespace
{
struct ProcessName
{
    juce::String undo;
    const char* fileSuffix;
};

ProcessName nameOf (ClipService::AudioProcess process)
{
    using P = ClipService::AudioProcess;

    switch (process)
    {
        case P::silence:
            return { TRANS ("Silence"), "Stille" };
        case P::invertPhase:
            return { TRANS ("Invert phase"), "Phase" };
        case P::reverse:
            return { TRANS ("Reverse section"), "Rueckwaerts" };
        case P::normalize:
            return { TRANS ("Normalize section"), "Normalisiert" };
        case P::gain:
            return { TRANS ("Amplify"), "Gain" };
        case P::fadeIn:
            return { TRANS ("Fade-in"), "FadeIn" };
        case P::fadeOut:
            return { TRANS ("Fade-out"), "FadeOut" };
    }

    return { TRANS ("Edit"), "Bearbeitet" };
}

/// Wendet die Bearbeitung auf [first, first + count) aller Kanäle an.
void applyProcess (
    juce::AudioBuffer<float>& buffer, int first, int count, ClipService::AudioProcess process, float gainDb)
{
    using P = ClipService::AudioProcess;

    switch (process)
    {
        case P::silence:
            buffer.clear (first, count);
            break;
        case P::invertPhase:
            buffer.applyGain (first, count, -1.0f);
            break;
        case P::reverse:
            buffer.reverse (first, count);
            break;
        case P::normalize:
            if (const auto peak = buffer.getMagnitude (first, count); peak > 0.0f)
                buffer.applyGain (first, count, 1.0f / peak);
            break;
        case P::gain:
            buffer.applyGain (first, count, juce::Decibels::decibelsToGain (gainDb));
            break;
        case P::fadeIn:
            buffer.applyGainRamp (first, count, 0.0f, 1.0f);
            break;
        case P::fadeOut:
            buffer.applyGainRamp (first, count, 1.0f, 0.0f);
            break;
    }
}
} // namespace

core::Result<void> ClipService::processAudio (const std::vector<juce::String>& clipIds,
                                              AudioProcess process,
                                              std::optional<ClipRange> range,
                                              float gainDb)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto audioFolder = projectService.getProjectFolder().getChildFile ("Audio");

    if (! audioFolder.createDirectory())
        return core::fail (core::ErrorCode::ioError, TRANS ("The Audio/ folder is missing.").toStdString());

    const auto name = nameOf (process);

    // Erst alle Dateien schreiben, dann in einem Undo-Schritt umhängen.
    std::vector<std::pair<te::WaveAudioClip*, juce::File>> changes;

    for (const auto& clipId : clipIds)
    {
        auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

        if (clip == nullptr)
            continue;

        if (clip->getIsReversed())
            return core::fail (core::ErrorCode::invalidArgument,
                               TRANS ("Not possible on reversed clips. Turn off \"Reverse\" first.").toStdString());

        if (clip->getAutoTempo())
            return core::fail (core::ErrorCode::invalidArgument,
                               TRANS ("Not possible while the clip follows the project tempo. Turn off \"Follow tempo\" first.").toStdString());

        auto audio = readClipAudio (*clip);

        if (! audio)
            return std::unexpected (audio.error());

        // Clip-Zeit in Samples der Datei: Die Quelle läuft mit der Geschwindigkeit des Clips.
        const auto total = audio->buffer.getNumSamples();
        const auto samplesPerClipSecond = audio->sampleRate * clip->getSpeedRatio();
        const auto first =
            range ? juce::jlimit (0, total, static_cast<int> (std::llround (range->fromSeconds * samplesPerClipSecond)))
                  : 0;
        const auto last =
            range
                ? juce::jlimit (first, total, static_cast<int> (std::llround (range->toSeconds * samplesPerClipSecond)))
                : total;

        if (last - first < 1)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("The section is empty.").toStdString());

        applyProcess (audio->buffer, first, last - first, process, gainDb);

        const auto file =
            audioFolder.getChildFile (clip->getName() + " " + name.fileSuffix + ".wav").getNonexistentSibling (false);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer =
            wav.createWriterFor (stream,
                                 juce::AudioFormatWriterOptions {}
                                     .withSampleRate (audio->sampleRate)
                                     .withNumChannels (audio->buffer.getNumChannels())
                                     .withBitsPerSample (32)
                                     .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));

        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio->buffer, 0, audio->buffer.getNumSamples()))
            return core::fail (core::ErrorCode::ioError,
                               plugins::i18n::format (TRANS ("Could not write the file: {path}"), { { "path", file.getFullPathName() } }).toStdString());

        writer.reset(); // Datei schließen, bevor Tracktion sie liest
        changes.emplace_back (clip, file);
    }

    if (changes.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No audio clips in the selection.").toStdString());

    projectService.beginUndoStep (name.undo);

    for (const auto& [clip, file] : changes)
    {
        projectService.registerProjectFile (file);
        clip->getSourceFileReference().setToFile (file, te::SourceFileReference::PathStyle::alwaysRelative, false);
        clip->setOffset (te::TimeDuration()); // die neue Datei beginnt genau am Clip-Anfang
    }

    return {};
}

std::vector<juce::String>
ClipService::getClipsInRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds) const
{
    std::vector<juce::String> ids;

    for (auto* track : tracks)
        for (auto* clip : track->getClips())
        {
            const auto range = clip->getPosition().time;

            if (range.getEnd().inSeconds() > fromSeconds && range.getStart().inSeconds() < toSeconds)
                ids.push_back (clip->itemID.toString());
        }

    return ids;
}

core::Result<void>
ClipService::deleteRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds, bool closeGap)
{
    if (toSeconds - fromSeconds < 0.0001)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The range is empty.").toStdString());

    if (auto unlocked = checkUnlocked (getClipsInRange (tracks, fromSeconds, toSeconds)); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (closeGap ? TRANS ("Remove range") : TRANS ("Delete range"));
    const te::TimeRange range (te::TimePosition::fromSeconds (fromSeconds), te::TimePosition::fromSeconds (toSeconds));
    const auto length = range.getLength();

    for (auto* track : tracks)
    {
        track->deleteRegion (range, nullptr);

        // Reste unter einer Zehntel-Millisekunde stören nur: weg damit (wie Tracktions eigene Funktion).
        for (auto* clip : track->getClips())
            if (clip->getPosition().getLength() < te::TimeDuration::fromSeconds (0.0001))
                clip->removeFromParent();

        if (closeGap)
            for (auto* clip : track->getClips())
                if (clip->getPosition().getStart() >= range.getEnd() - te::TimeDuration::fromSeconds (0.0001))
                    clip->setStart (clip->getPosition().getStart() - length, false, true);
    }

    return {};
}

core::Result<void>
ClipService::insertSilence (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (toSeconds - fromSeconds < 0.0001)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The range is empty.").toStdString());

    if (auto unlocked = checkUnlocked (getClipsInRange (tracks, fromSeconds, 1.0e9)); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Insert silence"));
    juce::Array<te::Track*> targets;

    for (auto* track : tracks)
        targets.add (track);

    // Nie mit leerer Liste aufrufen: Dann verschiebt Tracktion alle Spuren samt Tempospur.
    if (! targets.isEmpty())
        te::insertSpaceIntoEdit (
            *edit,
            te::TimeRange (te::TimePosition::fromSeconds (fromSeconds), te::TimePosition::fromSeconds (toSeconds)),
            targets);

    return {};
}

core::Result<void>
ClipService::splitAtRange (const std::vector<TrackHandle*>& tracks, double fromSeconds, double toSeconds)
{
    if (auto unlocked = checkUnlocked (getClipsInRange (tracks, fromSeconds, toSeconds)); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (TRANS ("Split at range"));

    for (auto* track : tracks)
        for (const auto seconds : { toSeconds, fromSeconds }) // erst hinten, dann vorne
        {
            const auto time = te::TimePosition::fromSeconds (seconds);

            for (auto* clip : track->getClips())
            {
                const auto range = clip->getPosition().time;

                if (range.getStart() < time && time < range.getEnd())
                {
                    track->splitClip (*clip, time);
                    break; // ein Zeitpunkt trifft höchstens einen Clip je Spur (Überlappungen ausgenommen)
                }
            }
        }

    return {};
}

core::Result<void> ClipService::setClipsMuted (const std::vector<juce::String>& clipIds, bool shouldBeMuted)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    projectService.beginUndoStep (shouldBeMuted ? TRANS ("Mute clips") : TRANS ("Unmute clips"));

    for (const auto& clipId : clipIds)
        if (auto* clip = findClip (clipId))
            clip->setMuted (shouldBeMuted);

    return {};
}

core::Result<void> ClipService::trimClip (const juce::String& clipId, double newStartSeconds, double newEndSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    const auto position = clip->getPosition();
    const auto currentStart = position.getStart().inSeconds();
    const auto currentEnd = position.getEnd().inSeconds();

    // Der linke Rand darf nicht vor den Anfang des Materials: Der Versatz würde negativ.
    const auto earliestStart = juce::jmax (0.0, currentStart - position.getOffset().inSeconds());
    const auto start = juce::jmax (earliestStart, newStartSeconds);
    const auto end = juce::jmax (start + 0.01, newEndSeconds);

    if (std::abs (start - currentStart) < 1.0e-6 && std::abs (end - currentEnd) < 1.0e-6)
        return {};

    projectService.beginUndoStep (TRANS ("Trim clip"));

    // preserveSync = true: Das Material bleibt an seiner Stelle, nur die Grenzen wandern.
    if (std::abs (start - currentStart) >= 1.0e-6)
        clip->setStart (te::TimePosition::fromSeconds (start), true, false);

    clip->setEnd (te::TimePosition::fromSeconds (end), true);
    return {};
}

core::Result<void>
ClipService::moveClips (const std::vector<juce::String>& clipIds, double deltaSeconds, int trackDelta)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto audioTracks = te::getAudioTracks (*edit);

    // Erst alle Ziele prüfen, dann ändern: Entweder bewegt sich alles oder nichts.
    struct Move
    {
        te::Clip* clip;
        te::AudioTrack* target;
    };
    std::vector<Move> moves;

    for (const auto& clipId : clipIds)
    {
        auto* clip = findClip (clipId);

        if (clip == nullptr)
            continue;

        auto* source = dynamic_cast<te::AudioTrack*> (clip->getClipTrack());
        const auto sourceIndex = audioTracks.indexOf (source);

        if (source == nullptr || sourceIndex < 0)
            return core::fail (core::ErrorCode::unknown, TRANS ("The clip is not on any track.").toStdString());

        const auto targetIndex = sourceIndex + trackDelta;

        if (targetIndex < 0 || targetIndex >= audioTracks.size())
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("There is no track there.").toStdString());

        auto* target = audioTracks[targetIndex];

        if (bakeLock::areClipsLocked (*target))
            return std::unexpected (bakeLock::lockedError (*target));

        if (target != source && ! clipFitsTrack (*clip, *target))
            return core::fail (core::ErrorCode::invalidArgument,
                               (clip->isMidi() ? TRANS ("MIDI clips only fit on instrument tracks.")
                                               : TRANS ("Audio clips only fit on audio tracks."))
                                   .toStdString());

        moves.push_back ({ clip, target });
    }

    if (moves.empty())
        return {};

    // Nie vor Null: Der ganze Block rückt höchstens bis zum frühesten Clip an den Anfang.
    double earliest = std::numeric_limits<double>::max();

    for (const auto& move : moves)
        earliest = juce::jmin (earliest, move.clip->getPosition().getStart().inSeconds());

    const auto delta = juce::jmax (deltaSeconds, -earliest);

    projectService.beginUndoStep (trackDelta == 0 ? TRANS ("Move clips") : TRANS ("Move clips to another track"));

    for (const auto& move : moves)
    {
        if (move.target != move.clip->getClipTrack())
            move.clip->moveTo (*move.target);

        move.clip->setStart (move.clip->getPosition().getStart() + te::TimeDuration::fromSeconds (delta), false, true);
    }

    return {};
}

core::Result<void> ClipService::copyClips (const std::vector<juce::String>& clipIds)
{
    auto entries = makeEntries (clipIds);

    if (entries.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No clips selected.").toStdString());

    clipboard = std::move (entries);
    return {};
}

core::Result<void> ClipService::cutClips (const std::vector<juce::String>& clipIds)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    if (const auto copied = copyClips (clipIds); ! copied)
        return copied;

    projectService.beginUndoStep (TRANS ("Cut clips"));

    for (const auto& clipId : clipIds)
        if (auto* clip = findClip (clipId))
            clip->removeFromParent();

    return {};
}

bool ClipService::hasClipboardContent() const noexcept
{
    return ! clipboard.empty();
}

core::Result<std::vector<juce::String>> ClipService::pasteClips (te::AudioTrack& track, double atSeconds)
{
    if (auto unlocked = bakeLock::checkClipsUnlocked (track); ! unlocked)
        return std::unexpected (unlocked.error());

    if (clipboard.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The clipboard is empty.").toStdString());

    projectService.beginUndoStep (TRANS ("Paste clips"));
    return insertEntries (clipboard, track, juce::jmax (0.0, atSeconds));
}

core::Result<std::vector<juce::String>> ClipService::duplicateClips (const std::vector<juce::String>& clipIds)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    const auto entries = makeEntries (clipIds);
    auto* edit = projectService.getEdit();

    if (entries.empty() || edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No clips selected.").toStdString());

    // Block direkt hinter das Ende der Auswahl. makeEntries rechnet die Abstände von der obersten
    // beteiligten Spur und dem frühesten Anfang aus, also beides hier genauso bestimmen.
    const auto audioTracks = te::getAudioTracks (*edit);
    double blockStart = std::numeric_limits<double>::max();
    double blockEnd = 0.0;
    te::AudioTrack* topTrack = nullptr;
    int topIndex = audioTracks.size();

    for (const auto& clipId : clipIds)
    {
        auto* clip = findClip (clipId);

        if (clip == nullptr)
            continue;

        blockStart = juce::jmin (blockStart, clip->getPosition().getStart().inSeconds());
        blockEnd = juce::jmax (blockEnd, clip->getPosition().getEnd().inSeconds());

        if (auto* clipTrack = dynamic_cast<te::AudioTrack*> (clip->getClipTrack()))
            if (const auto index = audioTracks.indexOf (clipTrack); index >= 0 && index < topIndex)
            {
                topIndex = index;
                topTrack = clipTrack;
            }
    }

    if (topTrack == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The clip is not on any track.").toStdString());

    projectService.beginUndoStep (TRANS ("Duplicate clips"));
    return insertEntries (entries, *topTrack, blockEnd);
}

core::Result<juce::String> ClipService::mergeClips (const std::vector<juce::String>& clipIds)
{
    if (auto unlocked = checkUnlocked (clipIds); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    std::vector<te::Clip*> found;

    for (const auto& clipId : clipIds)
        if (auto* clip = findClip (clipId))
            found.push_back (clip);

    if (found.size() < 2)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Select at least two clips to glue.").toStdString());

    auto* track = dynamic_cast<te::AudioTrack*> (found.front()->getClipTrack());

    if (track == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The clip is not on any track.").toStdString());

    const bool midi = found.front()->isMidi();

    for (auto* clip : found)
    {
        if (clip->getClipTrack() != track)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("Only clips on the same track can be glued.").toStdString());

        if (clip->isMidi() != midi)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("Audio and MIDI clips cannot be joined.").toStdString());
    }

    std::sort (found.begin(),
               found.end(),
               [] (const te::Clip* a, const te::Clip* b)
               { return a->getPosition().getStart() < b->getPosition().getStart(); });

    if (midi)
    {
        juce::Array<te::MidiClip*> midiClips;

        for (auto* clip : found)
            midiClips.add (dynamic_cast<te::MidiClip*> (clip));

        projectService.beginUndoStep (TRANS ("Glue MIDI clips"));

        // Tracktion liefert den neuen Clip nicht zurück: Er ist der, den es vorher nicht gab.
        std::vector<te::EditItemID> before;

        for (auto* clip : track->getClips())
            before.push_back (clip->itemID);

        if (const auto result = te::mergeMidiClips (midiClips, nullptr); result.failed())
            return core::fail (core::ErrorCode::unknown, result.getErrorMessage().toStdString());

        te::Clip* newest = nullptr;

        for (auto* clip : track->getClips())
            if (std::find (before.begin(), before.end(), clip->itemID) == before.end())
                newest = clip;

        if (newest == nullptr)
            return core::fail (core::ErrorCode::unknown, TRANS ("Glued clip not found.").toStdString());

        return newest->itemID.toString();
    }

    // Audio: den Bereich der Clips ohne Plugins rendern, dann Originale durch die Datei ersetzen.
    const auto projectFolder = projectService.getProjectFolder();

    if (projectFolder == juce::File())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Gluing needs a saved project.").toStdString());

    const auto audioFolder = projectFolder.getChildFile ("Audio");
    audioFolder.createDirectory();
    const auto name = found.front()->getName();
    const auto file = audioFolder.getChildFile (name + " geklebt.wav").getNonexistentSibling (false);
    auto& engine = edit->engine;
    auto* audioFormat = engine.getAudioFileFormatManager().getFormatFromFileName (file);

    if (audioFormat == nullptr)
        return core::fail (core::ErrorCode::unsupported, TRANS ("WAV format not available.").toStdString());

    auto rangeEnd = found.front()->getPosition().getEnd();

    for (auto* clip : found)
        rangeEnd = juce::jmax (rangeEnd, clip->getPosition().getEnd());

    const te::TimeRange range (found.front()->getPosition().getStart(), rangeEnd);

    te::Renderer::Parameters parameters (*edit);
    parameters.destFile = file;
    parameters.audioFormat = audioFormat;
    parameters.bitDepth = 24;
    parameters.sampleRateForAudio = engine.getDeviceManager().getSampleRate();
    parameters.blockSizeForAudio = engine.getDeviceManager().getBlockSize();
    parameters.time = range;
    parameters.usePlugins = false;
    parameters.useMasterPlugins = false;
    parameters.tracksToDo.setBit (te::getAllTracks (*edit).indexOf (track));

    for (auto* clip : found)
        parameters.allowedClips.add (clip);

    {
        te::TransportControl::stopAllTransports (engine, false, true);
        const te::Edit::ScopedRenderStatus renderStatus (*edit, true);
        auto task = te::render_utils::createRenderTask (parameters, "Kleben", nullptr, nullptr);

        if (task == nullptr)
            return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the render task.").toStdString());

        while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
        }

        if (task->errorMessage.isNotEmpty())
            return core::fail (core::ErrorCode::ioError,
                               plugins::i18n::format (TRANS ("Gluing failed: {error}"), { { "error", task->errorMessage } }).toStdString());
    }

    if (! file.existsAsFile())
        return core::fail (core::ErrorCode::ioError, TRANS ("Gluing did not produce a file.").toStdString());

    projectService.registerProjectFile (file);
    projectService.beginUndoStep (TRANS ("Glue audio clips"));

    for (auto* clip : found)
        clip->removeFromParent();

    const te::ClipPosition position { range, {} };
    auto newClip = track->insertWaveClip (name, file, position, false);

    if (newClip == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the glued clip.").toStdString());

    return newClip->itemID.toString();
}

std::vector<ClipService::ClipboardEntry> ClipService::makeEntries (const std::vector<juce::String>& clipIds) const
{
    std::vector<ClipboardEntry> entries;
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return entries;

    const auto audioTracks = te::getAudioTracks (*edit);
    double firstStart = std::numeric_limits<double>::max();
    int firstTrackIndex = audioTracks.size();

    for (const auto& clipId : clipIds)
    {
        auto* clip = findClip (clipId);

        if (clip == nullptr)
            continue;

        const auto trackIndex = audioTracks.indexOf (dynamic_cast<te::AudioTrack*> (clip->getClipTrack()));

        if (trackIndex < 0)
            continue;

        firstStart = juce::jmin (firstStart, clip->getPosition().getStart().inSeconds());
        firstTrackIndex = juce::jmin (firstTrackIndex, trackIndex);
        entries.push_back ({ clip->state.createCopy(), clip->getPosition().getStart().inSeconds(), trackIndex });
    }

    for (auto& entry : entries)
    {
        entry.startOffsetSeconds -= firstStart;
        entry.trackOffset -= firstTrackIndex;
    }

    return entries;
}

core::Result<std::vector<juce::String>>
ClipService::insertEntries (const std::vector<ClipboardEntry>& entries, te::AudioTrack& firstTrack, double atSeconds)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto audioTracks = te::getAudioTracks (*edit);
    const auto firstIndex = audioTracks.indexOf (&firstTrack);

    if (firstIndex < 0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Target track does not exist.").toStdString());

    // Erst alle Ziele prüfen: Entweder landet alles oder nichts.
    std::vector<te::AudioTrack*> targets;

    for (const auto& entry : entries)
    {
        const auto index = firstIndex + entry.trackOffset;

        if (index >= audioTracks.size())
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("There are not enough tracks below the target track for the copy.").toStdString());

        auto* target = audioTracks[index];

        if (! clipStateFitsTrack (entry.state, *target))
            return core::fail (core::ErrorCode::invalidArgument,
                               (entry.state.hasType (te::IDs::MIDICLIP) ? TRANS ("MIDI clips only fit on instrument tracks.")
                                                                        : TRANS ("Audio clips only fit on audio tracks."))
                                   .toStdString());

        targets.push_back (target);
    }

    std::vector<juce::String> newIds;

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        // Kopie des Zustands mit neuen Kennungen (auch für Plugins im Clip), dann an die neue Zeit.
        auto state = entries[i].state.createCopy();
        te::EditItemID::remapIDs (state, nullptr, *edit);
        const auto length = te::TimeDuration::fromSeconds (static_cast<double> (state[te::IDs::length]));
        const auto start = te::TimePosition::fromSeconds (atSeconds + entries[i].startOffsetSeconds);
        const auto offset = te::TimeDuration::fromSeconds (static_cast<double> (state[te::IDs::offset]));

        auto* clip = targets[i]->insertClipWithState (state,
                                                      state[te::IDs::name].toString(),
                                                      state.hasType (te::IDs::MIDICLIP) ? te::TrackItem::Type::midi
                                                                                        : te::TrackItem::Type::wave,
                                                      { { start, start + length }, offset },
                                                      false,
                                                      false);

        if (clip == nullptr)
            return core::fail (core::ErrorCode::unknown, TRANS ("Could not paste the clip.").toStdString());

        newIds.push_back (clip->itemID.toString());
    }

    return newIds;
}

namespace
{
/// Spuren mit Instrument (auch Rack) und MIDI-Spuren an Geräten tragen MIDI-Clips, alle anderen Audio.
bool takesMidi (te::AudioTrack& track)
{
    if (static_cast<bool> (track.state.getProperty (trackProperties::externalMidi, false)))
        return true;

    for (auto plugin : track.pluginList.getPlugins())
        if (plugin->isSynth())
            return true;

    return false;
}
} // namespace

bool ClipService::clipFitsTrack (const te::Clip& clip, te::AudioTrack& track)
{
    return clip.isMidi() == takesMidi (track);
}

bool ClipService::clipStateFitsTrack (const juce::ValueTree& clipState, te::AudioTrack& track)
{
    return clipState.hasType (te::IDs::MIDICLIP) == takesMidi (track);
}

core::Result<void> ClipService::setFades (const juce::String& clipId, double fadeInSeconds, double fadeOutSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* audioClip = dynamic_cast<te::AudioClipBase*> (findClip (clipId));

    if (audioClip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Fades are only available for audio clips.").toStdString());

    const auto length = audioClip->getPosition().getLength().inSeconds();
    const auto fadeIn = juce::jlimit (0.0, length, fadeInSeconds);
    const auto fadeOut = juce::jlimit (0.0, length - fadeIn, fadeOutSeconds);

    audioClip->setFadeIn (te::TimeDuration::fromSeconds (fadeIn));
    audioClip->setFadeOut (te::TimeDuration::fromSeconds (fadeOut));
    return {};
}

core::Result<void> ClipService::setGainDb (const juce::String& clipId, float gainDb)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* audioClip = dynamic_cast<te::AudioClipBase*> (findClip (clipId));

    if (audioClip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Clip gain is only available for audio clips.").toStdString());

    audioClip->setGainDB (juce::jlimit (-24.0f, 24.0f, gainDb));
    return {};
}

void ClipService::beginChange (const juce::String& actionName)
{
    projectService.beginUndoStep (actionName);
}

std::vector<TakeInfo> ClipService::getTakes (const juce::String& clipId) const
{
    std::vector<TakeInfo> takes;
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return takes;

    const auto descriptions = clip->getTakeDescriptions();
    const int current = clip->getCurrentTake();

    for (int i = 0; i < descriptions.size(); ++i)
        takes.push_back ({ i, descriptions[i].fromFirstOccurrenceOf (". ", false, false), i == current });

    return takes;
}

core::Result<void> ClipService::setCurrentTake (const juce::String& clipId, int takeIndex)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    if (takeIndex < 0 || takeIndex >= clip->getNumTakes (true))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Take does not exist.").toStdString());

    projectService.beginUndoStep (TRANS ("Select take"));
    clip->setCurrentTake (takeIndex);
    return {};
}

core::Result<void> ClipService::addTakeFromFile (const juce::String& clipId, const juce::File& audioFile)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

    if (clip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Takes from files are only available for audio clips.").toStdString());

    if (! audioFile.existsAsFile())
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("File not found: {path}"), { { "path", audioFile.getFullPathName() } }).toStdString());

    const auto imported = ensureInProject (audioFile);

    if (! imported)
        return core::fail (imported.error().code, imported.error().message);

    projectService.beginUndoStep (TRANS ("Add take"));

    // Ohne Takes-Liste wird die aktuelle Datei zum ersten Take, sonst verlöre sie ihren Platz.
    // Tracktion erkennt den aktuellen Take über die Quellreferenz; die Schreibweise kann vom
    // Clip abweichen (relativ/absolut), deshalb den ersten Take ausdrücklich wählen.
    const bool firstTakeList = ! clip->hasAnyTakes();

    if (firstTakeList)
        clip->addTake (clip->getOriginalFile());

    clip->addTake (*imported);

    if (firstTakeList)
        clip->setCurrentTake (0);

    return {};
}

core::Result<void> ClipService::setShowingTakes (const juce::String& clipId, bool shouldShow)
{
    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    clip->setShowingTakes (shouldShow);
    clip->changed();
    return {};
}

core::Result<juce::String>
ClipService::useTakeForRange (const juce::String& clipId, int takeIndex, double fromSeconds, double toSeconds)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    if (takeIndex < 0 || takeIndex >= clip->getNumTakes (true))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Take does not exist.").toStdString());

    auto* track = clip->getClipTrack();

    if (track == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("The clip is not on any track.").toStdString());

    const auto clipRange = clip->getEditTimeRange();
    const auto from = te::TimePosition::fromSeconds (juce::jmax (fromSeconds, clipRange.getStart().inSeconds()));
    const auto to = te::TimePosition::fromSeconds (juce::jmin (toSeconds, clipRange.getEnd().inSeconds()));

    if (to <= from)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The range is not inside the clip.").toStdString());

    projectService.beginUndoStep (TRANS ("Take for range"));

    // Erst hinten, dann vorne schneiden: Nach dem Schnitt vorne ist "clip" das linke Stück,
    // das rechte (unser Mittelstück) kommt vom Schnitt zurück.
    te::Clip* middle = clip;

    if (to < clipRange.getEnd())
        track->splitClip (*clip, to);

    if (from > clipRange.getStart())
        middle = track->splitClip (*clip, from);

    if (middle == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not split the clip.").toStdString());

    middle->setCurrentTake (takeIndex);

    // Mittelstück an den Schnittkanten etwas in die Nachbarn ragen lassen: Die Überlappung wird mit
    // automatischen Crossfades (Projekteinstellung) zu einer kurzen Überblendung statt eines Knacksers.
    constexpr auto compOverlap = te::TimeDuration::fromSeconds (0.01);
    const auto middleRange = middle->getPosition();

    if (from > clipRange.getStart() && middleRange.getOffset() >= compOverlap)
        middle->setStart (middleRange.getStart() - compOverlap, true, false);

    if (to < clipRange.getEnd())
        middle->setEnd (middle->getPosition().getEnd() + compOverlap, true);

    return middle->itemID.toString();
}

core::Result<void> ClipService::unpackTakes (const juce::String& clipId)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = findClip (clipId);

    if (clip == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Clip not found.").toStdString());

    if (clip->getNumTakes (true) < 2)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The clip has no takes.").toStdString());

    projectService.beginUndoStep (TRANS ("Distribute takes"));

    // Tracktion greift beim Verteilen auf den Comp-Manager zu, ohne ihn anzulegen.
    if (auto* audioClip = dynamic_cast<te::WaveAudioClip*> (clip))
        audioClip->getCompManager();
    else if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
        midiClip->getCompManager();

    clip->unpackTakes (false);
    return {};
}

core::Result<void> ClipService::deleteUnusedTakes (const juce::String& clipId)
{
    if (auto unlocked = checkUnlocked ({ clipId }); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* clip = dynamic_cast<te::WaveAudioClip*> (findClip (clipId));

    if (clip == nullptr)
        return core::fail (core::ErrorCode::invalidArgument,
                           TRANS ("Deleting unused takes is only available for audio clips.").toStdString());

    projectService.beginUndoStep (TRANS ("Clean up takes"));
    clip->deleteAllUnusedTakes (false);
    return {};
}

core::Result<juce::File> ClipService::ensureInProject (const juce::File& audioFile) const
{
    const auto projectFolder = projectService.getProjectFolder();

    if (projectFolder == juce::File() || audioFile.isAChildOf (projectFolder))
    {
        projectService.registerProjectFile (audioFile);
        return audioFile;
    }

    // Wie der Pool in Cubase: Das Projekt enthält alle seine Dateien, ein Ordner lässt sich
    // komplett sichern oder weitergeben. Tracktion kennt Takes nur für Dateien im Projekt.
    const auto audioFolder = projectFolder.getChildFile ("Audio");
    audioFolder.createDirectory();
    const auto target = audioFolder.getChildFile (audioFile.getFileName()).getNonexistentSibling (false);

    if (! audioFile.copyFileTo (target))
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not copy the file into the project: {path}"), { { "path", audioFile.getFullPathName() } }).toStdString());

    projectService.registerProjectFile (target);
    return target;
}

core::Result<void> ClipService::checkUnlocked (const std::vector<juce::String>& clipIds) const
{
    for (const auto& clipId : clipIds)
        if (auto* clip = findClip (clipId))
            if (auto* track = dynamic_cast<te::AudioTrack*> (clip->getClipTrack()))
                if (auto unlocked = bakeLock::checkClipsUnlocked (*track); ! unlocked)
                    return unlocked;

    return {};
}

te::Clip* ClipService::findClip (const juce::String& clipId) const
{
    if (auto* edit = projectService.getEdit())
        return te::findClipForID (*edit, te::EditItemID::fromString (clipId));

    return nullptr;
}

} // namespace fk::services
