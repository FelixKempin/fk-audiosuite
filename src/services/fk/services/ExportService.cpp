#include "fk/services/ExportService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>
#include <memory>

namespace fk::services
{

namespace
{
constexpr int ticksPerQuarterNote = 960;
constexpr int oggDefaultQuality = 6;  // 192 kbit/s
constexpr int mp3DefaultQuality = 23; // 320 kbit/s CBR

te::RenderFormat renderFormatFor (ExportFormat format)
{
    switch (format)
    {
        case ExportFormat::wav:
            return te::RenderFormat::wav;
        case ExportFormat::flac:
            return te::RenderFormat::flac;
        case ExportFormat::aiff:
            return te::RenderFormat::aiff;
        case ExportFormat::ogg:
            return te::RenderFormat::ogg;
        case ExportFormat::mp3:
            return te::RenderFormat::mp3;
    }

    return te::RenderFormat::wav;
}

/// Die Qualitätsnamen der Encoder sind englisch; für die Anzeige in der Sprache der Oberfläche.
juce::String localisedQualityName (juce::String name)
{
    return name.replace ("Kb/s", "kbit/s")
        .replace ("kbps", "kbit/s")
        .replace ("VBR quality", TRANS ("VBR quality"))
        .replace ("(best)", TRANS ("(best)"))
        .replace ("(smallest)", TRANS ("(smallest)"));
}

/// Viertelnoten vom Projektanfang bis zu einer Schlagposition. Ein Schlag ist in Tracktion der
/// Nenner der Taktart (in 6/8 eine Achtel), eine MIDI-Datei zählt dagegen in Viertelnoten.
double quarterNotesAt (const te::TempoSequence& tempo, te::BeatPosition beat)
{
    double quarters = 0.0;
    const int count = tempo.getNumTimeSigs();

    for (int i = 0; i < count; ++i)
    {
        auto* signature = tempo.getTimeSig (i);
        const double from = signature->getStartBeat().inBeats();
        const double to = i + 1 < count ? tempo.getTimeSig (i + 1)->getStartBeat().inBeats() : beat.inBeats();
        const double end = std::min (to, beat.inBeats());

        if (end > from)
            quarters += (end - from) * 4.0 / signature->denominator.get();
    }

    return quarters;
}

int ticksAt (const te::TempoSequence& tempo, te::BeatPosition beat)
{
    return juce::roundToInt (quarterNotesAt (tempo, beat) * ticksPerQuarterNote);
}

/// Tempospur der MIDI-Datei. bpmCountsQuarters: Tracktion zählt bpm je nach Engine-Einstellung in Vierteln
/// oder in Noten des Taktart-Nenners (lengthOfOneBeatDependsOnTimeSignature).
juce::MidiMessageSequence
createTempoTrack (const te::TempoSequence& tempo, const juce::String& name, bool bpmCountsQuarters)
{
    juce::MidiMessageSequence sequence;
    sequence.addEvent (juce::MidiMessage::textMetaEvent (3, name));

    for (int i = 0; i < tempo.getNumTimeSigs(); ++i)
    {
        auto* signature = tempo.getTimeSig (i);
        auto event =
            juce::MidiMessage::timeSignatureMetaEvent (signature->numerator.get(), signature->denominator.get());
        event.setTimeStamp (ticksAt (tempo, signature->getStartBeat()));
        sequence.addEvent (event);
    }

    // Tempo-Rampen werden als Stufe am Startpunkt geschrieben.
    for (int i = 0; i < tempo.getNumTempos(); ++i)
    {
        auto* setting = tempo.getTempo (i);
        const auto denominator = tempo.getTimeSigAt (setting->getStartBeat()).denominator.get();
        const double secondsPerQuarter =
            bpmCountsQuarters ? 60.0 / setting->getBpm() : 60.0 / setting->getBpm() * denominator / 4.0;
        auto event = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (secondsPerQuarter * 1.0e6));
        event.setTimeStamp (ticksAt (tempo, setting->getStartBeat()));
        sequence.addEvent (event);
    }

    sequence.sort();
    return sequence;
}

/// Gemeinsamer Teil von Mixdown und Stems; Ziel und Spuren setzt der Aufrufer.
te::RenderSpecification specificationFor (te::Edit& edit, const ExportOptions& options, te::TimeRange range)
{
    te::RenderSpecification spec;
    spec.time = range;
    spec.format = renderFormatFor (options.format);
    spec.sampleRate = options.sampleRate > 0.0 ? options.sampleRate : edit.engine.getDeviceManager().getSampleRate();
    spec.bitDepth = options.bitDepth;
    spec.quality = options.quality >= 0 ? options.quality : ExportService::getDefaultQuality (options.format);
    spec.dither = options.dither && options.bitDepth < 32;
    spec.includeTails = options.includeTails;
    spec.normalise = options.normalise == NormaliseMode::peak;
    spec.normaliseByLUFS = options.normalise == NormaliseMode::loudness;
    spec.normaliseToLevelDb = options.normaliseTargetDb;
    spec.usePlugins = true;
    spec.channelLayout = "stereo";
    return spec;
}
} // namespace

ExportService::ExportService (ProjectService& service)
    : projectService (service)
{
}

core::Result<te::TimeRange> ExportService::rangeFor (te::Edit& edit, ExportRange range) const
{
    if (range == ExportRange::loopRange)
    {
        const auto loop = edit.getTransport().getLoopRange();

        if (loop.getLength().inSeconds() <= 0.0)
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("No loop range is set.").toStdString());

        return loop;
    }

    const auto length = edit.getLength();

    if (length.inSeconds() <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project contains no clips yet.").toStdString());

    return te::TimeRange (te::TimePosition(), te::TimePosition() + length);
}

core::Result<juce::File> ExportService::exportMixdown (const ExportOptions& options,
                                                       const std::function<void (float)>& progress)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (options.file == juce::File())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No target file specified.").toStdString());

    if (! isFormatAvailable (options.format))
        return core::fail (core::ErrorCode::unsupported,
                           TRANS ("MP3 needs the LAME library (package libmp3lame0), which is not installed.").toStdString());

    const auto range = rangeFor (*edit, options.range);

    if (! range)
        return std::unexpected (range.error());

    auto spec = specificationFor (*edit, options, *range);
    // Zieldatei immer mit passender Endung, sonst erkennt niemand das Format.
    spec.destination = options.file.withFileExtension (extensionFor (options.format));
    spec.useMasterPlugins = true;

    if (const auto valid = te::validateRenderSpecification (*edit, spec); valid.failed())
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("Export not possible: {error}"), { { "error", valid.getErrorMessage() } }).toStdString());

    auto job = te::createRenderJob (*edit, spec);

    if (! job)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the render task.").toStdString());

    return render (std::move (*job), progress);
}

core::Result<std::vector<juce::File>> ExportService::exportStems (const ExportOptions& options,
                                                                 const std::vector<juce::String>& trackIds,
                                                                 const std::function<void (float)>& progress)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (trackIds.empty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No tracks selected.").toStdString());

    if (options.file == juce::File())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No target folder specified.").toStdString());

    if (! isFormatAvailable (options.format))
        return core::fail (core::ErrorCode::unsupported,
                           TRANS ("MP3 needs the LAME library (package libmp3lame0), which is not installed.").toStdString());

    const auto range = rangeFor (*edit, options.range);

    if (! range)
        return std::unexpected (range.error());

    auto base = specificationFor (*edit, options, *range);
    base.useMasterPlugins = false; // Stems gehen in einen fremden Mix, der Master-Bus gehört nicht dazu
    base.includeSourceTracks = true;

    for (const auto& id : trackIds)
    {
        const auto itemId = te::EditItemID::fromVar (id);

        if (te::findTrackForID (*edit, itemId) == nullptr)
            return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("Track not found: {id}"), { { "id", id } }).toStdString());

        base.tracks.add (itemId);
    }

    const auto folder = options.file;

    if (const auto created = folder.createDirectory(); created.failed())
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not create the folder: {error}"), { { "error", created.getErrorMessage() } }).toStdString());

    const auto specs = te::createPerTrackSpecifications (*edit, base, folder);
    std::vector<juce::File> files;

    for (std::size_t i = 0; i < specs.size(); ++i)
    {
        if (const auto valid = te::validateRenderSpecification (*edit, specs[i]); valid.failed())
            return core::fail (core::ErrorCode::invalidArgument,
                               plugins::i18n::format (TRANS ("Stem not possible: {error}"), { { "error", valid.getErrorMessage() } }).toStdString());

        auto job = te::createRenderJob (*edit, specs[i]);

        if (! job)
            return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the render task.").toStdString());

        const auto done = static_cast<float> (i);
        const auto total = static_cast<float> (specs.size());
        const auto file = render (std::move (*job),
                                  [&progress, done, total] (float p)
                                  {
                                      if (progress)
                                          progress ((done + p) / total);
                                  });

        if (! file)
            return std::unexpected (file.error());

        files.push_back (*file);
    }

    if (progress)
        progress (1.0f);

    return files;
}

core::Result<juce::File> ExportService::render (te::PlannedRenderJob job, const std::function<void (float)>& progress)
{
    auto& edit = *job.params.edit;
    const auto file = job.params.destFile;
    file.getParentDirectory().createDirectory();
    file.deleteFile(); // ein Datei-Stream hängt sonst an eine vorhandene Datei an

    // Wie Tracktions eigener Export: Wiedergabe anhalten und das Edit als "wird gerendert" markieren.
    te::TransportControl::stopAllTransports (edit.engine, false, true);
    const te::Edit::ScopedRenderStatus renderStatus (edit, true);

    // Quellspuren (Sidechain, Sends) laufen stumm mit. Stummschalten ändert das Edit und gehört
    // deshalb auf den Message-Thread; callSync ruft direkt, wenn wir schon dort sind (Tests).
    std::unique_ptr<te::ScopedTrackMuter> muter;

    if (! job.tracksToMute.isEmpty())
        juce::MessageManager::callSync ([&]
                                        { muter = std::make_unique<te::ScopedTrackMuter> (edit, job.tracksToMute); });

    auto task = te::render_utils::createRenderTask (job.params, job.name, nullptr, nullptr);

    if (task == nullptr)
    {
        juce::MessageManager::callSync ([&] { muter.reset(); });
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the render task.").toStdString());
    }

    // Den Job selbst durchlaufen lassen, statt Tracktions Fortschrittsfenster: So funktioniert
    // der Export auch ohne Oberfläche (Tests, Kommandozeile), und die Oberfläche zeigt ihren eigenen Balken.
    while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        if (progress)
            progress (task->getCurrentTaskProgress());

    juce::MessageManager::callSync ([&] { muter.reset(); });

    if (progress)
        progress (1.0f);

    if (task->errorMessage.isNotEmpty())
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Export failed: {error}"), { { "error", task->errorMessage } }).toStdString());

    if (! file.existsAsFile())
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Export did not produce a file: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    return file;
}

core::Result<juce::File> ExportService::exportMidi (const juce::File& file)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (file == juce::File())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No target file specified.").toStdString());

    const auto& tempo = edit->tempoSequence;
    const auto target = file.withFileExtension (".mid");

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarterNote);
    midiFile.addTrack (
        createTempoTrack (tempo,
                          target.getFileNameWithoutExtension(),
                          edit->engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature()));

    for (auto* track : te::getAudioTracks (*edit))
    {
        juce::MidiMessageSequence sequence;

        for (auto* clip : track->getClips())
        {
            auto* midiClip = dynamic_cast<te::MidiClip*> (clip);

            if (midiClip == nullptr || midiClip->isMuted())
                continue;

            // Wiedergabe-Folge in Projekt-Schlägen: berücksichtigt Clip-Grenzen, Loop und Groove.
            const auto played = midiClip->getSequence().exportToPlaybackMidiSequence (
                *midiClip, te::MidiList::TimeBase::beats, false);

            for (const auto* event : played)
            {
                auto message = event->message;
                message.setTimeStamp (
                    ticksAt (tempo, te::BeatPosition::fromBeats (event->message.getTimeStamp())));
                sequence.addEvent (message);
            }
        }

        if (sequence.getNumEvents() == 0)
            continue;

        sequence.addEvent (juce::MidiMessage::textMetaEvent (3, track->getName()));
        sequence.sort();
        sequence.updateMatchedPairs();
        midiFile.addTrack (sequence);
    }

    if (midiFile.getNumTracks() < 2)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project contains no MIDI clips.").toStdString());

    target.getParentDirectory().createDirectory();
    target.deleteFile();
    juce::FileOutputStream out (target);

    if (out.failedToOpen() || ! midiFile.writeTo (out))
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not write the MIDI file: {path}"), { { "path", target.getFullPathName() } }).toStdString());

    return target;
}

juce::AudioFormat* ExportService::findFormat (ExportFormat format) const
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return nullptr;

    auto& formats = edit->engine.getAudioFileFormatManager();

    switch (format)
    {
        case ExportFormat::wav:
            return formats.getWavFormat();
        case ExportFormat::flac:
            return formats.getFlacFormat();
        case ExportFormat::aiff:
            return formats.getAiffFormat();
        case ExportFormat::ogg:
            return formats.getOggFormat();
        case ExportFormat::mp3:
            // LAME wird erst bei Bedarf angemeldet: Tracktion lädt libmp3lame zur Laufzeit, falls vorhanden.
            te::LAMEManager::registerAudioFormat (formats);
            return formats.getLameFormat();
    }

    return nullptr;
}

bool ExportService::isFormatAvailable (ExportFormat format) const
{
    return findFormat (format) != nullptr;
}

std::vector<QualityChoice> ExportService::getQualityChoices (ExportFormat format) const
{
    std::vector<QualityChoice> choices;

    if (isLossless (format))
        return choices;

    if (auto* audioFormat = findFormat (format))
    {
        const auto names = audioFormat->getQualityOptions();

        for (int i = 0; i < names.size(); ++i)
            choices.push_back ({ localisedQualityName (names[i]), i });
    }

    return choices;
}

int ExportService::getDefaultQuality (ExportFormat format)
{
    switch (format)
    {
        case ExportFormat::ogg:
            return oggDefaultQuality;
        case ExportFormat::mp3:
            return mp3DefaultQuality;
        case ExportFormat::wav:
        case ExportFormat::flac:
        case ExportFormat::aiff:
            break;
    }

    return 0;
}

juce::String ExportService::extensionFor (ExportFormat format)
{
    switch (format)
    {
        case ExportFormat::wav:
            return ".wav";
        case ExportFormat::flac:
            return ".flac";
        case ExportFormat::aiff:
            return ".aiff";
        case ExportFormat::ogg:
            return ".ogg";
        case ExportFormat::mp3:
            return ".mp3";
    }

    return ".wav";
}

bool ExportService::isLossless (ExportFormat format)
{
    return format != ExportFormat::ogg && format != ExportFormat::mp3;
}

} // namespace fk::services
