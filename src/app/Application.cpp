#include "Application.h"

#include "fk/core/BuildInfo.h"
#include "fk/engine/EngineContext.h"
#include "fk/plugins/i18n/FoundationTranslations.h"
#include "fk/plugins/i18n/LanguageSetting.h"
#include "fk/plugins/i18n/Translator.h"
#include "fk/services/AutomationService.h"
#include "fk/services/AutosaveService.h"
#include "fk/services/BakeService.h"
#include "fk/services/BundledPlugins.h"
#include "fk/services/ClipService.h"
#include "fk/services/ContentService.h"
#include "fk/services/DiagnosticsService.h"
#include "fk/services/DrumMapService.h"
#include "fk/services/ExportService.h"
#include "fk/services/ExpressionMapService.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/LibraryService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/MidiService.h"
#include "fk/services/MixerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/PreviewService.h"
#include "fk/services/ProjectService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/RackService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/SendService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TemplateService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"
#include "fk/services/TransportService.h"
#include "fk/services/UpdateService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/ProjectWindow.h"

#include <algorithm>
#include <iostream>

#include "FkDawTranslations.h"
#include "HangWatchdog.h"

namespace fk::app
{

namespace
{
constexpr int maxLogFiles = 20;

/// ~/.config/FKAudioSuite (Linux), %APPDATA%/FKAudioSuite (Windows): Logs, Berichte, Vorlagen.
juce::File appDataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("FKAudioSuite");
}

juce::File crashReportFolder()
{
    return appDataFolder().getChildFile ("Absturz");
}

/// Nach einem Absturz (Signal): Stack-Trace in eine Datei. Läuft im Signal-Handler, also so wenig wie möglich.
void writeCrashReport (void*)
{
    const auto file = crashReportFolder().getChildFile (
        "Absturz " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".txt");
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::SystemStats::getStackBacktrace());
    juce::Logger::writeToLog ("Absturz, Bericht: " + file.getFullPathName());
}

void deleteOldestFiles (const juce::File& folder, int keep)
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.log");
    std::sort (files.begin(),
               files.end(),
               [] (const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });

    for (int i = keep; i < files.size(); ++i)
        files.getReference (i).deleteFile();
}
} // namespace

Application::Application() = default;
Application::~Application() = default;

const juce::String Application::getApplicationName()
{
    return JUCE_APPLICATION_NAME_STRING;
}

const juce::String Application::getApplicationVersion()
{
    // Version mit Git-Stand, damit Logs und Diagnose-Berichte den genauen Build nennen.
    return juce::String (core::buildInfo::version) + " (" + core::buildInfo::channel + ", " + core::buildInfo::gitSha +
           ")";
}

bool Application::moreThanOneInstanceAllowed()
{
    // Muss true sein: Der Plugin-Scan startet eine zweite Instanz dieser App als Helfer,
    // und JUCE würde sie sonst still an die erste weiterreichen und beenden.
    return true;
}

void Application::initialise (const juce::String& commandLine)
{
    // Tracktion startet zum Plugin-Scan eine zweite Instanz dieser App mit einer besonderen
    // Kommandozeile. Diese Instanz macht nur den Scan und beendet sich selbst; kein Fenster, keine Engine.
    if (engine::EngineContext::handlePluginScanWorkerStartup (commandLine))
        return;

    // "--scan-plugins": Scan ohne Oberfläche, Ergebnis auf der Konsole, dann Ende.
    // Nützlich für Skripte, Fehlersuche und die Prüfung des Unterprozess-Scans.
    if (commandLine.contains ("--scan-plugins"))
    {
        runHeadlessPluginScan();
        quit();
        return;
    }

    startLogging();
    applyLanguage(); // vor dem ersten Fenster: Menüs und Dialoge entstehen gleich in der richtigen Sprache

    lookAndFeel = std::make_unique<ui::FkLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

    engineContext = std::make_unique<engine::EngineContext> (getApplicationName());
    engineContext->registerInternalPlugins();
    engineContext->enableAllAudioInputs();
    engineContext->setProgressWindowsEnabled (true); // Bake zeigt einen Fortschrittsbalken

    // Hänger-Berichte in ~/.config/FKAudioSuite/Haenger (Linux), mit Stack-Traces aller Threads.
    hangWatchdog = std::make_unique<HangWatchdog> (
        appDataFolder().getChildFile ("Haenger"),
        HangWatchdog::AudioProbe { [this] { return engineContext->getDeviceManager().getCpuUsage(); },
                                   [this] { return engineContext->getDeviceManager().getCurrentStreamTime(); } });

    projectService = std::make_unique<services::ProjectService> (*engineContext);
    autosaveService = std::make_unique<services::AutosaveService> (*projectService, *engineContext);
    transportService = std::make_unique<services::TransportService> (*projectService);
    timelineService = std::make_unique<services::TimelineService> (*projectService);
    trackService = std::make_unique<services::TrackService> (*projectService);
    clipService = std::make_unique<services::ClipService> (*projectService);
    waveformService = std::make_unique<services::WaveformService> (*projectService);
    pluginService = std::make_unique<services::PluginService> (*engineContext, *projectService);
    installBundledPlugins();
    mixerService = std::make_unique<services::MixerService> (*projectService);
    midiService = std::make_unique<services::MidiService> (*engineContext);
    midiService->ensureVirtualKeyboardExists();
    midiClipService = std::make_unique<services::MidiClipService> (*projectService);
    midiControllerService = std::make_unique<services::MidiControllerService> (*projectService);
    notePreviewService = std::make_unique<services::NotePreviewService> (*projectService);
    bakeService = std::make_unique<services::BakeService> (*projectService, *trackService);
    exportService = std::make_unique<services::ExportService> (*projectService);
    sendService = std::make_unique<services::SendService> (*projectService, *trackService);
    automationService = std::make_unique<services::AutomationService> (*projectService);
    signatureService = std::make_unique<services::SignatureService> (*projectService);
    previewService = std::make_unique<services::PreviewService> (*engineContext);
    midiLearnService = std::make_unique<services::MidiLearnService> (*engineContext, *projectService);
    projectSettingsService = std::make_unique<services::ProjectSettingsService> (*engineContext, *projectService);
    templateService =
        std::make_unique<services::TemplateService> (*projectService, appDataFolder().getChildFile ("Vorlagen"));
    diagnosticsService = std::make_unique<services::DiagnosticsService> (
        *engineContext,
        *projectService,
        services::DiagnosticsFolders {
            appDataFolder().getChildFile ("Logs"), appDataFolder().getChildFile ("Haenger"), crashReportFolder() });
    markerService = std::make_unique<services::MarkerService> (*projectService);
    rackService = std::make_unique<services::RackService> (*projectService, *trackService);
    externalMidiService =
        std::make_unique<services::ExternalMidiService> (*projectService, *trackService, *transportService);
    drumMapService = std::make_unique<services::DrumMapService> (
        *projectService, *trackService, appDataFolder().getChildFile ("DrumMaps"));
    samplerService = std::make_unique<services::SamplerService> (*projectService, *trackService, *pluginService);
    expressionMapService = std::make_unique<services::ExpressionMapService> (
        *projectService, *trackService, appDataFolder().getChildFile ("ExpressionMaps"));
    contentService = std::make_unique<services::ContentService> (appDataFolder().getChildFile ("Content"));
    updateService = std::make_unique<services::UpdateService> (
        core::buildInfo::updateUrl, core::buildInfo::version, appDataFolder().getChildFile ("Updates.txt"));
    libraryService = std::make_unique<services::LibraryService> (*pluginService,
                                                                 *contentService,
                                                                 *samplerService,
                                                                 *trackService,
                                                                 appDataFolder().getChildFile ("Bibliothek.json"));

    projectWindow = std::make_unique<ui::ProjectWindow> (
        getApplicationName(),
        ui::UiServices {
            *engineContext,   *projectService,         *transportService,   *timelineService,    *trackService,
            *clipService,     *waveformService,        *pluginService,      *mixerService,       *midiService,
            *midiClipService, *midiControllerService,  *notePreviewService, *bakeService,        *exportService,
            *sendService,     *automationService,      *signatureService,   *previewService,     *midiLearnService,
            *autosaveService, *projectSettingsService, *templateService,    *diagnosticsService, *markerService,
            *rackService,     *externalMidiService,    *drumMapService,     *samplerService,     *expressionMapService,
            *contentService,  *libraryService,         *updateService });
}

void Application::applyLanguage()
{
    const plugins::i18n::LanguageSetting setting { plugins::i18n::SharedSettings (
        plugins::i18n::SharedSettings::defaultFile()) };
    const auto language = setting.resolve();

    auto tables = plugins::i18n::foundationTranslations();
    tables.push_back ({ plugins::i18n::Language::german,
                        { FkDawTranslations::de_txt, static_cast<std::size_t> (FkDawTranslations::de_txtSize) } });
    plugins::i18n::Translator::apply (language, tables);

    juce::Logger::writeToLog ("Sprache: " + plugins::i18n::toCode (language) + " (System "
                              + juce::SystemStats::getUserLanguage() + ")");
}

void Application::startLogging()
{
    // Eine Log-Datei je Start; Tracktion und JUCE schreiben über juce::Logger mit hinein.
    const auto folder = appDataFolder().getChildFile ("Logs");
    folder.createDirectory();
    deleteOldestFiles (folder, maxLogFiles - 1);

    logger = std::make_unique<juce::FileLogger> (
        folder.getChildFile ("FK Audio Suite " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".log"),
        getApplicationName() + " " + getApplicationVersion() + ", " + juce::SystemStats::getOperatingSystemName(),
        0);
    juce::Logger::setCurrentLogger (logger.get());
    juce::SystemStats::setApplicationCrashHandler (writeCrashReport);
}

void Application::installBundledPlugins()
{
    // FK Rig Lite und Co. liegen im Paket (ADR 0008 §3). Nur beim ersten Start einer neuen Version
    // wird kopiert und gescannt; danach sind die Plugins bekannt und das hier kostet nichts.
    const services::BundledPlugins bundled (services::BundledPlugins::defaultFolders(),
                                            juce::String (core::buildInfo::version) + "+" + core::buildInfo::gitSha);
    const auto plugins = bundled.install();

    if (! plugins)
    {
        juce::Logger::writeToLog (juce::String::fromUTF8 (("Mitgelieferte Plugins: " + plugins.error().message).c_str()));
        return;
    }

    if (const auto added = pluginService->addPluginFiles (*plugins); added > 0)
        juce::Logger::writeToLog ("Mitgelieferte Plugins neu bekannt: " + juce::String (added));
}

void Application::runHeadlessPluginScan()
{
    engineContext = std::make_unique<engine::EngineContext> (getApplicationName());
    engineContext->registerInternalPlugins();
    projectService = std::make_unique<services::ProjectService> (*engineContext);
    pluginService = std::make_unique<services::PluginService> (*engineContext, *projectService);

    const auto result =
        pluginService->scanAllFormats ([] (const juce::String& file) { std::cout << "  scan: " << file << '\n'; });

    std::cout << "Plugins vorher: " << result.numPluginsBefore << ", nachher: " << result.numPluginsAfter << '\n';

    for (const auto& failed : result.failedFiles)
        std::cout << "  fehlgeschlagen: " << failed << '\n';

    for (const auto& effect : pluginService->getAvailableEffects())
        std::cout << "  " << effect.format << " | " << effect.manufacturer << " | " << effect.name << '\n';
}

void Application::shutdown()
{
    // Umgekehrte Aufbaureihenfolge, explizit: Fenster weg, Projekt zu, dann Engine.
    projectWindow.reset();
    hangWatchdog.reset();
    libraryService.reset();
    updateService.reset();
    contentService.reset();
    expressionMapService.reset();
    samplerService.reset();
    drumMapService.reset();
    externalMidiService.reset();
    rackService.reset();
    markerService.reset();
    diagnosticsService.reset();
    templateService.reset();
    projectSettingsService.reset();
    midiLearnService.reset();
    // Alle Services vor der Engine abbauen: PreviewService und MidiService melden sich in ihren
    // Destruktoren bei der Geräteverwaltung ab; nach engineContext.reset() gäbe es die nicht mehr
    // (Absturz beim Beenden, Assertion in tracktion_Engine.cpp: deviceManager == nullptr).
    previewService.reset();
    signatureService.reset();
    automationService.reset();
    sendService.reset();
    exportService.reset();
    bakeService.reset();
    notePreviewService.reset(); // beendet klingende Vorhör-Noten, braucht ProjectService
    midiControllerService.reset();
    midiClipService.reset();
    midiService.reset();
    mixerService.reset();
    pluginService.reset();
    waveformService.reset();
    clipService.reset();
    trackService.reset();
    timelineService.reset();
    transportService.reset();
    autosaveService.reset();
    projectService.reset();
    engineContext.reset();

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    lookAndFeel.reset();

    juce::Logger::writeToLog ("Beendet");
    juce::Logger::setCurrentLogger (nullptr);
    logger.reset();
}

void Application::systemRequestedQuit()
{
    // Erst nach ungespeicherten Änderungen fragen; Abbrechen lässt die App offen.
    if (projectWindow != nullptr)
        projectWindow->closeProjectThen ([] { quit(); });
    else
        quit();
}

} // namespace fk::app
