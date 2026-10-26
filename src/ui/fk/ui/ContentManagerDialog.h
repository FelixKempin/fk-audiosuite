#pragma once

#include "fk/services/ContentService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace fk::ui
{

/// Content-Manager: freie Bibliotheken laden und entfernen. Je Paket eine Zeile mit Beschreibung,
/// Lizenz, Größe und Status; der Download läuft in einem Hintergrund-Thread mit Fortschritt und Abbrechen.
class ContentManagerDialog final : public juce::Component, private juce::Timer
{
public:
    /// Nach erfolgreicher Installation oder Entfernung (Instrumentenlisten neu lesen).
    std::function<void()> onContentChanged;

    explicit ContentManagerDialog (services::ContentService& contentService);
    ~ContentManagerDialog() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    /// Für Tests: Installation eines Pakets starten und den Statustext einer Zeile lesen.
    void startInstall (const juce::String& packageId);
    [[nodiscard]] bool isBusy() const noexcept { return worker != nullptr; }
    [[nodiscard]] juce::String getStatusText (const juce::String& packageId) const;

    static void show (services::ContentService& contentService, std::function<void()> onContentChanged);

private:
    struct Row
    {
        juce::String packageId;
        juce::Label title;
        juce::Label details;
        juce::Label status;
        IconButton action { Icon::exportFile, {}, {} };
    };

    /// Ein Download im Hintergrund; das Ergebnis holt der Timer auf dem Message-Thread ab.
    class Worker final : public juce::Thread
    {
    public:
        Worker (services::ContentService& service, juce::String packageId);
        ~Worker() override;
        void run() override;

        const juce::String packageId;
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> finished { false };
        core::Result<void> result = {};

    private:
        services::ContentService& content;
    };

    void timerCallback() override;
    void refreshRows();
    void removePackage (const juce::String& packageId);

    services::ContentService& content;
    std::vector<std::unique_ptr<Row>> rows;
    std::unique_ptr<Worker> worker;
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };
    juce::TextButton cancelButton { "Abbrechen" };
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentManagerDialog)
};

} // namespace fk::ui
