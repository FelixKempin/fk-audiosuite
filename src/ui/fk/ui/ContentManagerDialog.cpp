#include "fk/ui/ContentManagerDialog.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int rowHeight = 70;
} // namespace

ContentManagerDialog::Worker::Worker (services::ContentService& service, juce::String id)
    : juce::Thread ("Content-Download")
    , packageId (std::move (id))
    , content (service)
{
}

ContentManagerDialog::Worker::~Worker()
{
    stopThread (10000); // Abbrechen fragt threadShouldExit() ab und räumt dann auf
}

void ContentManagerDialog::Worker::run()
{
    result = content.install (packageId, [this] (float p) { progress = p; }, [this] { return threadShouldExit(); });
    finished = true;
}

ContentManagerDialog::ContentManagerDialog (services::ContentService& contentService)
    : content (contentService)
{
    for (const auto& package : content.getPackages())
    {
        auto row = std::make_unique<Row>();
        row->packageId = package.id;
        row->title.setText (package.name, juce::dontSendNotification);
        row->title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        row->details.setText (juce::translate (package.description) + "\n"
                                  + plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{license} · approx. {size}")),
                                                           { { "license", package.licence },
                                                             { "size", juce::File::descriptionOfSizeInBytes (package.sizeBytes) } }),
                              juce::dontSendNotification);
        row->details.setColour (juce::Label::textColourId, theme::textSecondary);
        row->details.setJustificationType (juce::Justification::topLeft);
        row->status.setJustificationType (juce::Justification::centredRight);

        const auto id = package.id;
        row->action.onClick = [this, id]
        {
            if (content.isInstalled (id))
                removePackage (id);
            else
                startInstall (id);
        };

        for (auto* component : std::initializer_list<juce::Component*> { &row->title, &row->details, &row->status, &row->action })
            addAndMakeVisible (component);

        rows.push_back (std::move (row));
    }

    addChildComponent (progressBar);
    cancelButton.setTooltip (TRANS ("Cancel the download, partial files are deleted"));
    cancelButton.onClick = [this]
    {
        if (worker != nullptr)
            worker->signalThreadShouldExit();
    };
    addChildComponent (cancelButton);

    hint.setText (TRANS ("The libraries come directly from their makers (GitHub) and play in the FK Sampler. Afterwards their instruments appear under Track > Instrument from Library and in the Instrument Browser."),
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (hint);

    refreshRows();
    setSize (620, 120 + static_cast<int> (rows.size()) * rowHeight);
}

ContentManagerDialog::~ContentManagerDialog()
{
    stopTimer();
    worker.reset();
}

void ContentManagerDialog::refreshRows()
{
    for (auto& row : rows)
    {
        const bool installed = content.isInstalled (row->packageId);
        const bool downloading = worker != nullptr && worker->packageId == row->packageId;
        row->status.setText (getStatusText (row->packageId), juce::dontSendNotification);
        row->status.setColour (juce::Label::textColourId, installed ? theme::meterLow : theme::textSecondary);
        row->action.setIcon (installed ? Icon::trash : Icon::exportFile);
        row->action.setLabel (installed ? TRANS ("Remove") : TRANS ("Install"));
        row->action.setTooltip (installed ? TRANS ("Delete the package from disk") : TRANS ("Download and install"));
        row->action.setEnabled (worker == nullptr || downloading);
    }

    progressBar.setVisible (worker != nullptr);
    cancelButton.setVisible (worker != nullptr);
}

juce::String ContentManagerDialog::getStatusText (const juce::String& packageId) const
{
    if (worker != nullptr && worker->packageId == packageId)
        return TRANS ("downloading ...");

    return content.isInstalled (packageId) ? TRANS ("installed") : TRANS ("not installed");
}

void ContentManagerDialog::startInstall (const juce::String& packageId)
{
    if (worker != nullptr)
        return;

    worker = std::make_unique<Worker> (content, packageId);
    worker->startThread();
    progressValue = 0.0;
    startTimerHz (10);
    refreshRows();
}

void ContentManagerDialog::removePackage (const juce::String& packageId)
{
    if (const auto result = content.remove (packageId); ! result)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, TRANS ("Content Manager"),
                                                services::errorText (result.error()));
    else if (onContentChanged)
        onContentChanged();

    refreshRows();
}

void ContentManagerDialog::timerCallback()
{
    if (worker == nullptr)
    {
        stopTimer();
        return;
    }

    progressValue = worker->progress.load();

    if (! worker->finished)
        return;

    stopTimer();
    const auto result = worker->result;
    worker.reset();
    refreshRows();

    if (! result)
    {
        if (result.error().code != core::ErrorCode::cancelled)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, TRANS ("Content Manager"),
                                                    services::errorText (result.error()));
    }
    else if (onContentChanged)
    {
        onContentChanged();
    }
}

void ContentManagerDialog::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingLarge);
    hint.setBounds (area.removeFromTop (40));
    area.removeFromTop (theme::spacingSmall);

    for (auto& row : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        auto right = line.removeFromRight (150);
        row->action.setBounds (right.removeFromTop (30).reduced (0, 2));
        row->status.setBounds (right.removeFromTop (20));
        row->title.setBounds (line.removeFromTop (20));
        row->details.setBounds (line);
    }

    auto bottom = area.removeFromBottom (28);
    cancelButton.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (theme::spacingSmall);
    progressBar.setBounds (bottom);
}

void ContentManagerDialog::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundBase);
}

void ContentManagerDialog::show (services::ContentService& contentService, std::function<void()> onContentChanged)
{
    auto* dialog = new ContentManagerDialog (contentService); // JUCE-API verlangt besessenen Zeiger
    dialog->onContentChanged = std::move (onContentChanged);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = TRANS ("Content Manager");
    options.dialogBackgroundColour = theme::backgroundBase;
    options.content.setOwned (dialog);
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace fk::ui
