#include "fk/ui/MixConsole.h"

#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int meterRefreshHz = 30;
constexpr int masterWidth = 120;
constexpr double minVolumeDb = -60.0;
constexpr double maxVolumeDb = 6.0;
} // namespace

MixConsole::MixConsole (services::ProjectService& projectService,
                        services::TrackService& trackService,
                        services::MixerService& mixerService,
                        services::PluginService& pluginService,
                        services::SendService& sendService)
    : project (projectService)
    , tracks (trackService)
    , mixer (mixerService)
    , plugins (pluginService)
    , sends (sendService)
{
    viewport.setViewedComponent (&stripsContainer, false);
    viewport.setScrollBarsShown (false, true);

    masterLabel.setText (TRANS ("Master"), juce::dontSendNotification);
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    masterSlider.setSliderStyle (juce::Slider::LinearVertical);
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    masterSlider.setRange (minVolumeDb, maxVolumeDb, 0.1);
    masterSlider.setSkewFactorFromMidPoint (-12.0);
    masterSlider.setDoubleClickReturnValue (true, 0.0);
    masterSlider.setTextValueSuffix (" dB");
    masterSlider.onValueChange = [this]
    {
        const auto value = masterSlider.getValue();
        mixer.setMasterVolumeDb (value <= minVolumeDb ? -100.0f : static_cast<float> (value));
    };

    addAndMakeVisible (viewport);
    addAndMakeVisible (masterLabel);
    addAndMakeVisible (masterSlider);
    addAndMakeVisible (masterMeter);

    project.addListener (*this);
    tracks.addListener (*this);
    mixer.addListener (*this);
    plugins.addListener (*this);
    sends.addListener (*this);

    rebuildStrips();
    startTimerHz (meterRefreshHz);
}

MixConsole::~MixConsole()
{
    pluginWindows.clear();
    sends.removeListener (*this);
    plugins.removeListener (*this);
    mixer.removeListener (*this);
    tracks.removeListener (*this);
    project.removeListener (*this);
}

void MixConsole::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);
    g.setColour (theme::borderStrong);
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));

    const auto masterArea = getLocalBounds().removeFromRight (masterWidth);
    g.setColour (theme::backgroundElevated);
    g.fillRect (masterArea);
}

void MixConsole::resized()
{
    auto area = getLocalBounds().withTrimmedTop (1);

    auto masterArea = area.removeFromRight (masterWidth).reduced (theme::spacingMedium);
    masterLabel.setBounds (masterArea.removeFromTop (20));
    masterArea.removeFromTop (theme::spacingSmall);
    masterMeter.setBounds (masterArea.removeFromRight (14).withTrimmedBottom (22));
    masterArea.removeFromRight (theme::spacingSmall);
    masterSlider.setBounds (masterArea);

    viewport.setBounds (area);
    layoutStrips();
}

void MixConsole::tracksChanged()
{
    rebuildStrips();
    closeOrphanedPluginWindows(); // gelöschte Spur: ihre Plugin-Fenster mit
}

void MixConsole::contentChanged()
{
    refreshStrips(); // Name, Mute, Solo, Eingang; die Kanalzüge bleiben
}

void MixConsole::mixerChanged()
{
    refreshStrips();
}

void MixConsole::insertsChanged()
{
    refreshStrips();
    closeOrphanedPluginWindows();
}

void MixConsole::sendsChanged()
{
    refreshStrips();
}

void MixConsole::projectChanged()
{
    pluginWindows.clear();
}

void MixConsole::timerCallback()
{
    const auto levels = mixer.getMasterLevels();
    masterMeter.setLevels (levels.leftDb, levels.rightDb);
}

void MixConsole::rebuildStrips()
{
    strips.clear();

    for (auto* track : tracks.getVisibleTracks())
    {
        auto strip =
            std::make_unique<ChannelStrip> (tracks, mixer, plugins, sends, *track, tracks.getTrackInfo (*track).colour);
        strip->onOpenInsertEditor = [this] (const juce::String& insertId) { openInsertEditor (insertId); };
        strip->onError = [this] (const juce::String& message)
        {
            if (onError)
                onError (message);
        };
        stripsContainer.addAndMakeVisible (*strip);
        strips.push_back (std::move (strip));
    }

    masterSlider.setValue (juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (mixer.getMasterVolumeDb())),
                           juce::dontSendNotification);
    layoutStrips();
}

void MixConsole::refreshStrips()
{
    for (auto& strip : strips)
        strip->refresh();

    masterSlider.setValue (juce::jlimit (minVolumeDb, maxVolumeDb, static_cast<double> (mixer.getMasterVolumeDb())),
                           juce::dontSendNotification);
}

void MixConsole::layoutStrips()
{
    const int height = juce::jmax (1, viewport.getMaximumVisibleHeight());
    stripsContainer.setSize (static_cast<int> (strips.size()) * theme::channelStripWidth, height);

    int x = 0;

    for (auto& strip : strips)
    {
        strip->setBounds (x, 0, theme::channelStripWidth, height);
        x += theme::channelStripWidth;
    }
}

void MixConsole::openInsertEditor (const juce::String& insertId)
{
    if (auto found = pluginWindows.find (insertId); found != pluginWindows.end())
    {
        found->second->toFront (true);
        return;
    }

    auto editor = plugins.createEditor (insertId);

    if (! editor)
    {
        juce::Logger::writeToLog ("Plugin-Fenster: " + services::errorText (editor.error()));

        if (onError)
            onError (services::errorText (editor.error()));

        return;
    }

    const auto title = (*editor)->getAudioProcessor()->getName();
    auto window = std::make_unique<PluginWindow> (title, std::move (*editor));
    window->onClosed = [this, insertId] { pluginWindows.erase (insertId); };
    pluginWindows.emplace (insertId, std::move (window));
}

void MixConsole::closeOrphanedPluginWindows()
{
    // Fenster schließen, deren Insert nicht mehr existiert (entfernt oder per Undo verschwunden).
    std::vector<juce::String> stillPresent;

    for (auto* track : tracks.getAudioTracks())
    {
        for (const auto& insert : plugins.getInserts (*track))
            stillPresent.push_back (insert.id);

        if (const auto instrument = plugins.getInstrument (*track))
            stillPresent.push_back (instrument->id);
    }

    for (auto it = pluginWindows.begin(); it != pluginWindows.end();)
    {
        const bool present = std::find (stillPresent.begin(), stillPresent.end(), it->first) != stillPresent.end();
        it = present ? std::next (it) : pluginWindows.erase (it);
    }
}

} // namespace fk::ui
