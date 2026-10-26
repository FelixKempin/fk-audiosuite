#include "fk/engine/FkUIBehaviour.h"
#include "fk/plugins/i18n/Text.h"

#include <atomic>

namespace fk::engine
{

void FkUIBehaviour::setFocusedEdit (te::Edit* edit) noexcept
{
    focusedEdit = edit;
}

te::Edit* FkUIBehaviour::getCurrentlyFocusedEdit()
{
    return focusedEdit;
}

te::Edit* FkUIBehaviour::getLastFocusedEdit()
{
    return focusedEdit;
}

juce::Array<te::Edit*> FkUIBehaviour::getAllOpenEdits()
{
    if (focusedEdit == nullptr)
        return {};

    return { focusedEdit };
}

namespace
{
/// Führt eine Tracktion-Aufgabe in einem eigenen Thread aus und zeigt dabei ein Fenster mit
/// Fortschrittsbalken und Abbrechen-Button. runThread() kehrt erst nach dem Ende zurück.
class TaskProgressWindow final : public juce::ThreadWithProgressWindow
{
public:
    TaskProgressWindow (te::ThreadPoolJobWithProgress& jobToRun, const juce::String& title)
        : juce::ThreadWithProgressWindow (title, true, true)
        , job (jobToRun)
    {
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            if (job.runJob() != juce::ThreadPoolJob::jobNeedsRunningAgain)
                return;

            setProgress (job.getCurrentTaskProgress());
        }

        cancelled = true;
    }

    [[nodiscard]] bool wasCancelled() const noexcept { return cancelled; }

private:
    te::ThreadPoolJobWithProgress& job;
    std::atomic<bool> cancelled { false };
};

// Tracktion beschreibt die Aufgabe englisch, mit dem Spurnamen in Anführungszeichen.
juce::String windowTitleFor (const te::ThreadPoolJobWithProgress& job)
{
    const auto name =
        job.getJobName().fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
    return name.isEmpty() ? TRANS ("Rendering...")
                          : plugins::i18n::format (TRANS ("\"{name}\" is being baked..."), { { "name", name } });
}
} // namespace

void FkUIBehaviour::runTaskWithProgressBar (te::ThreadPoolJobWithProgress& task)
{
    lastTaskCancelled = false;

    if (! progressWindowsEnabled || ! juce::MessageManager::existsAndIsCurrentThread())
    {
        while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
        }

        return;
    }

    TaskProgressWindow window (task, windowTitleFor (task));
    window.runThread();
    lastTaskCancelled = window.wasCancelled();
}

void FkUIBehaviour::setProgressWindowsEnabled (bool shouldShow) noexcept
{
    progressWindowsEnabled = shouldShow;
}

bool FkUIBehaviour::wasLastTaskCancelled() const noexcept
{
    return lastTaskCancelled;
}

} // namespace fk::engine
