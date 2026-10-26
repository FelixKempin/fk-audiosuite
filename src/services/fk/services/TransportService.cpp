#include "fk/services/TransportService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
// Eigener Knoten im Edit für Transport-Einstellungen, die Tracktion nicht kennt.
const juce::Identifier transportNode ("FKTRANSPORT");
const juce::Identifier preRollProperty ("preRoll");
const juce::Identifier postRollProperty ("postRoll");
} // namespace

TransportService::TransportService (ProjectService& service)
    : projectService (service)
{
    projectService.addListener (*this);
    attachToCurrentEdit();
}

TransportService::~TransportService()
{
    projectService.removeListener (*this);
}

void TransportService::play()
{
    if (attachedTransport != nullptr)
        attachedTransport->play (false);
}

void TransportService::stop()
{
    if (attachedTransport != nullptr)
        attachedTransport->stop (false, false);
}

void TransportService::togglePlay()
{
    if (isPlaying())
        stop();
    else
        play();
}

core::Result<int> TransportService::captureRetrospective()
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto countClips = [edit]
    {
        int count = 0;

        for (auto* track : te::getAudioTracks (*edit))
            count += track->getClips().size();

        return count;
    };

    // Ohne scharfe Spur gibt es nichts zu holen; Tracktion meldete das sonst als Fehler.
    const auto inputs = edit->getAllInputDevices();

    if (std::none_of (inputs.begin(), inputs.end(), [] (auto* input) { return input->isRecordingActive(); }))
        return 0;

    const auto before = countClips();
    projectService.beginUndoStep (TRANS ("Retrospective recording"));

    if (const auto result = edit->getTransport().applyRetrospectiveRecord (true); result.failed())
        return core::fail (core::ErrorCode::unknown, plugins::i18n::format (TRANS ("Retrospective recording: {error}"), { { "error", result.getErrorMessage() } }).toStdString());

    return countClips() - before;
}

void TransportService::record()
{
    if (attachedTransport == nullptr)
        return;

    stopAtSeconds.reset();

    // Punch: vor dem linken Locator anfangen (Pre-Roll), damit man sich einhören kann; Tracktion nimmt
    // trotzdem erst ab dem Locator auf. Post-Roll: nach dem rechten Locator ausklingen lassen, dann stoppen.
    if (isPunchEnabled() && ! attachedTransport->isPlaying())
    {
        const auto range = attachedTransport->getLoopRange();
        attachedTransport->setPosition (
            te::TimePosition::fromSeconds (std::max (0.0, range.getStart().inSeconds() - getPreRollSeconds())));

        if (const auto postRoll = getPostRollSeconds(); postRoll > 0.0)
        {
            stopAtSeconds = range.getEnd().inSeconds() + postRoll;
            startTimerHz (30);
        }
    }

    recordStartSeconds = isPunchEnabled() ? attachedTransport->getLoopRange().getStart().inSeconds()
                                          : attachedTransport->getPosition().inSeconds();
    attachedTransport->record (false);
}

bool TransportService::isPunchEnabled() const
{
    const auto* edit = projectService.getEdit();
    return edit != nullptr && edit->recordingPunchInOut;
}

void TransportService::setPunchEnabled (bool shouldPunch)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return;

    // Tracktion lehnt Aufnahmen mit Loop und Punch zugleich ab; deshalb schließen sie sich hier aus.
    if (shouldPunch)
        edit->getTransport().looping = false;

    edit->recordingPunchInOut = shouldPunch;
    notifyStateChanged();
}

double TransportService::getPreRollSeconds() const
{
    return getRollSetting (preRollProperty);
}

void TransportService::setPreRollSeconds (double seconds)
{
    setRollSetting (preRollProperty, seconds, TRANS ("Pre-roll"));
}

double TransportService::getPostRollSeconds() const
{
    return getRollSetting (postRollProperty);
}

void TransportService::setPostRollSeconds (double seconds)
{
    setRollSetting (postRollProperty, seconds, TRANS ("Post-roll"));
}

double TransportService::getRollSetting (const juce::Identifier& property) const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return 0.0;

    return edit->state.getChildWithName (transportNode).getProperty (property, 0.0);
}

void TransportService::setRollSetting (const juce::Identifier& property, double seconds, const juce::String& undoName)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return;

    projectService.beginUndoStep (undoName);
    auto* undoManager = &edit->getUndoManager();
    edit->state.getOrCreateChildWithName (transportNode, undoManager)
        .setProperty (property, std::max (seconds, 0.0), undoManager);
    notifyStateChanged();
}

void TransportService::timerCallback()
{
    // Post-Roll: Die Aufnahme endet am rechten Locator von selbst; hier nur die Wiedergabe beenden.
    if (attachedTransport == nullptr || ! stopAtSeconds || ! attachedTransport->isPlaying())
    {
        stopAtSeconds.reset();
        stopTimer();
        return;
    }

    if (attachedTransport->getPosition().inSeconds() >= *stopAtSeconds)
    {
        stopAtSeconds.reset();
        stopTimer();
        stop();
    }
}

TransportService::CountIn TransportService::getCountIn() const
{
    const auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return CountIn::none;

    switch (edit->getCountInMode())
    {
        case te::Edit::CountIn::oneBar:
            return CountIn::oneBar;
        case te::Edit::CountIn::twoBar:
            return CountIn::twoBars;
        case te::Edit::CountIn::none:
        case te::Edit::CountIn::twoBeat:
        case te::Edit::CountIn::oneBeat:
            return CountIn::none;
    }

    return CountIn::none;
}

void TransportService::setCountIn (CountIn countIn)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return;

    // Tracktion merkt sich den Vorzähler in den Einstellungen der Engine, nicht im Projekt:
    // eine Arbeitsgewohnheit, die für alle Projekte gilt.
    switch (countIn)
    {
        case CountIn::none:
            edit->setCountInMode (te::Edit::CountIn::none);
            break;
        case CountIn::oneBar:
            edit->setCountInMode (te::Edit::CountIn::oneBar);
            break;
        case CountIn::twoBars:
            edit->setCountInMode (te::Edit::CountIn::twoBar);
            break;
    }

    notifyStateChanged();
}

juce::String TransportService::getCountInName (CountIn countIn)
{
    switch (countIn)
    {
        case CountIn::none:
            return TRANS ("No count-in");
        case CountIn::oneBar:
            return TRANS ("1 bar");
        case CountIn::twoBars:
            return TRANS ("2 bars");
    }

    return {};
}

void TransportService::returnToStart()
{
    setPositionSeconds (0.0);
}

void TransportService::setPositionSeconds (double seconds)
{
    if (attachedTransport != nullptr)
        attachedTransport->setPosition (te::TimePosition::fromSeconds (seconds));
}

bool TransportService::isPlaying() const
{
    return attachedTransport != nullptr && attachedTransport->isPlaying();
}

bool TransportService::isRecording() const
{
    return attachedTransport != nullptr && attachedTransport->isRecording();
}

float TransportService::getDspLoad() const
{
    if (auto* edit = projectService.getEdit())
        return edit->engine.getDeviceManager().getCpuUsage();

    return 0.0f;
}

double TransportService::getPositionSeconds() const
{
    return attachedTransport != nullptr ? attachedTransport->getPosition().inSeconds() : 0.0;
}

double TransportService::getRecordStartSeconds() const noexcept
{
    return recordStartSeconds;
}

void TransportService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void TransportService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void TransportService::projectChanged()
{
    stopAtSeconds.reset();
    stopTimer();

    // Das alte Edit ist hier bereits zerstört (ProjectService meldet erst danach),
    // der alte Zeiger darf also nur noch vergessen, nicht mehr benutzt werden.
    attachedTransport = nullptr;
    attachToCurrentEdit();
    notifyStateChanged();
}

void TransportService::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == attachedTransport)
        notifyStateChanged();
}

void TransportService::attachToCurrentEdit()
{
    if (auto* edit = projectService.getEdit())
    {
        attachedTransport = &edit->getTransport();
        attachedTransport->addChangeListener (this);
    }
}

void TransportService::notifyStateChanged()
{
    listeners.call ([] (Listener& listener) { listener.transportStateChanged(); });
}

} // namespace fk::services
