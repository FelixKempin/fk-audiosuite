#include "fk/ui/ExportDialog.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <array>
#include <variant>

namespace fk::ui
{

namespace
{
/// Was der Hintergrund-Thread erledigt; die Ergebnisse unterscheiden sich je Ziel.
struct ExportJob
{
    ExportDialog::Target target = ExportDialog::Target::mixdown;
    services::ExportOptions options;
    std::vector<juce::String> stemTrackIds;
};

/// Führt den Export im Hintergrund aus und zeigt dabei JUCEs Fortschrittsfenster.
class ExportTask final : public juce::ThreadWithProgressWindow
{
public:
    ExportTask (services::ExportService& exportService, ExportJob exportJob)
        : juce::ThreadWithProgressWindow (TRANS ("Exporting..."), true, true)
        , exporter (exportService)
        , job (std::move (exportJob))
    {
    }

    void run() override
    {
        const auto reportProgress = [this] (float p) { setProgress (p); };

        switch (job.target)
        {
            case ExportDialog::Target::mixdown:
                result = exporter.exportMixdown (job.options, reportProgress);
                break;
            case ExportDialog::Target::stems:
                result = exporter.exportStems (job.options, job.stemTrackIds, reportProgress);
                break;
            case ExportDialog::Target::midi:
                // Kurz und nur lesend, aber über das Edit: auf dem Message-Thread wie jede Edit-Abfrage.
                juce::MessageManager::callSync ([this] { result = exporter.exportMidi (job.options.file); });
                break;
        }
    }

    void threadComplete (bool userPressedCancel) override
    {
        if (userPressedCancel)
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, TRANS ("Export"), TRANS ("Export cancelled."));
        else
            std::visit ([] (const auto& outcome) { report (outcome); }, result);

        delete this; // JUCE-Vertrag für ThreadWithProgressWindow bei launchThread(): löscht sich selbst nach Abschluss
    }

private:
    static void report (const core::Result<juce::File>& outcome)
    {
        if (outcome)
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon, TRANS ("Export"),
                plugins::i18n::format (TRANS ("Written:\n{file}"), { { "file", outcome->getFullPathName() } }));
        else
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, TRANS ("Export"), services::errorText (outcome.error()));
    }

    static void report (const core::Result<std::vector<juce::File>>& outcome)
    {
        if (outcome && ! outcome->empty())
        {
            const auto count = static_cast<int> (outcome->size());
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                TRANS ("Export"),
                plugins::i18n::format (
                    plugins::i18n::plural (count, "{count} stem written to:\n{folder}", "{count} stems written to:\n{folder}"),
                    { { "folder", outcome->front().getParentDirectory().getFullPathName() } }));
        }
        else if (! outcome)
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, TRANS ("Export"), services::errorText (outcome.error()));
    }

    services::ExportService& exporter;
    ExportJob job;
    std::variant<core::Result<juce::File>, core::Result<std::vector<juce::File>>> result =
        core::Result<juce::File> (core::fail (core::ErrorCode::unknown, TRANS ("Not started").toStdString()));
};

constexpr int rangeWhole = 1;
constexpr int rangeLoop = 2;
constexpr int sampleRateProject = 1; // ComboBox-ID; die anderen IDs sind die Rate selbst
constexpr std::array<int, 4> sampleRates { 44100, 48000, 88200, 96000 };

/// Normalisieren: Anzeigename, Modus, Zielwert.
struct NormaliseChoice
{
    const char* name;
    services::NormaliseMode mode;
    float targetDb;
};

const std::array<NormaliseChoice, 6> normaliseChoices { {
    { NEEDS_TRANS ("Off"), services::NormaliseMode::off, 0.0f },
    { NEEDS_TRANS ("Peak 0 dBFS"), services::NormaliseMode::peak, 0.0f },
    { NEEDS_TRANS ("Peak -1 dBFS"), services::NormaliseMode::peak, -1.0f },
    { NEEDS_TRANS ("Loudness -14 LUFS (Streaming)"), services::NormaliseMode::loudness, -14.0f },
    { NEEDS_TRANS ("Loudness -16 LUFS (Podcast)"), services::NormaliseMode::loudness, -16.0f },
    { NEEDS_TRANS ("Loudness -23 LUFS (Broadcast, EBU R128)"), services::NormaliseMode::loudness, -23.0f },
} };

constexpr std::array<services::ExportFormat, 5> formats { services::ExportFormat::wav,
                                                          services::ExportFormat::flac,
                                                          services::ExportFormat::aiff,
                                                          services::ExportFormat::ogg,
                                                          services::ExportFormat::mp3 };

juce::String formatName (services::ExportFormat format)
{
    switch (format)
    {
        case services::ExportFormat::wav:
            return "WAV";
        case services::ExportFormat::flac:
            return TRANS ("FLAC (lossless, smaller)");
        case services::ExportFormat::aiff:
            return "AIFF";
        case services::ExportFormat::ogg:
            return "OGG Vorbis";
        case services::ExportFormat::mp3:
            return "MP3";
    }

    return "WAV";
}

int formatId (services::ExportFormat format)
{
    return static_cast<int> (format) + 1;
}
} // namespace

ExportDialog::ExportDialog (Services services)
    : exporter (services.exporter)
    , project (services.project)
    , projectSettings (services.projectSettings)
    , tracks (services.tracks)
    , chooseFileButton (TRANS ("Change..."))
    , allStemsButton (TRANS ("All"))
    , noStemsButton (TRANS ("None"))
    , exportButton (Icon::exportFile, TRANS ("Start export"), TRANS ("Export Now"))
{
    targetLabel.setText (TRANS ("Export"), juce::dontSendNotification);
    targetBox.addItem (TRANS ("Mixdown (master, one file)"), static_cast<int> (Target::mixdown));
    targetBox.addItem (TRANS ("Stems (one file per track or group)"), static_cast<int> (Target::stems));
    targetBox.addItem (TRANS ("MIDI file (all MIDI clips, with tempo)"), static_cast<int> (Target::midi));
    targetBox.setSelectedId (static_cast<int> (Target::mixdown), juce::dontSendNotification);
    targetBox.setTooltip (TRANS ("Mixdown: the sum as you hear it. Stems: each selected track on its own, without master effects, for mixing elsewhere. MIDI: notes and tempo as a standard MIDI file."));
    targetBox.onChange = [this]
    {
        updateTargetFile();
        updateVisibility();
    };

    fileLabel.setText (TRANS ("Destination"), juce::dontSendNotification);
    fileName.setMinimumHorizontalScale (0.6f);
    chooseFileButton.setTooltip (TRANS ("Choose file or folder"));
    chooseFileButton.onClick = [this] { chooseFile(); };

    formatLabel.setText (TRANS ("Format"), juce::dontSendNotification);

    for (const auto format : formats)
        formatBox.addItem (formatName (format), formatId (format));

    if (! exporter.isFormatAvailable (services::ExportFormat::mp3))
        formatBox.setItemEnabled (formatId (services::ExportFormat::mp3), false);

    formatBox.setSelectedId (formatId (services::ExportFormat::wav), juce::dontSendNotification);
    formatBox.setTooltip (TRANS ("WAV/AIFF/FLAC are lossless; OGG and MP3 are lossy and small. MP3 needs the libmp3lame system library."));
    formatBox.onChange = [this]
    {
        fillQualityChoices();
        updateTargetFile();
        updateVisibility();
    };

    rangeLabel.setText (TRANS ("Range"), juce::dontSendNotification);
    rangeBox.addItem (TRANS ("Entire project"), rangeWhole);
    rangeBox.addItem (TRANS ("Loop range"), rangeLoop);
    rangeBox.setSelectedId (rangeWhole, juce::dontSendNotification);

    bitDepthLabel.setText (TRANS ("Bit depth"), juce::dontSendNotification);
    bitDepthBox.addItem (TRANS ("16 bit (CD)"), 16);
    bitDepthBox.addItem (TRANS ("24 bit"), 24);
    bitDepthBox.addItem (TRANS ("32 bit float"), 32);
    bitDepthBox.setSelectedId (24, juce::dontSendNotification);
    bitDepthBox.onChange = [this] { updateVisibility(); };

    qualityLabel.setText (TRANS ("Quality"), juce::dontSendNotification);
    qualityBox.setTooltip (TRANS ("Higher bitrate = better sound, larger file. 320 kbit/s for MP3 is practically indistinguishable from the original."));
    fillQualityChoices();

    sampleRateLabel.setText (TRANS ("Sample rate"), juce::dontSendNotification);
    const auto projectRate = juce::roundToInt (projectSettings.getSettings().sampleRate);
    sampleRateBox.addItem (plugins::i18n::format (TRANS ("Same as project ({rate} Hz)"), { { "rate", juce::String (projectRate) } }),
                           sampleRateProject);

    for (const auto rate : sampleRates)
        sampleRateBox.addItem (juce::String (rate) + " Hz", rate);

    sampleRateBox.setSelectedId (sampleRateProject, juce::dontSendNotification);
    sampleRateBox.setTooltip (TRANS ("44100 Hz for CD and most streaming services, 48000 Hz for video. The project is rendered directly at the target rate."));

    normaliseLabel.setText (TRANS ("Normalize"), juce::dontSendNotification);

    for (std::size_t i = 0; i < normaliseChoices.size(); ++i)
        normaliseBox.addItem (juce::translate (normaliseChoices[i].name), static_cast<int> (i) + 1);

    normaliseBox.setSelectedId (1, juce::dontSendNotification);
    normaliseBox.setTooltip (TRANS ("Peak: loudest point to the value. Loudness: measured loudness (LUFS) to the value, the peak stays below -1 dBTP."));

    ditherToggle.setButtonText (TRANS ("Dither"));
    ditherToggle.setTooltip (TRANS ("Fine noise when reducing to 16 or 24 bit; prevents distortion in quiet tails. Always switch on for 16 bit."));
    ditherToggle.setToggleState (true, juce::dontSendNotification);

    tailsToggle.setButtonText (TRANS ("Let tails ring out"));
    tailsToggle.setTooltip (TRANS ("Let reverb and echo keep running after the end until silence (10 seconds at most)."));
    tailsToggle.setToggleState (true, juce::dontSendNotification);

    stemsLabel.setText (TRANS ("Tracks"), juce::dontSendNotification);
    allStemsButton.setTooltip (TRANS ("Select all tracks"));
    noStemsButton.setTooltip (TRANS ("Select no tracks"));
    allStemsButton.onClick = [this]
    {
        for (auto& row : stemRows)
            row->toggle.setToggleState (true, juce::dontSendNotification);
    };
    noStemsButton.onClick = [this]
    {
        for (auto& row : stemRows)
            row->toggle.setToggleState (false, juce::dontSendNotification);
    };

    for (auto* track : tracks.getAudioTracks())
    {
        const auto info = tracks.getTrackInfo (*track);
        auto row = std::make_unique<StemRow>();
        row->trackId = tracks.getTrackId (*track);
        row->toggle.setButtonText (info.kind == services::TrackKind::group
                                       ? plugins::i18n::format (TRANS ("{name} (group)"), { { "name", info.name } })
                                   : info.kind == services::TrackKind::fx
                                       ? plugins::i18n::format (TRANS ("{name} (FX)"), { { "name", info.name } })
                                       : info.name);
        row->toggle.setToggleState (true, juce::dontSendNotification);
        stemHolder.addAndMakeVisible (row->toggle);
        stemRows.push_back (std::move (row));
    }

    stemViewport.setViewedComponent (&stemHolder, false);
    stemViewport.setScrollBarsShown (true, false);

    hint.setColour (juce::Label::textColourId, theme::textSecondary);
    hint.setJustificationType (juce::Justification::topLeft);

    exportButton.onClick = [this] { startExport(); };

    for (auto* label : { &targetLabel,
                         &fileLabel,
                         &formatLabel,
                         &rangeLabel,
                         &bitDepthLabel,
                         &qualityLabel,
                         &sampleRateLabel,
                         &normaliseLabel,
                         &stemsLabel })
        label->setColour (juce::Label::textColourId, theme::textSecondary);

    for (auto* component : std::initializer_list<juce::Component*> { &targetLabel,
                                                                     &targetBox,
                                                                     &fileLabel,
                                                                     &fileName,
                                                                     &chooseFileButton,
                                                                     &formatLabel,
                                                                     &formatBox,
                                                                     &rangeLabel,
                                                                     &rangeBox,
                                                                     &bitDepthLabel,
                                                                     &bitDepthBox,
                                                                     &qualityLabel,
                                                                     &qualityBox,
                                                                     &sampleRateLabel,
                                                                     &sampleRateBox,
                                                                     &normaliseLabel,
                                                                     &normaliseBox,
                                                                     &ditherToggle,
                                                                     &tailsToggle,
                                                                     &stemsLabel,
                                                                     &allStemsButton,
                                                                     &noStemsButton,
                                                                     &stemViewport,
                                                                     &hint,
                                                                     &exportButton })
        addChildComponent (component);

    updateTargetFile();
    updateVisibility();
    setSize (560, 600);
}

void ExportDialog::resized()
{
    constexpr int rowHeight = 26;
    constexpr int labelWidth = 110;

    auto area = getLocalBounds().reduced (theme::spacingLarge);
    exportButton.setBounds (area.removeFromBottom (rowHeight + 6).removeFromRight (150));
    area.removeFromBottom (theme::spacingSmall);

    const auto row = [&area] (juce::Label& label, juce::Component& field)
    {
        if (! field.isVisible())
            return;

        auto r = area.removeFromTop (rowHeight);
        label.setBounds (r.removeFromLeft (labelWidth));
        field.setBounds (r);
        area.removeFromTop (theme::spacingSmall);
    };

    row (targetLabel, targetBox);

    auto fileRow = area.removeFromTop (rowHeight);
    fileLabel.setBounds (fileRow.removeFromLeft (labelWidth));
    chooseFileButton.setBounds (fileRow.removeFromRight (90));
    fileRow.removeFromRight (theme::spacingSmall);
    fileName.setBounds (fileRow);
    area.removeFromTop (theme::spacingSmall);

    row (formatLabel, formatBox);
    row (rangeLabel, rangeBox);
    row (bitDepthLabel, bitDepthBox);
    row (qualityLabel, qualityBox);
    row (sampleRateLabel, sampleRateBox);
    row (normaliseLabel, normaliseBox);

    if (ditherToggle.isVisible() || tailsToggle.isVisible())
    {
        auto toggles = area.removeFromTop (rowHeight);
        toggles.removeFromLeft (labelWidth);
        ditherToggle.setBounds (toggles.removeFromLeft (120));
        tailsToggle.setBounds (toggles.removeFromLeft (200));
        area.removeFromTop (theme::spacingSmall);
    }

    if (stemViewport.isVisible())
    {
        auto header = area.removeFromTop (rowHeight);
        stemsLabel.setBounds (header.removeFromLeft (labelWidth));
        allStemsButton.setBounds (header.removeFromLeft (70));
        header.removeFromLeft (theme::spacingSmall);
        noStemsButton.setBounds (header.removeFromLeft (70));
        area.removeFromTop (theme::spacingSmall);

        auto list = area.removeFromTop (juce::jmin (area.getHeight() - 40, 200));
        list.removeFromLeft (labelWidth);
        stemViewport.setBounds (list);
        const int width = stemViewport.getMaximumVisibleWidth();

        for (std::size_t i = 0; i < stemRows.size(); ++i)
            stemRows[i]->toggle.setBounds (0, static_cast<int> (i) * 24, width, 24);

        stemHolder.setSize (width, static_cast<int> (stemRows.size()) * 24);
        area.removeFromTop (theme::spacingSmall);
    }

    hint.setBounds (area);
}

void ExportDialog::show (Services services)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Export");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (new ExportDialog (services)); // JUCE-API verlangt hier einen besessenen Zeiger
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void ExportDialog::setTarget (Target target)
{
    targetBox.setSelectedId (static_cast<int> (target), juce::sendNotificationSync);
}

ExportDialog::Target ExportDialog::currentTarget() const
{
    return static_cast<Target> (targetBox.getSelectedId());
}

services::ExportFormat ExportDialog::currentFormat() const
{
    return static_cast<services::ExportFormat> (formatBox.getSelectedId() - 1);
}

void ExportDialog::fillQualityChoices()
{
    const auto format = currentFormat();
    qualityBox.clear (juce::dontSendNotification);

    for (const auto& choice : exporter.getQualityChoices (format))
        qualityBox.addItem (choice.name, choice.index + 1); // ComboBox-IDs müssen > 0 sein

    qualityBox.setSelectedId (services::ExportService::getDefaultQuality (format) + 1, juce::dontSendNotification);
}

void ExportDialog::updateTargetFile()
{
    const auto exportFolder = project.getProjectFolder().getChildFile ("Export");

    switch (currentTarget())
    {
        case Target::mixdown:
            targetFile = (targetFile.getParentDirectory() == exportFolder && targetFile.getFileName() != "Stems"
                              ? targetFile
                              : exportFolder.getChildFile (project.getProjectName()))
                             .withFileExtension (services::ExportService::extensionFor (currentFormat()));
            break;
        case Target::stems:
            targetFile = exportFolder.getChildFile ("Stems");
            break;
        case Target::midi:
            targetFile = exportFolder.getChildFile (project.getProjectName() + ".mid");
            break;
    }

    fileName.setText (targetFile.getFullPathName(), juce::dontSendNotification);
}

void ExportDialog::updateVisibility()
{
    const auto target = currentTarget();
    const bool audio = target != Target::midi;
    const bool lossless = services::ExportService::isLossless (currentFormat());

    for (auto* component : std::initializer_list<juce::Component*> { &targetLabel,
                                                                     &targetBox,
                                                                     &fileLabel,
                                                                     &fileName,
                                                                     &chooseFileButton,
                                                                     &hint,
                                                                     &exportButton })
        component->setVisible (true);

    for (auto* component : std::initializer_list<juce::Component*> { &formatLabel,
                                                                     &formatBox,
                                                                     &rangeLabel,
                                                                     &rangeBox,
                                                                     &sampleRateLabel,
                                                                     &sampleRateBox,
                                                                     &normaliseLabel,
                                                                     &normaliseBox,
                                                                     &tailsToggle })
        component->setVisible (audio);

    bitDepthLabel.setVisible (audio && lossless);
    bitDepthBox.setVisible (audio && lossless);
    qualityLabel.setVisible (audio && ! lossless);
    qualityBox.setVisible (audio && ! lossless);
    ditherToggle.setVisible (audio && lossless);
    ditherToggle.setEnabled (bitDepthBox.getSelectedId() < 32);

    const bool stems = target == Target::stems;

    for (auto* component : std::initializer_list<juce::Component*> {
             &stemsLabel, &allStemsButton, &noStemsButton, &stemViewport })
        component->setVisible (stems);

    juce::String text;

    if (target == Target::stems)
        text = TRANS ("Each selected track is rendered on its own with its inserts, without master effects. Groups contain their tracks; sidechain sources run along silently.");
    else if (target == Target::midi)
        text = TRANS ("Type 1 MIDI file: tempo and time signature in the first track, then one track per instrument track. Muted clips are left out.");
    else if (! exporter.isFormatAvailable (services::ExportFormat::mp3))
        text = TRANS ("MP3 is off: the libmp3lame library is missing (install the package libmp3lame0).");

    hint.setText (text, juce::dontSendNotification);
    resized();
}

void ExportDialog::chooseFile()
{
    if (currentTarget() == Target::stems)
    {
        juce::FileChooser chooser (TRANS ("Choose folder for the stems"), targetFile);

        if (chooser.browseForDirectory())
            targetFile = chooser.getResult();
    }
    else
    {
        const auto extension = currentTarget() == Target::midi
                                   ? juce::String (".mid")
                                   : services::ExportService::extensionFor (currentFormat());
        juce::FileChooser chooser (TRANS ("Save as"), targetFile.withFileExtension (extension), "*" + extension);

        if (chooser.browseForFileToSave (true))
            targetFile = chooser.getResult();
    }

    fileName.setText (targetFile.getFullPathName(), juce::dontSendNotification);
}

std::vector<juce::String> ExportDialog::getSelectedStemTrackIds() const
{
    std::vector<juce::String> ids;

    for (const auto& row : stemRows)
        if (row->toggle.getToggleState())
            ids.push_back (row->trackId);

    return ids;
}

void ExportDialog::startExport()
{
    ExportJob job { currentTarget(), collectOptions(), getSelectedStemTrackIds() };

    if (job.target == Target::stems && job.stemTrackIds.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Export", TRANS ("No track selected for stems."));
        return;
    }

    // Der Task löscht sich selbst, wenn er fertig ist (siehe threadComplete).
    auto* task = new ExportTask (exporter, std::move (job));
    task->launchThread();
}

services::ExportOptions ExportDialog::collectOptions() const
{
    services::ExportOptions options;
    options.file = targetFile;
    options.format = currentFormat();
    options.range =
        rangeBox.getSelectedId() == rangeLoop ? services::ExportRange::loopRange : services::ExportRange::wholeProject;
    options.bitDepth = bitDepthBox.getSelectedId();
    options.quality = qualityBox.getSelectedId() - 1;
    options.sampleRate = sampleRateBox.getSelectedId() == sampleRateProject
                             ? projectSettings.getSettings().sampleRate
                             : static_cast<double> (sampleRateBox.getSelectedId());
    options.dither = ditherToggle.getToggleState() && options.bitDepth < 32;
    options.includeTails = tailsToggle.getToggleState();

    const auto& normalise = normaliseChoices[static_cast<std::size_t> (juce::jmax (1, normaliseBox.getSelectedId()) - 1)];
    options.normalise = normalise.mode;
    options.normaliseTargetDb = normalise.targetDb;
    return options;
}

} // namespace fk::ui
