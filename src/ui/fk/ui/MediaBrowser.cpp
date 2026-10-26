#include "fk/ui/MediaBrowser.h"

#include "fk/services/ErrorText.h"
#include "fk/plugins/i18n/Text.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int rootHome = 1;
constexpr int rootMusic = 2;
constexpr int rootDesktop = 3;
constexpr int previewHeight = 72;
} // namespace

MediaBrowser::MediaBrowser (services::PreviewService& previewService, services::WaveformService& waveformService)
    : preview (previewService)
    , waveforms (waveformService)
    , playButton (Icon::play, TRANS ("Audition the selected file (Space in the browser)"))
    , importButton (Icon::addAudioTrack,
                    TRANS ("Insert at the cursor on the selected track (double-click)"),
                    {})
{
    scanThread.startThread();

    rootBox.addItem (TRANS ("Home folder"), rootHome);
    rootBox.addItem (TRANS ("Music"), rootMusic);
    rootBox.addItem (TRANS ("Desktop"), rootDesktop);
    rootBox.setTooltip (TRANS ("Start folder of the file tree"));
    rootBox.onChange = [this]
    {
        switch (rootBox.getSelectedId())
        {
            case rootMusic:
                setRoot (juce::File::getSpecialLocation (juce::File::userMusicDirectory));
                break;
            case rootDesktop:
                setRoot (juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
                break;
            default:
                setRoot (juce::File::getSpecialLocation (juce::File::userHomeDirectory));
                break;
        }
    };

    tree.setItemHeight (20);
    tree.setMultiSelectEnabled (true);
    tree.setDragAndDropDescription (dragDescriptionPrefix);
    tree.addListener (this);

    infoLabel.setFont (juce::FontOptions (12.0f));
    infoLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    infoLabel.setText (TRANS ("Choose a file (audio or MIDI): waveform and audition appear here. Dragging drops the file into the arrangement."),
                       juce::dontSendNotification);

    playButton.setOnColour (theme::meterLow);
    playButton.onClick = [this] { togglePreview(); };
    importButton.onClick = [this]
    {
        if (onImportRequested && ! getSelectedAudioFiles().isEmpty())
            onImportRequested (getSelectedAudioFiles());
    };

    addAndMakeVisible (rootBox);
    addAndMakeVisible (tree);
    addAndMakeVisible (infoLabel);
    addAndMakeVisible (playButton);
    addAndMakeVisible (importButton);

    rootBox.setSelectedId (rootHome, juce::sendNotificationSync);
    startTimerHz (10);
}

MediaBrowser::~MediaBrowser()
{
    tree.removeListener (this);
    thumbnail.reset();
    contents.clear();
    scanThread.stopThread (2000);
}

void MediaBrowser::setRoot (const juce::File& folder)
{
    contents.setDirectory (
        folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userHomeDirectory), true, true);
    contents.refresh();
}

juce::StringArray MediaBrowser::getSelectedAudioFiles() const
{
    juce::StringArray files;

    for (int i = 0; i < tree.getNumSelectedFiles(); ++i)
        if (const auto file = tree.getSelectedFile (i); file.existsAsFile())
            files.add (file.getFullPathName());

    return files;
}

void MediaBrowser::selectionChanged()
{
    const auto files = getSelectedAudioFiles();
    selectedFile = files.isEmpty() ? juce::File() : juce::File (files[0]);

    // Die Beschreibung des Drag-Vorgangs trägt die Pfade: Das Arrangement liest sie beim Abwurf.
    tree.setDragAndDropDescription (dragDescriptionPrefix + files.joinIntoString ("\n"));

    const bool midi = selectedFile.hasFileExtension ("mid;midi");
    thumbnail = selectedFile.existsAsFile() && ! midi ? waveforms.createThumbnail (selectedFile, *this) : nullptr;
    playButton.setEnabled (! midi); // Vorhören gibt es nur für Audio; MIDI braucht ein Instrument

    if (selectedFile.existsAsFile())
    {
        const auto more = files.size() > 1 ? plugins::i18n::format (TRANS ("   (+{count} more)"), { { "count", juce::String (files.size() - 1) } })
                                  : juce::String();

        if (midi)
            infoLabel.setText (
                selectedFile.getFileName() +
                    TRANS ("   MIDI file: inserting creates one clip per track of the file") + more,
                juce::dontSendNotification);
        else
            infoLabel.setText (selectedFile.getFileName() + "   " +
                                   juce::String (preview.getFileLengthSeconds (selectedFile), 1) + " s" + more,
                               juce::dontSendNotification);
    }

    repaint();
}

void MediaBrowser::fileClicked (const juce::File&, const juce::MouseEvent&) {}

void MediaBrowser::fileDoubleClicked (const juce::File& file)
{
    if (file.existsAsFile() && onImportRequested)
        onImportRequested ({ file.getFullPathName() });
}

void MediaBrowser::browserRootChanged (const juce::File&) {}

void MediaBrowser::togglePreview()
{
    if (preview.isPlaying())
    {
        preview.stop();
        return;
    }

    if (! selectedFile.existsAsFile())
        return;

    if (const auto result = preview.play (selectedFile); ! result)
        juce::Logger::writeToLog ("Vorschau: " + services::errorText (result.error()));
}

void MediaBrowser::timerCallback()
{
    playButton.setToggleState (preview.isPlaying(), juce::dontSendNotification);

    if (preview.isPlaying())
        repaint (getLocalBounds().removeFromBottom (previewHeight));
}

void MediaBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);

    auto area = getLocalBounds().removeFromBottom (previewHeight).reduced (theme::spacingMedium, theme::spacingSmall);
    area.removeFromLeft (48 + theme::spacingMedium); // Platz der Buttons
    g.setColour (theme::backgroundInput);
    g.fillRoundedRectangle (area.toFloat(), 4.0f);

    if (thumbnail != nullptr && thumbnail->getTotalLength() > 0.0)
    {
        g.setColour (theme::accent.withAlpha (0.8f));
        thumbnail->drawChannels (g, area.reduced (2), 0.0, thumbnail->getTotalLength(), 1.0f);

        if (preview.isPlaying() && preview.getCurrentFile() == selectedFile && preview.getLengthSeconds() > 0.0)
        {
            const auto x = area.getX() + static_cast<int> (preview.getPositionSeconds() / preview.getLengthSeconds() *
                                                           area.getWidth());
            g.setColour (theme::textPrimary);
            g.drawVerticalLine (x, static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        }
    }
}

void MediaBrowser::resized()
{
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom (previewHeight).reduced (theme::spacingMedium, theme::spacingSmall);
    auto buttons = bottom.removeFromLeft (48);
    playButton.setBounds (buttons.removeFromTop (buttons.getHeight() / 2).reduced (2));
    importButton.setBounds (buttons.reduced (2));

    auto top = area.removeFromTop (28).reduced (theme::spacingMedium, theme::spacingSmall);
    rootBox.setBounds (top.removeFromLeft (180));
    top.removeFromLeft (theme::spacingMedium);
    infoLabel.setBounds (top);
    tree.setBounds (area.reduced (theme::spacingMedium, 0));
}

} // namespace fk::ui
