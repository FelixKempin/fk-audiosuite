#include "fk/engine/ExpressionMap.h"

#include <algorithm>
#include <map>

namespace fk::engine
{

namespace
{
constexpr double eventLead = 0.0005; ///< Keyswitch und CC so viel vor der Note (Schläge oder Sekunden)
} // namespace

const Articulation* ExpressionMap::find (const juce::String& articulationId) const
{
    for (const auto& articulation : articulations)
        if (articulation.id == articulationId)
            return &articulation;

    return nullptr;
}

juce::String ExpressionMap::toJson() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("id", id);
    root->setProperty ("name", name);
    juce::Array<juce::var> list;

    for (const auto& articulation : articulations)
    {
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty ("id", articulation.id);
        entry->setProperty ("name", articulation.name);
        entry->setProperty ("symbol", articulation.symbol);

        auto output = std::make_unique<juce::DynamicObject>();

        if (articulation.output.channel > 0)
            output->setProperty ("channel", articulation.output.channel);

        if (articulation.output.keyswitch >= 0)
            output->setProperty ("keyswitch", articulation.output.keyswitch);

        if (articulation.output.cc >= 0)
        {
            output->setProperty ("cc", articulation.output.cc);
            output->setProperty ("ccValue", articulation.output.ccValue);
        }

        entry->setProperty ("output", juce::var (output.release()));
        list.add (juce::var (entry.release()));
    }

    root->setProperty ("articulations", list);
    return juce::JSON::toString (juce::var (root.release()));
}

std::optional<ExpressionMap> ExpressionMap::fromJson (const juce::String& json)
{
    const auto parsed = juce::JSON::parse (json);
    const auto* list = parsed.getProperty ("articulations", {}).getArray();

    if (! parsed.isObject() || list == nullptr)
        return std::nullopt;

    ExpressionMap map;
    map.id = parsed.getProperty ("id", {}).toString();
    map.name = parsed.getProperty ("name", "Expression Map").toString();

    for (const auto& entry : *list)
    {
        Articulation articulation;
        articulation.id = entry.getProperty ("id", {}).toString();
        articulation.name = entry.getProperty ("name", articulation.id).toString();
        articulation.symbol = entry.getProperty ("symbol", articulation.name.substring (0, 3)).toString();
        const auto output = entry.getProperty ("output", {});
        articulation.output.channel = std::clamp (static_cast<int> (output.getProperty ("channel", 0)), 0, 16);
        articulation.output.keyswitch = std::clamp (static_cast<int> (output.getProperty ("keyswitch", -1)), -1, 127);
        articulation.output.cc = std::clamp (static_cast<int> (output.getProperty ("cc", -1)), -1, 127);
        articulation.output.ccValue = std::clamp (static_cast<int> (output.getProperty ("ccValue", 0)), 0, 127);

        if (articulation.id.isEmpty())
            return std::nullopt;

        map.articulations.push_back (articulation);
    }

    return map;
}

juce::MidiMessageSequence createSequenceWithArticulations (const te::MidiList& list,
                                                           te::MidiClip& clip,
                                                           te::MidiList::TimeBase timeBase,
                                                           bool generateMPE)
{
    auto* track = clip.getAudioTrack();
    const auto json = track != nullptr ? track->state.getProperty (expressionMapProperty).toString() : juce::String();

    // Schneller Weg: keine Map oder keine Note mit Spielweise.
    const bool anyArticulation = std::ranges::any_of (list.getNotes(), [] (const te::MidiNote* note)
                                                      { return note->state.hasProperty (noteArticulationProperty); });

    if (json.isEmpty() || ! anyArticulation || generateMPE)
        return te::MidiList::createDefaultPlaybackMidiSequence (list, clip, timeBase, generateMPE);

    const auto map = ExpressionMap::fromJson (json);

    if (! map)
        return te::MidiList::createDefaultPlaybackMidiSequence (list, clip, timeBase, generateMPE);

    // Noten nach Übertragung gruppieren; Noten ohne (oder mit unbekannter) Spielweise in die Standardgruppe.
    std::map<ArticulationOutput, std::vector<const te::MidiNote*>> groups;

    for (const auto* note : list.getNotes())
    {
        const auto* articulation = map->find (note->state.getProperty (noteArticulationProperty).toString());
        groups[articulation != nullptr ? articulation->output : ArticulationOutput {}].push_back (note);
    }

    const auto clipChannel = list.getMidiChannel().getChannelNumber();
    juce::MidiMessageSequence result;
    bool controllersDone = false;

    for (const auto& [output, notes] : groups)
    {
        // Eigene Liste je Gruppe: Tracktion rechnet Lage, Loop, Quantisierung und Groove wie gewohnt.
        te::MidiList group;
        group.setMidiChannel (te::MidiChannel (output.channel > 0 ? output.channel : clipChannel));

        for (const auto* note : notes)
            group.addNote (note->getNoteNumber(), note->getStartBeat(), note->getLengthBeats(), note->getVelocity(),
                           note->getColour(), nullptr);

        // Controller und SysEx des Clips nur einmal (mit der ersten Gruppe).
        if (! controllersDone)
        {
            for (const auto* controller : list.getControllerEvents())
                group.addControllerEvent (*controller, nullptr);

            for (const auto* sysex : list.getSysexEvents())
                group.addSysExEvent (sysex->getMessage(), sysex->getBeatPosition(), nullptr);

            controllersDone = true;
        }

        const auto sequence = te::MidiList::createDefaultPlaybackMidiSequence (group, clip, timeBase, false);
        const auto channel = output.channel > 0 ? output.channel : clipChannel;

        // Keyswitch und CC vor jede Note dieser Gruppe, dann die Gruppe selbst.
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto& message = sequence.getEventPointer (i)->message;

            if (! message.isNoteOn())
                continue;

            const double time = std::max (0.0, message.getTimeStamp() - eventLead);

            if (output.cc >= 0)
                result.addEvent (juce::MidiMessage::controllerEvent (channel, output.cc, output.ccValue), time);

            if (output.keyswitch >= 0)
            {
                result.addEvent (juce::MidiMessage::noteOn (channel, output.keyswitch, static_cast<juce::uint8> (100)), time);
                result.addEvent (juce::MidiMessage::noteOff (channel, output.keyswitch), time);
            }
        }

        result.addSequence (sequence, 0.0);
    }

    result.updateMatchedPairs();
    return result;
}

} // namespace fk::engine
