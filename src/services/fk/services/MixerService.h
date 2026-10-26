#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <juce_events/juce_events.h>

#include <map>
#include <memory>

namespace fk::services
{

/// Momentaner Spitzenpegel eines Kanalpaars in dB (≤ 0; -100 = Stille).
struct LevelReading
{
    float leftDb = -100.0f;
    float rightDb = -100.0f;
};

/// Anwendungsfälle für den Mixer: Lautstärke, Panorama, Pegelanzeige, Master.
/// Pegel werden gepollt (die Oberfläche fragt 30-mal pro Sekunde), nicht per Ereignis.
class MixerService final : private ProjectService::Listener,
                           private juce::ValueTree::Listener,
                           private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Lautstärke oder Panorama einer Spur haben sich geändert (auch durch Undo oder Automation).
        virtual void mixerChanged() = 0;
    };

    explicit MixerService (ProjectService& projectService);
    ~MixerService() override;

    [[nodiscard]] float getVolumeDb (TrackHandle& track) const;
    void setVolumeDb (TrackHandle& track, float volumeDb);

    /// Panorama von -1 (links) bis +1 (rechts).
    [[nodiscard]] float getPan (TrackHandle& track) const;
    void setPan (TrackHandle& track, float pan);

    /// Eingangs-Trim vor den Inserts in dB (±24) und Phasenumkehr. Das zuständige Plugin (FK Trim)
    /// entsteht beim ersten Verstellen und ist nie unter den Inserts zu sehen.
    [[nodiscard]] float getTrimDb (TrackHandle& track) const;
    [[nodiscard]] core::Result<void> setTrimDb (TrackHandle& track, float trimDb);
    [[nodiscard]] bool isPhaseInverted (TrackHandle& track) const;
    [[nodiscard]] core::Result<void> setPhaseInverted (TrackHandle& track, bool shouldInvert);

    [[nodiscard]] float getMasterVolumeDb() const;
    void setMasterVolumeDb (float volumeDb);

    /// Welchen Spurregler eine Geste betrifft.
    enum class MixParameter
    {
        volume,
        pan
    };

    /// Anfang und Ende einer Reglerbewegung (Maus gedrückt bis losgelassen). Tracktion braucht
    /// das für Touch/Latch beim Schreiben von Automation und fasst Undo-Schritte zusammen.
    void beginGesture (TrackHandle& track, MixParameter parameter);
    void endGesture (TrackHandle& track, MixParameter parameter);

    /// Spitzenpegel seit der letzten Abfrage. Meldet sich beim ersten Aufruf am Meter an.
    [[nodiscard]] LevelReading getLevels (TrackHandle& track);
    [[nodiscard]] LevelReading getMasterLevels();

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    [[nodiscard]] te::ExternalPlugin* findTrim (TrackHandle& track) const;
    [[nodiscard]] core::Result<te::ExternalPlugin*> ensureTrim (TrackHandle& track);

    /// Ein angemeldeter Pegel-Abnehmer. Meldet sich im Destruktor selbst ab (RAII).
    struct MeterClient
    {
        explicit MeterClient (te::LevelMeasurer& measurerToUse);
        ~MeterClient();

        te::LevelMeasurer& measurer;
        te::LevelMeasurer::Client client;

        JUCE_DECLARE_NON_COPYABLE (MeterClient)
    };

    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    [[nodiscard]] static LevelReading readLevels (MeterClient& meterClient);
    [[nodiscard]] MeterClient* meterClientFor (const juce::String& key, te::LevelMeasurer* measurer);

    ProjectService& projectService;
    juce::ValueTree observedEditState;
    std::map<juce::String, std::unique_ptr<MeterClient>> meterClients;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerService)
};

} // namespace fk::services
