#include "fk/services/TimelineService.h"

#include <algorithm>

#include <cmath>

namespace fk::services
{

namespace
{
constexpr int ticksPerBeat = 480;
constexpr double minTempoBpm = 20.0;
constexpr double maxTempoBpm = 400.0;
constexpr auto zoomPresetsItem = "fkZoomPresets"; // JSON-Liste in den Engine-Einstellungen
} // namespace

TimelineService::TimelineService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

TimelineService::~TimelineService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

double TimelineService::getTempoBpm() const
{
    if (const auto* edit = projectService.getEdit())
        if (auto* tempo = edit->tempoSequence.getTempo (0))
            return tempo->getBpm();

    return 120.0;
}

void TimelineService::setTempoBpm (double bpm)
{
    projectService.beginUndoStep (TRANS ("Change tempo"));
    if (auto* edit = projectService.getEdit())
        if (auto* tempo = edit->tempoSequence.getTempo (0))
            tempo->setBpm (juce::jlimit (minTempoBpm, maxTempoBpm, bpm));
}

TimeSignature TimelineService::getTimeSignature() const
{
    if (const auto* edit = projectService.getEdit())
        if (auto* timeSig = edit->tempoSequence.getTimeSig (0))
            return { timeSig->numerator.get(), timeSig->denominator.get() };

    return {};
}

void TimelineService::setTimeSignature (TimeSignature signature)
{
    projectService.beginUndoStep (TRANS ("Change time signature"));
    if (signature.numerator < 1 || signature.numerator > 32)
        return;

    if (signature.denominator != 2 && signature.denominator != 4 && signature.denominator != 8 &&
        signature.denominator != 16)
        return;

    if (auto* edit = projectService.getEdit())
    {
        if (auto* timeSig = edit->tempoSequence.getTimeSig (0))
        {
            timeSig->numerator = signature.numerator;
            timeSig->denominator = signature.denominator;
        }
    }
}

juce::String TimelineService::formatBarsBeats (double seconds) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return "001.01.000";

    const auto barsAndBeats = edit->tempoSequence.toBarsAndBeats (te::TimePosition::fromSeconds (seconds));
    const auto beatsInBar = barsAndBeats.beats.inBeats();
    const auto wholeBeats = static_cast<int> (std::floor (beatsInBar));
    const auto ticks = static_cast<int> (std::floor ((beatsInBar - wholeBeats) * ticksPerBeat));

    return juce::String::formatted ("%03d.%02d.%03d", barsAndBeats.bars + 1, wholeBeats + 1, ticks);
}

double TimelineService::secondsToBeats (double seconds) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return seconds * 2.0; // 120 BPM

    return edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds))).inBeats();
}

double TimelineService::beatsToSeconds (double beats) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return beats / 2.0; // 120 BPM

    return edit->tempoSequence.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, beats))).inSeconds();
}

std::optional<double> TimelineService::parseBarsBeats (const juce::String& text) const
{
    juce::StringArray parts;
    parts.addTokens (text.trim(), ".:", {});
    parts.removeEmptyStrings();

    if (parts.isEmpty() || parts.size() > 3)
        return std::nullopt;

    for (const auto& part : parts)
        if (! part.containsOnly ("0123456789"))
            return std::nullopt;

    const int bar = parts[0].getIntValue();
    const int beat = parts.size() > 1 ? parts[1].getIntValue() : 1;
    const int tick = parts.size() > 2 ? parts[2].getIntValue() : 0;

    if (bar < 1 || beat < 1 || tick < 0 || tick >= ticksPerBeat)
        return std::nullopt;

    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return ((bar - 1) * 4 + (beat - 1) + tick / static_cast<double> (ticksPerBeat)) / 2.0;

    te::tempo::BarsAndBeats position;
    position.bars = bar - 1;
    position.beats = te::BeatDuration::fromBeats ((beat - 1) + tick / static_cast<double> (ticksPerBeat));
    return edit->tempoSequence.toTime (position).inSeconds();
}

double TimelineService::barLengthSeconds (double atSeconds) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return 2.0; // 120 BPM, 4/4

    // Taktart an dieser Position, nicht die des Projektanfangs: Taktartwechsel (Signaturspur)
    // ändern die Taktlänge ab ihrem Takt.
    const auto& tempo = edit->tempoSequence;
    const auto position = te::TimePosition::fromSeconds (juce::jmax (0.0, atSeconds));
    const auto& signature = tempo.getTimeSigAt (position);
    const double beatsPerBar = 4.0 / signature.denominator.get() * signature.numerator.get();
    const auto startBeats = tempo.toBeats (position);
    const auto endBeats = startBeats + te::BeatDuration::fromBeats (beatsPerBar);
    return (tempo.toTime (endBeats) - tempo.toTime (startBeats)).inSeconds();
}

juce::String GridResolution::shortName() const
{
    switch (kind)
    {
        case Kind::bar:
            return TRANS ("Bar");
        case Kind::beat:
            return TRANS ("Beat");
        case Kind::note:
            return note.shortName();
        case Kind::seconds:
            return TRANS ("Seconds");
        case Kind::samples:
            return TRANS ("Samples");
    }

    return {};
}

double TimelineService::deviceSampleRate() const
{
    const auto* edit = projectService.getEdit();
    const auto rate = edit != nullptr ? edit->engine.getDeviceManager().getSampleRate() : 0.0;
    return rate > 0.0 ? rate : 48000.0;
}

double TimelineService::gridStepSeconds (double atSeconds, GridResolution resolution) const
{
    if (resolution.kind == GridResolution::Kind::seconds)
        return 1.0;

    if (resolution.kind == GridResolution::Kind::samples)
        return 1.0 / deviceSampleRate();

    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return 0.0;

    const auto beats = edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (atSeconds)).inBeats();
    const auto step = resolutionInQuarterNotes (resolution);
    return edit->tempoSequence.toTime (te::BeatPosition::fromBeats (beats + step)).inSeconds() - atSeconds;
}

double TimelineService::snapSeconds (double seconds, GridResolution resolution) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return seconds;

    if (resolution.kind == GridResolution::Kind::seconds)
        return juce::jmax (0.0, std::round (seconds));

    if (resolution.kind == GridResolution::Kind::samples)
    {
        const auto rate = deviceSampleRate();
        return juce::jmax (0.0, std::round (seconds * rate) / rate);
    }

    const auto step = resolutionInQuarterNotes (resolution);
    const auto beats = edit->tempoSequence.toBeats (te::TimePosition::fromSeconds (seconds)).inBeats();
    const auto snappedBeats = std::round (beats / step) * step;

    return juce::jmax (0.0, edit->tempoSequence.toTime (te::BeatPosition::fromBeats (snappedBeats)).inSeconds());
}

std::vector<GridLine> TimelineService::getGridLines (double fromSeconds,
                                                     double toSeconds,
                                                     GridResolution resolution,
                                                     double minSpacingSeconds) const
{
    std::vector<GridLine> lines;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr || toSeconds <= fromSeconds)
        return lines;

    // Zeitbasierte Raster rasten auf Sekunden oder Samples ein; gezeichnet werden trotzdem Zählzeiten,
    // damit Lineal und Spuren musikalisch lesbar bleiben.
    if (! resolution.isMusical())
        resolution = GridResolution::beat();

    const auto& tempoSequence = edit->tempoSequence;
    const auto step = resolutionInQuarterNotes (resolution);
    const auto firstBeat =
        std::floor (tempoSequence.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, fromSeconds))).inBeats() /
                    step) *
        step;
    const auto lastBeat = tempoSequence.toBeats (te::TimePosition::fromSeconds (toSeconds)).inBeats();

    for (double beat = firstBeat; beat <= lastBeat; beat += step)
    {
        const auto time = tempoSequence.toTime (te::BeatPosition::fromBeats (beat));
        const auto barsAndBeats = tempoSequence.toBarsAndBeats (time);
        const bool isBarStart = barsAndBeats.beats.inBeats() < 1.0e-6;

        if (! isBarStart && ! lines.empty() && time.inSeconds() - lines.back().seconds < minSpacingSeconds)
            continue;

        lines.push_back ({ time.inSeconds(), isBarStart, barsAndBeats.bars + 1 });
    }

    return lines;
}

double TimelineService::getNextBarSeconds (double seconds) const
{
    const auto bar = barLengthSeconds (seconds);

    for (const auto& line : getGridLines (seconds, seconds + 2.0 * bar + 0.01, GridResolution::bar()))
        if (line.seconds > seconds + 0.001)
            return line.seconds;

    return seconds + bar;
}

double TimelineService::getPreviousBarSeconds (double seconds) const
{
    // Knapp hinter einem Taktanfang (z. B. bei laufendem Transport) zum davor springen.
    constexpr double tolerance = 0.05;
    const auto bar = barLengthSeconds (std::max (seconds - 0.01, 0.0));
    double previous = 0.0;

    for (const auto& line : getGridLines (std::max (seconds - 2.0 * bar - 0.01, 0.0), seconds, GridResolution::bar()))
        if (line.seconds < seconds - tolerance)
            previous = line.seconds;

    return previous;
}

bool TimelineService::isLooping() const
{
    const auto* edit = projectService.getEdit();
    return edit != nullptr && edit->getTransport().looping.get();
}

void TimelineService::setLooping (bool shouldLoop)
{
    if (auto* edit = projectService.getEdit())
        edit->getTransport().looping = shouldLoop;
}

LoopRange TimelineService::getLoopRange() const
{
    if (const auto* edit = projectService.getEdit())
    {
        const auto range = edit->getTransport().getLoopRange();
        return { range.getStart().inSeconds(), range.getEnd().inSeconds() };
    }

    return {};
}

void TimelineService::setLoopRange (LoopRange range)
{
    if (auto* edit = projectService.getEdit())
    {
        const auto start = juce::jmax (0.0, juce::jmin (range.startSeconds, range.endSeconds));
        const auto end = juce::jmax (range.startSeconds, range.endSeconds);
        edit->getTransport().setLoopRange (
            { te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (end) });
    }
}

std::vector<ZoomPreset> TimelineService::getZoomPresets() const
{
    std::vector<ZoomPreset> presets;
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return presets;

    const auto stored = juce::JSON::parse (
        edit->engine.getPropertyStorage().getPropertyItem (te::SettingID::invalid, zoomPresetsItem).toString());

    if (const auto* list = stored.getArray())
        for (const auto& entry : *list)
            if (const auto seconds = static_cast<double> (entry.getProperty ("seconds", 0.0)); seconds > 0.0)
                presets.push_back ({ entry.getProperty ("name", {}).toString(), seconds });

    return presets;
}

core::Result<void> TimelineService::saveZoomPreset (const ZoomPreset& preset)
{
    if (projectService.getEdit() == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (preset.name.trim().isEmpty() || preset.visibleSeconds <= 0.0)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("A zoom preset needs a name and a width.").toStdString());

    auto presets = getZoomPresets();
    std::erase_if (presets, [&preset] (const ZoomPreset& existing) { return existing.name == preset.name.trim(); });
    presets.push_back ({ preset.name.trim(), preset.visibleSeconds });
    std::ranges::sort (presets, {}, &ZoomPreset::visibleSeconds); // von nah nach weit
    storeZoomPresets (presets);
    return {};
}

core::Result<void> TimelineService::removeZoomPreset (const juce::String& name)
{
    auto presets = getZoomPresets();

    if (std::erase_if (presets, [&name] (const ZoomPreset& existing) { return existing.name == name; }) == 0)
        return core::fail (core::ErrorCode::notFound, TRANS ("Zoom preset not found.").toStdString());

    storeZoomPresets (presets);
    return {};
}

void TimelineService::storeZoomPresets (const std::vector<ZoomPreset>& presets)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return;

    juce::Array<juce::var> list;

    for (const auto& preset : presets)
    {
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty ("name", preset.name);
        entry->setProperty ("seconds", preset.visibleSeconds);
        list.add (juce::var (entry.release()));
    }

    auto& storage = edit->engine.getPropertyStorage();
    storage.setPropertyItem (te::SettingID::invalid, zoomPresetsItem, juce::JSON::toString (juce::var (list), true));
    storage.flushSettingsToDisk();
}

bool TimelineService::isMetronomeEnabled() const
{
    const auto* edit = projectService.getEdit();
    return edit != nullptr && edit->clickTrackEnabled.get();
}

void TimelineService::setMetronomeEnabled (bool shouldBeEnabled)
{
    if (auto* edit = projectService.getEdit())
        edit->clickTrackEnabled = shouldBeEnabled;
}

void TimelineService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void TimelineService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void TimelineService::projectChanged()
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

void TimelineService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.timelineChanged(); });
}

void TimelineService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (te::IDs::TEMPO) || tree.hasType (te::IDs::TIMESIG) || tree.hasType (te::IDs::TRANSPORT) ||
        (tree.hasType (te::IDs::CLICKTRACK) && property == te::IDs::active))
        triggerAsyncUpdate();
}

void TimelineService::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::TEMPO) || child.hasType (te::IDs::TIMESIG))
        triggerAsyncUpdate();
}

void TimelineService::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::TEMPO) || child.hasType (te::IDs::TIMESIG))
        triggerAsyncUpdate();
}

double TimelineService::resolutionInQuarterNotes (GridResolution resolution) const
{
    // Tracktion zählt Beats in Viertelnoten. Ein "Schlag" der Taktart ist 4/Nenner Viertel,
    // ein Takt Zähler mal so viel. Bei 6/8 ist ein Schlag also eine halbe Viertelnote.
    const auto signature = getTimeSignature();
    const auto beatInQuarters = 4.0 / signature.denominator;

    switch (resolution.kind)
    {
        case GridResolution::Kind::bar:
            return beatInQuarters * signature.numerator;
        case GridResolution::Kind::note:
            return resolution.note.beats();
        case GridResolution::Kind::beat:
        case GridResolution::Kind::seconds:
        case GridResolution::Kind::samples:
            break;
    }

    return beatInQuarters;
}

} // namespace fk::services
