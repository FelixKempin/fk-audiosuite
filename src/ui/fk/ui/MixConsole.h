#pragma once

#include "fk/services/MixerService.h"
#include "fk/services/PluginService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/SendService.h"
#include "fk/services/TrackService.h"
#include "fk/ui/ChannelStrip.h"
#include "fk/ui/LevelMeter.h"
#include "fk/ui/PluginWindow.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace fk::ui
{

/// Die MixConsole (untere Zone): ein Kanalzug pro Spur, rechts der Master.
/// Verwaltet außerdem die offenen Plugin-Fenster und schließt sie, wenn ihr Insert
/// oder das Projekt verschwindet.
class MixConsole final : public juce::Component,
                         private services::TrackService::Listener,
                         private services::MixerService::Listener,
                         private services::PluginService::Listener,
                         private services::SendService::Listener,
                         private services::ProjectService::Listener,
                         private juce::Timer
{
public:
    MixConsole (services::ProjectService& projectService,
                services::TrackService& trackService,
                services::MixerService& mixerService,
                services::PluginService& pluginService,
                services::SendService& sendService);
    ~MixConsole() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /// Öffnet (oder hebt) das Fenster eines Inserts; auch der Inspector nutzt das.
    void openInsertEditor (const juce::String& insertId);

    /// Ein Fenster ließ sich nicht öffnen (zum Beispiel gebackener Insert); Text für den Benutzer.
    std::function<void (const juce::String& message)> onError;

private:
    void tracksChanged() override;
    void contentChanged() override;
    void mixerChanged() override;
    void insertsChanged() override;
    void sendsChanged() override;
    void projectChanged() override;
    void timerCallback() override;

    void rebuildStrips();
    void refreshStrips();
    void layoutStrips();
    void closeOrphanedPluginWindows();

    services::ProjectService& project;
    services::TrackService& tracks;
    services::MixerService& mixer;
    services::PluginService& plugins;
    services::SendService& sends;

    juce::Viewport viewport;
    juce::Component stripsContainer;
    std::vector<std::unique_ptr<ChannelStrip>> strips;

    juce::Label masterLabel;
    juce::Slider masterSlider;
    LevelMeter masterMeter;

    std::map<juce::String, std::unique_ptr<PluginWindow>> pluginWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixConsole)
};

} // namespace fk::ui
