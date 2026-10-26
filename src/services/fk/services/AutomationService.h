#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <juce_events/juce_events.h>

#include <optional>
#include <string_view>
#include <vector>

namespace fk::services
{

/// Wie eine Spur beim Abspielen mit Schreiben umgeht (Cubase: Read/Touch/Latch/Write).
enum class AutomationMode
{
    read,  ///< nur lesen, nie schreiben
    touch, ///< schreibt, solange der Regler angefasst ist
    latch, ///< schreibt ab dem ersten Anfassen bis Stop
    write  ///< schreibt ab Play-Start bis zum Loslassen
};

/// Ein automatisierbarer Parameter einer Spur: Lautstärke, Pan oder ein Plugin-Parameter.
struct AutomationParameterInfo
{
    juce::String id;        ///< stabil für Spur und Projekt: "<Plugin-ID>/<Parameter-ID>"
    juce::String name;      ///< "Lautstärke", "Pan", "Cutoff"
    juce::String ownerName; ///< leer für Spurparameter, sonst Plugin-Name ("FK EQ")
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float currentValue = 0.0f;
    bool hasPoints = false;
};

/// Ein Punkt der Automationskurve in Sekunden und im Wertebereich des Parameters.
struct AutomationPointInfo
{
    double seconds = 0.0;
    float value = 0.0f;
    float curve = 0.0f; ///< -1..1, 0 = gerade Verbindung
};

/// Anwendungsfälle für Automation: Parameter einer Spur auflisten, einen davon anzeigen,
/// Kurvenpunkte setzen und verschieben, Modus je Spur, Lesen/Schreiben global.
/// Tracktion zeichnet beim Schreiben selbst mit (AutomationRecordManager); wir schalten nur.
class AutomationService final : private ProjectService::Listener,
                                private juce::ValueTree::Listener,
                                private juce::AsyncUpdater,
                                private te::AutomationRecordManager::Listener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Kurven, angezeigter Parameter, Modus oder Lesen/Schreiben haben sich geändert.
        virtual void automationChanged() = 0;
    };

    explicit AutomationService (ProjectService& projectService);
    ~AutomationService() override;

    /// Lautstärke und Pan zuerst, dann alle Plugin-Parameter in Insert-Reihenfolge.
    [[nodiscard]] std::vector<AutomationParameterInfo> getParameters (TrackHandle& track) const;
    [[nodiscard]] std::optional<AutomationParameterInfo> getParameter (TrackHandle& track,
                                                                       const juce::String& parameterId) const;

    /// Automationsspuren (Lagen) einer Spur: je Lage ein Parameter oder keiner. Die Liste steht
    /// im Projekt (Spur-Eigenschaft fkAutomationLanes), damit sie das Speichern überlebt.
    [[nodiscard]] int getNumLanes (TrackHandle& track) const;
    [[nodiscard]] std::optional<AutomationParameterInfo> getLaneParameter (TrackHandle& track, int laneIndex) const;
    void setLaneParameter (TrackHandle& track, int laneIndex, const juce::String& parameterId);
    void addLane (TrackHandle& track, const juce::String& parameterId = {});
    void removeLane (TrackHandle& track, int laneIndex);

    [[nodiscard]] std::vector<AutomationPointInfo> getPoints (TrackHandle& track,
                                                              const juce::String& parameterId) const;
    [[nodiscard]] float getValueAt (TrackHandle& track, const juce::String& parameterId, double seconds) const;
    [[nodiscard]] juce::String getValueText (TrackHandle& track, const juce::String& parameterId, float value) const;

    /// Liefert den Index des neuen Punkts. Werte werden auf den Bereich begrenzt.
    [[nodiscard]] core::Result<int>
    addPoint (TrackHandle& track, const juce::String& parameterId, double seconds, float value);

    /// Verschiebt einen Punkt; er überholt seine Nachbarn nicht (Zeit wird an ihnen begrenzt), der
    /// Index bleibt deshalb gleich und wird zurückgegeben. Kein eigener Undo-Schritt: Wer zieht,
    /// ruft vorher einmal beginChange().
    [[nodiscard]] core::Result<int>
    movePoint (TrackHandle& track, const juce::String& parameterId, int index, double seconds, float value);
    [[nodiscard]] core::Result<void> removePoint (TrackHandle& track, const juce::String& parameterId, int index);
    [[nodiscard]] core::Result<void> clearPoints (TrackHandle& track, const juce::String& parameterId);

    /// Beginnt einen Undo-Schritt für eine zusammenhängende Änderung (z. B. Ziehen eines Punkts).
    void beginChange (const juce::String& actionName);

    [[nodiscard]] AutomationMode getMode (TrackHandle& track) const;
    void setMode (TrackHandle& track, AutomationMode mode);
    [[nodiscard]] static juce::String getModeName (AutomationMode mode);

    /// Global wie die Auto-Read/Auto-Write-Schalter im Transport. Ohne Lesen bleiben Kurven stumm.
    [[nodiscard]] bool isReadEnabled() const;
    void setReadEnabled (bool shouldRead);
    [[nodiscard]] bool isWriteEnabled() const;
    void setWriteEnabled (bool shouldWrite);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void automationModeChanged() override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    [[nodiscard]] te::AutomatableParameter* findParameter (TrackHandle& track, const juce::String& parameterId) const;
    [[nodiscard]] static juce::String idFor (const te::AutomatableParameter& parameter);
    [[nodiscard]] static AutomationParameterInfo infoFor (te::AutomatableParameter& parameter);
    [[nodiscard]] static bool isAutomationTree (const juce::ValueTree& tree);
    [[nodiscard]] double secondsFor (const te::AutomatableParameter& parameter, int index) const;
    [[nodiscard]] static juce::StringArray readLanes (TrackHandle& track);
    void writeLanes (TrackHandle& track, const juce::StringArray& lanes);

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    te::AutomationRecordManager* observedRecordManager = nullptr; // gehört dem Edit
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationService)
};

} // namespace fk::services
