#include "fk/services/AutomationService.h"

#include "fk/services/TrackProperties.h"

namespace fk::services
{

namespace
{
te::AutomationMode toTracktion (AutomationMode mode)
{
    switch (mode)
    {
        case AutomationMode::read:
            return te::AutomationMode::read;
        case AutomationMode::touch:
            return te::AutomationMode::touch;
        case AutomationMode::latch:
            return te::AutomationMode::latch;
        case AutomationMode::write:
            return te::AutomationMode::write;
    }

    return te::AutomationMode::read;
}

AutomationMode fromTracktion (te::AutomationMode mode)
{
    switch (mode)
    {
        case te::AutomationMode::read:
            return AutomationMode::read;
        case te::AutomationMode::touch:
            return AutomationMode::touch;
        case te::AutomationMode::latch:
            return AutomationMode::latch;
        case te::AutomationMode::write:
            return AutomationMode::write;
    }

    return AutomationMode::read;
}

/// Tracktions englische Namen der Spurparameter in unsere Oberfläche übersetzen.
juce::String displayName (const te::AutomatableParameter& parameter)
{
    if (parameter.paramID == "volume")
        return TRANS ("Volume");

    if (parameter.paramID == "pan")
        return TRANS ("Pan");

    return parameter.paramName;
}
} // namespace

AutomationService::AutomationService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

AutomationService::~AutomationService()
{
    if (observedRecordManager != nullptr)
        observedRecordManager->removeListener (this);

    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

juce::String AutomationService::idFor (const te::AutomatableParameter& parameter)
{
    return parameter.getOwnerID().toString() + "/" + parameter.paramID;
}

AutomationParameterInfo AutomationService::infoFor (te::AutomatableParameter& parameter)
{
    AutomationParameterInfo info;
    info.id = idFor (parameter);
    info.name = displayName (parameter);

    // Spurparameter (Lautstärke, Pan) tragen keinen Besitzer; alles andere nennt sein Plugin.
    if (auto* plugin = parameter.getPlugin();
        plugin != nullptr && dynamic_cast<te::VolumeAndPanPlugin*> (plugin) == nullptr)
        info.ownerName = plugin->getName();

    info.minValue = parameter.getValueRange().getStart();
    info.maxValue = parameter.getValueRange().getEnd();
    info.currentValue = parameter.getCurrentValue();
    info.hasPoints = parameter.hasAutomationPoints();
    return info;
}

te::AutomatableParameter* AutomationService::findParameter (TrackHandle& track, const juce::String& parameterId) const
{
    for (auto* parameter : track.getAllAutomatableParams())
        if (idFor (*parameter) == parameterId)
            return parameter;

    return nullptr;
}

std::vector<AutomationParameterInfo> AutomationService::getParameters (TrackHandle& track) const
{
    std::vector<AutomationParameterInfo> result;

    // Lautstärke und Pan zuerst, damit sie in jeder Liste oben stehen; der Rest in Plugin-Reihenfolge.
    if (auto* volume = track.getVolumePlugin())
        for (auto* parameter : volume->getAutomatableParameters())
            result.push_back (infoFor (*parameter));

    for (auto* parameter : track.getAllAutomatableParams())
        if (dynamic_cast<te::VolumeAndPanPlugin*> (parameter->getPlugin()) == nullptr)
            result.push_back (infoFor (*parameter));

    return result;
}

std::optional<AutomationParameterInfo> AutomationService::getParameter (TrackHandle& track,
                                                                        const juce::String& parameterId) const
{
    if (auto* parameter = findParameter (track, parameterId))
        return infoFor (*parameter);

    return std::nullopt;
}

juce::StringArray AutomationService::readLanes (TrackHandle& track)
{
    // "-" steht für eine Lage ohne Parameter; leere Tokens gingen beim Zerlegen verloren.
    juce::StringArray lanes;
    lanes.addTokens (track.state.getProperty (trackProperties::automationLanes).toString(), "|", "");

    for (auto& lane : lanes)
        if (lane == "-")
            lane = juce::String();

    return lanes;
}

void AutomationService::writeLanes (TrackHandle& track, const juce::StringArray& lanes)
{
    juce::StringArray stored (lanes);

    for (auto& lane : stored)
        if (lane.isEmpty())
            lane = "-";

    auto* edit = projectService.getEdit();
    track.state.setProperty (trackProperties::automationLanes,
                             stored.joinIntoString ("|"),
                             edit != nullptr ? &edit->getUndoManager() : nullptr);
}

int AutomationService::getNumLanes (TrackHandle& track) const
{
    return readLanes (track).size();
}

std::optional<AutomationParameterInfo> AutomationService::getLaneParameter (TrackHandle& track, int laneIndex) const
{
    const auto lanes = readLanes (track);

    if (laneIndex < 0 || laneIndex >= lanes.size() || lanes[laneIndex].isEmpty())
        return std::nullopt;

    return getParameter (track, lanes[laneIndex]);
}

void AutomationService::setLaneParameter (TrackHandle& track, int laneIndex, const juce::String& parameterId)
{
    auto lanes = readLanes (track);

    if (laneIndex < 0 || laneIndex >= lanes.size())
        return;

    projectService.beginUndoStep (TRANS ("Automation parameter"));
    lanes.set (laneIndex, parameterId);
    writeLanes (track, lanes);
}

void AutomationService::addLane (TrackHandle& track, const juce::String& parameterId)
{
    projectService.beginUndoStep (TRANS ("Add automation lane"));
    auto lanes = readLanes (track);
    lanes.add (parameterId);
    writeLanes (track, lanes);
}

void AutomationService::removeLane (TrackHandle& track, int laneIndex)
{
    auto lanes = readLanes (track);

    if (laneIndex < 0 || laneIndex >= lanes.size())
        return;

    projectService.beginUndoStep (TRANS ("Remove automation lane"));
    lanes.remove (laneIndex);
    writeLanes (track, lanes);
}

double AutomationService::secondsFor (const te::AutomatableParameter& parameter, int index) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return 0.0;

    return te::toTime (parameter.getCurve().getPointPosition (index), edit->tempoSequence).inSeconds();
}

std::vector<AutomationPointInfo> AutomationService::getPoints (TrackHandle& track,
                                                               const juce::String& parameterId) const
{
    std::vector<AutomationPointInfo> points;
    auto* parameter = findParameter (track, parameterId);

    if (parameter == nullptr)
        return points;

    const auto& curve = parameter->getCurve();

    for (int i = 0; i < curve.getNumPoints(); ++i)
        points.push_back ({ secondsFor (*parameter, i), curve.getPointValue (i), curve.getPointCurve (i) });

    return points;
}

float AutomationService::getValueAt (TrackHandle& track, const juce::String& parameterId, double seconds) const
{
    auto* parameter = findParameter (track, parameterId);

    if (parameter == nullptr)
        return 0.0f;

    return parameter->getCurve().getValueAt (te::EditPosition (te::TimePosition::fromSeconds (seconds)),
                                             parameter->getCurrentValue());
}

juce::String AutomationService::getValueText (TrackHandle& track, const juce::String& parameterId, float value) const
{
    if (auto* parameter = findParameter (track, parameterId))
        return parameter->valueToString (value);

    return {};
}

core::Result<int>
AutomationService::addPoint (TrackHandle& track, const juce::String& parameterId, double seconds, float value)
{
    auto* edit = projectService.getEdit();
    auto* parameter = findParameter (track, parameterId);

    if (edit == nullptr || parameter == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, (edit == nullptr ? TRANS ("No project is open.") : TRANS ("Parameter not found.")).toStdString());

    projectService.beginUndoStep (TRANS ("Set automation point"));
    const auto clamped = parameter->getValueRange().clipValue (value);
    const auto index =
        parameter->getCurve().addPoint (te::EditPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds))),
                                        clamped,
                                        0.0f,
                                        &edit->getUndoManager());
    return index;
}

core::Result<int> AutomationService::movePoint (
    TrackHandle& track, const juce::String& parameterId, int index, double seconds, float value)
{
    auto* edit = projectService.getEdit();
    auto* parameter = findParameter (track, parameterId);

    if (edit == nullptr || parameter == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, (edit == nullptr ? TRANS ("No project is open.") : TRANS ("Parameter not found.")).toStdString());

    auto& curve = parameter->getCurve();

    if (index < 0 || index >= curve.getNumPoints())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Automation point does not exist.").toStdString());

    const auto clamped = parameter->getValueRange().clipValue (value);
    return curve.movePoint (index,
                            te::EditPosition (te::TimePosition::fromSeconds (juce::jmax (0.0, seconds))),
                            clamped,
                            std::nullopt,
                            false,
                            &edit->getUndoManager());
}

core::Result<void> AutomationService::removePoint (TrackHandle& track, const juce::String& parameterId, int index)
{
    auto* edit = projectService.getEdit();
    auto* parameter = findParameter (track, parameterId);

    if (edit == nullptr || parameter == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, (edit == nullptr ? TRANS ("No project is open.") : TRANS ("Parameter not found.")).toStdString());

    auto& curve = parameter->getCurve();

    if (index < 0 || index >= curve.getNumPoints())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Automation point does not exist.").toStdString());

    projectService.beginUndoStep (TRANS ("Remove automation point"));
    curve.removePoint (index, &edit->getUndoManager());
    return {};
}

core::Result<void> AutomationService::clearPoints (TrackHandle& track, const juce::String& parameterId)
{
    auto* edit = projectService.getEdit();
    auto* parameter = findParameter (track, parameterId);

    if (edit == nullptr || parameter == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, (edit == nullptr ? TRANS ("No project is open.") : TRANS ("Parameter not found.")).toStdString());

    projectService.beginUndoStep (TRANS ("Delete automation"));
    parameter->getCurve().clear (&edit->getUndoManager());
    return {};
}

void AutomationService::beginChange (const juce::String& actionName)
{
    projectService.beginUndoStep (actionName);
}

AutomationMode AutomationService::getMode (TrackHandle& track) const
{
    return fromTracktion (track.automationMode.get());
}

void AutomationService::setMode (TrackHandle& track, AutomationMode mode)
{
    if (getMode (track) == mode)
        return;

    projectService.beginUndoStep (TRANS ("Automation mode"));
    track.automationMode = toTracktion (mode);
    triggerAsyncUpdate();
}

juce::String AutomationService::getModeName (AutomationMode mode)
{
    switch (mode)
    {
        case AutomationMode::read:
            return TRANS ("Read");
        case AutomationMode::touch:
            return TRANS ("Touch");
        case AutomationMode::latch:
            return TRANS ("Latch");
        case AutomationMode::write:
            return TRANS ("Write");
    }

    return {};
}

bool AutomationService::isReadEnabled() const
{
    return observedRecordManager != nullptr && observedRecordManager->isReadingAutomation();
}

void AutomationService::setReadEnabled (bool shouldRead)
{
    if (observedRecordManager != nullptr)
        observedRecordManager->setReadingAutomation (shouldRead);
}

bool AutomationService::isWriteEnabled() const
{
    return observedRecordManager != nullptr && observedRecordManager->isWritingAutomation();
}

void AutomationService::setWriteEnabled (bool shouldWrite)
{
    if (observedRecordManager != nullptr)
        observedRecordManager->setWritingAutomation (shouldWrite);
}

void AutomationService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void AutomationService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void AutomationService::projectChanged()
{
    if (observedRecordManager != nullptr)
        observedRecordManager->removeListener (this);

    observedRecordManager = nullptr;
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
        observedRecordManager = &edit->getAutomationRecordManager();
        observedRecordManager->addListener (this);
    }

    cancelPendingUpdate();
    handleAsyncUpdate();
}

void AutomationService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.automationChanged(); });
}

void AutomationService::automationModeChanged()
{
    triggerAsyncUpdate();
}

bool AutomationService::isAutomationTree (const juce::ValueTree& tree)
{
    return tree.hasType (te::IDs::AUTOMATIONCURVE) || tree.hasType (te::IDs::POINT);
}

void AutomationService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (isAutomationTree (tree) || property == trackProperties::automationLanes || property == te::IDs::automationMode)
        triggerAsyncUpdate();
}

void AutomationService::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (isAutomationTree (parent) || isAutomationTree (child))
        triggerAsyncUpdate();
}

void AutomationService::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (isAutomationTree (parent) || isAutomationTree (child))
        triggerAsyncUpdate();
}

} // namespace fk::services
