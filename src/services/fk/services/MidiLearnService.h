#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <juce_events/juce_events.h>

#include <memory>
#include <vector>

namespace fk::services
{

/// Eine gelernte Zuordnung: ein Hardware-Regler oder Pad steuert einen Parameter.
struct MidiMappingInfo
{
    int index = 0;
    juce::String parameterName;  ///< voller Name, z. B. "FK Synth: Cutoff" oder "Lautstärke"
    juce::String controllerName; ///< z. B. "CC 74 (Kanal 1)"
};

/// MIDI Learn (ROADMAP M8): Lernmodus an, einen Parameter in der DAW anfassen (Plugin-Regler,
/// Mixer-Fader), dann den Hardware-Regler bewegen: Tracktion verknüpft beide und speichert die
/// Zuordnung im Projekt (ParameterControlMappings). Bewegungen des Hardware-Reglers setzen danach
/// den Parameter und werden bei Automation "Schreiben" wie jede Reglerbewegung aufgezeichnet.
class MidiLearnService final : private ProjectService::Listener,
                               private te::MidiLearnState::Listener,
                               private juce::ChangeListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Lernmodus an/aus, Zuordnung hinzugekommen oder entfernt, nächster Schritt geändert.
        virtual void midiLearnChanged() = 0;
    };

    MidiLearnService (engine::EngineContext& engineContext, ProjectService& projectService);
    ~MidiLearnService() override;

    void setLearnActive (bool shouldBeActive);
    [[nodiscard]] bool isLearnActive() const;

    /// Was der Nutzer als Nächstes tun soll: "Parameter anfassen" oder "Regler bewegen" (leer = aus).
    [[nodiscard]] juce::String getLearnHint() const;

    [[nodiscard]] std::vector<MidiMappingInfo> getMappings() const;
    [[nodiscard]] core::Result<void> removeMapping (int index);
    void removeAllMappings();

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void midiLearnStatusChanged (bool isActive) override;
    void midiLearnAssignmentChanged (te::MidiLearnState::ChangeType type) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    [[nodiscard]] te::ParameterControlMappings* getMappingsOfEdit() const;
    [[nodiscard]] static juce::String describeController (int controllerId, int channel);

    engine::EngineContext& engineContext;
    ProjectService& projectService;
    juce::ListenerList<Listener> listeners;
};

} // namespace fk::services
