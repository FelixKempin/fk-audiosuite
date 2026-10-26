#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <juce_events/juce_events.h>

#include <vector>

namespace fk::services
{

/// Eine FX-Spur: Audiospur mit Aux-Return am Anfang ihrer Plugin-Kette.
struct FxTrackInfo
{
    juce::String id;
    juce::String name;
    int bus = 0;
};

/// Ein Send einer Spur zu einer FX-Spur. Gibt es (noch) kein Send-Plugin, ist exists false
/// und gainDb der Stillewert; die Oberfläche zeigt trotzdem einen Regler pro FX-Spur.
struct SendInfo
{
    juce::String fxTrackId;
    juce::String fxName;
    int bus = 0;
    bool exists = false;
    bool preFader = false;
    float gainDb = -100.0f;
};

/// Anwendungsfälle für Effektwege: FX-Spuren anlegen, Sends setzen, pre/post Fader.
/// Technik: Tracktions AuxSend-/AuxReturn-Plugins mit Bus-Nummer; die Position des Sends
/// in der Plugin-Kette (vor oder hinter Lautstärke/Pan) entscheidet über pre/post.
class SendService final : private ProjectService::Listener,
                          private juce::ValueTree::Listener,
                          private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void sendsChanged() = 0;
    };

    SendService (ProjectService& projectService, TrackService& trackService);
    ~SendService() override;

    [[nodiscard]] core::Result<te::AudioTrack*> createFxTrack (const juce::String& name);
    [[nodiscard]] std::vector<FxTrackInfo> getFxTracks() const;

    /// Ein Eintrag je FX-Spur, in Reihenfolge der FX-Spuren. FX-Spuren selbst haben keine Sends.
    [[nodiscard]] std::vector<SendInfo> getSends (TrackHandle& track) const;

    /// Setzt den Pegel; legt das Send-Plugin bei Bedarf an (post Fader).
    [[nodiscard]] core::Result<void> setSendLevel (TrackHandle& track, const juce::String& fxTrackId, float gainDb);
    [[nodiscard]] core::Result<void> setSendPreFader (TrackHandle& track, const juce::String& fxTrackId, bool preFader);
    [[nodiscard]] core::Result<void> removeSend (TrackHandle& track, const juce::String& fxTrackId);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int oldIndex, int newIndex) override;

    [[nodiscard]] te::AuxSendPlugin* findSend (TrackHandle& track, int bus) const;
    [[nodiscard]] int indexOfVolumePlugin (TrackHandle& track) const;
    [[nodiscard]] int nextFreeBus() const;

    ProjectService& projectService;
    TrackService& trackService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendService)
};

} // namespace fk::services
