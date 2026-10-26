#include "fk/services/MidiLearnService.h"
#include "fk/plugins/i18n/Text.h"

namespace fk::services
{

MidiLearnService::MidiLearnService (engine::EngineContext& context, ProjectService& service)
    : te::MidiLearnState::Listener (context.getEngine().getMidiLearnState())
    , engineContext (context)
    , projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

MidiLearnService::~MidiLearnService()
{
    if (auto* edit = projectService.getEdit())
        edit->getParameterChangeHandler().removeChangeListener (this);

    setLearnActive (false);
    projectService.removeListener (*this);
}

void MidiLearnService::setLearnActive (bool shouldBeActive)
{
    auto& state = engineContext.getEngine().getMidiLearnState();

    if (state.isActive() != shouldBeActive)
        state.setActive (shouldBeActive); // ruft midiLearnStatusChanged
}

bool MidiLearnService::isLearnActive() const
{
    return engineContext.getEngine().getMidiLearnState().isActive();
}

juce::String MidiLearnService::getLearnHint() const
{
    if (! isLearnActive())
        return {};

    if (auto* edit = projectService.getEdit())
        if (edit->getParameterChangeHandler().isParameterPending())
            return TRANS ("Now move the hardware knob or pad");

    return TRANS ("Touch a control in the DAW (plugin or mixer)");
}

std::vector<MidiMappingInfo> MidiLearnService::getMappings() const
{
    std::vector<MidiMappingInfo> result;
    auto* mappings = getMappingsOfEdit();

    if (mappings == nullptr)
        return result;

    for (int i = 0; i < mappings->getNumControllerIDs(); ++i)
    {
        const auto mapping = mappings->getMappingForRow (i);

        if (mapping.parameter == nullptr || mapping.controllerID <= 0)
            continue;

        result.push_back (
            { i, mapping.parameter->getFullName(), describeController (mapping.controllerID, mapping.channelID) });
    }

    return result;
}

core::Result<void> MidiLearnService::removeMapping (int index)
{
    auto* mappings = getMappingsOfEdit();

    if (mappings == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (index < 0 || index >= mappings->getNumControllerIDs())
        return core::fail (core::ErrorCode::notFound, TRANS ("Mapping not found.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove MIDI mapping"));
    mappings->removeMapping (index);
    mappings->saveToEdit();
    listeners.call (&MidiLearnService::Listener::midiLearnChanged);
    return {};
}

void MidiLearnService::removeAllMappings()
{
    auto* mappings = getMappingsOfEdit();

    if (mappings == nullptr)
        return;

    projectService.beginUndoStep (TRANS ("Remove all MIDI mappings"));

    for (int i = mappings->getNumControllerIDs(); --i >= 0;)
        mappings->removeMapping (i);

    mappings->saveToEdit();
    listeners.call (&MidiLearnService::Listener::midiLearnChanged);
}

void MidiLearnService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void MidiLearnService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void MidiLearnService::projectChanged()
{
    // Der Lernmodus endet mit dem Projekt; der ParameterChangeHandler des neuen Edits meldet
    // uns, wenn ein Parameter angefasst wurde (für den Hinweistext).
    setLearnActive (false);

    if (auto* edit = projectService.getEdit())
        edit->getParameterChangeHandler().addChangeListener (this);

    listeners.call (&MidiLearnService::Listener::midiLearnChanged);
}

void MidiLearnService::midiLearnStatusChanged (bool)
{
    listeners.call (&MidiLearnService::Listener::midiLearnChanged);
}

void MidiLearnService::midiLearnAssignmentChanged (te::MidiLearnState::ChangeType)
{
    // Tracktion hat eine Zuordnung gelernt: ins Projekt schreiben, damit Speichern sie mitnimmt.
    if (auto* mappings = getMappingsOfEdit())
        mappings->saveToEdit();

    listeners.call (&MidiLearnService::Listener::midiLearnChanged);
}

void MidiLearnService::changeListenerCallback (juce::ChangeBroadcaster*)
{
    listeners.call (&MidiLearnService::Listener::midiLearnChanged); // Parameter angefasst: Hinweis wechselt
}

te::ParameterControlMappings* MidiLearnService::getMappingsOfEdit() const
{
    if (auto* edit = const_cast<te::Edit*> (projectService.getEdit()))
        return &edit->getParameterControlMappings();

    return nullptr;
}

juce::String MidiLearnService::describeController (int controllerId, int channel)
{
    // Tracktions Kodierung: 0x10000 + CC-Nummer, 0x20000 NRPN, 0x30000 RPN, 0x40000 Kanaldruck.
    const auto channelText = juce::String (channel);

    if (controllerId >= 0x40000)
        return plugins::i18n::format (TRANS ("Aftertouch (channel {channel})"), { { "channel", channelText } });

    if (controllerId >= 0x30000)
        return plugins::i18n::format (TRANS ("RPN {number} (channel {channel})"),
                                      { { "number", juce::String (controllerId & 0x7fff) }, { "channel", channelText } });

    if (controllerId >= 0x20000)
        return plugins::i18n::format (TRANS ("NRPN {number} (channel {channel})"),
                                      { { "number", juce::String (controllerId & 0x7fff) }, { "channel", channelText } });

    if (controllerId >= 0x10000)
    {
        const auto number = juce::String (controllerId & 0x7f);
        const auto* name = juce::MidiMessage::getControllerName (controllerId & 0x7f);

        if (name == nullptr)
            return plugins::i18n::format (TRANS ("CC {number} (channel {channel})"),
                                          { { "number", number }, { "channel", channelText } });

        return plugins::i18n::format (TRANS ("CC {number} {name} (channel {channel})"),
                                      { { "number", number }, { "name", juce::String (name) }, { "channel", channelText } });
    }

    return plugins::i18n::format (TRANS ("Controller {number} (channel {channel})"),
                                  { { "number", juce::String (controllerId) }, { "channel", channelText } });
}

} // namespace fk::services
