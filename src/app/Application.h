#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace fk::engine
{
class EngineContext;
}

namespace fk::services
{
class ProjectService;
class AutosaveService;
class ProjectSettingsService;
class TemplateService;
class DiagnosticsService;
class MarkerService;
class RackService;
class ExternalMidiService;
class DrumMapService;
class SamplerService;
class ExpressionMapService;
class ContentService;
class LibraryService;
class UpdateService;
class TransportService;
class TimelineService;
class TrackService;
class ClipService;
class WaveformService;
class PluginService;
class MixerService;
class MidiService;
class MidiClipService;
class MidiControllerService;
class NotePreviewService;
class BakeService;
class ExportService;
class SendService;
class AutomationService;
class SignatureService;
class PreviewService;
class MidiLearnService;
} // namespace fk::services

namespace fk::ui
{
class FkLookAndFeel;
class ProjectWindow;
} // namespace fk::ui

namespace fk::app
{
class HangWatchdog;

/// Lebenszyklus der Anwendung und "Composition Root": Hier, und nur hier, werden Engine,
/// Services und Fenster erzeugt und zusammengesteckt (ARCHITECTURE.md §3).
///
/// Alle Member sind unique_ptr auf vorwärtsdeklarierte Typen: Der Header bleibt leicht,
/// und die Reihenfolge der Member bestimmt Aufbau und Abbau (→ LEARNING.md §4, §6.5).
class Application final : public juce::JUCEApplication
{
public:
    Application();
    ~Application() override;

    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;

private:
    void runHeadlessPluginScan();
    void installBundledPlugins();
    void startLogging();
    /// Oberflächensprache aus der gemeinsamen Einstellung, sonst nach Systemsprache (ADR 0010).
    void applyLanguage();

    // Deklarationsreihenfolge = Aufbaureihenfolge; Abbau läuft rückwärts, also Fenster zuerst.
    std::unique_ptr<juce::FileLogger> logger; ///< zuerst gebaut, zuletzt abgebaut: protokolliert alles dazwischen
    std::unique_ptr<ui::FkLookAndFeel> lookAndFeel;
    std::unique_ptr<engine::EngineContext> engineContext;
    std::unique_ptr<HangWatchdog> hangWatchdog;
    std::unique_ptr<services::ProjectService> projectService;
    std::unique_ptr<services::AutosaveService> autosaveService;
    std::unique_ptr<services::TransportService> transportService;
    std::unique_ptr<services::TimelineService> timelineService;
    std::unique_ptr<services::TrackService> trackService;
    std::unique_ptr<services::ClipService> clipService;
    std::unique_ptr<services::WaveformService> waveformService;
    std::unique_ptr<services::PluginService> pluginService;
    std::unique_ptr<services::MixerService> mixerService;
    std::unique_ptr<services::MidiService> midiService;
    std::unique_ptr<services::MidiClipService> midiClipService;
    std::unique_ptr<services::MidiControllerService> midiControllerService;
    std::unique_ptr<services::NotePreviewService> notePreviewService;
    std::unique_ptr<services::BakeService> bakeService;
    std::unique_ptr<services::ExportService> exportService;
    std::unique_ptr<services::SendService> sendService;
    std::unique_ptr<services::AutomationService> automationService;
    std::unique_ptr<services::SignatureService> signatureService;
    std::unique_ptr<services::PreviewService> previewService;
    std::unique_ptr<services::MidiLearnService> midiLearnService;
    std::unique_ptr<services::ProjectSettingsService> projectSettingsService;
    std::unique_ptr<services::TemplateService> templateService;
    std::unique_ptr<services::DiagnosticsService> diagnosticsService;
    std::unique_ptr<services::MarkerService> markerService;
    std::unique_ptr<services::RackService> rackService;
    std::unique_ptr<services::ExternalMidiService> externalMidiService;
    std::unique_ptr<services::DrumMapService> drumMapService;
    std::unique_ptr<services::SamplerService> samplerService;
    std::unique_ptr<services::ExpressionMapService> expressionMapService;
    std::unique_ptr<services::ContentService> contentService;
    std::unique_ptr<services::LibraryService> libraryService;
    std::unique_ptr<services::UpdateService> updateService;
    std::unique_ptr<ui::ProjectWindow> projectWindow;
};

} // namespace fk::app
