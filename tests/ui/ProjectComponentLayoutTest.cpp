#include "fk/services/AutomationService.h"
#include "fk/services/AutosaveService.h"
#include "fk/services/BakeService.h"
#include "fk/services/DiagnosticsService.h"
#include "fk/services/ExportService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/ContentService.h"
#include "fk/services/DrumMapService.h"
#include "fk/services/ExpressionMapService.h"
#include "fk/services/ExternalMidiService.h"
#include "fk/services/LibraryService.h"
#include "fk/services/RackService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/UpdateService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/MidiLearnService.h"
#include "fk/services/MidiService.h"
#include "fk/services/MixerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PreviewService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/SendService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TemplateService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/IconButton.h"
#include "fk/ui/LowerZone.h"
#include "fk/ui/ProjectComponent.h"
#include "fk/ui/TransportBar.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{
/// Alle Services, die das Projektfenster braucht, in einer Testumgebung.
struct FullServices
{
    explicit FullServices (TestEnvironment& testEnv)
        : env (testEnv)
    {
    }

    TestEnvironment& env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiService midi { env.engineContext };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::MidiControllerService midiControllers { env.projectService };
    fk::services::NotePreviewService notePreview { env.projectService };
    fk::services::BakeService bake { env.projectService, env.trackService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::SendService sends { env.projectService, env.trackService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signature { env.projectService };
    fk::services::PreviewService preview { env.engineContext };
    fk::services::MidiLearnService midiLearn { env.engineContext, env.projectService };
    fk::services::AutosaveService autosave { env.projectService, env.engineContext };
    fk::services::ProjectSettingsService projectSettings { env.engineContext, env.projectService };
    fk::services::TemplateService templates { env.projectService, env.tempFolder.getChildFile ("Vorlagen") };
    fk::services::DiagnosticsService diagnostics { env.engineContext,
                                                   env.projectService,
                                                   { env.tempFolder.getChildFile ("Logs"),
                                                     env.tempFolder.getChildFile ("Haenger"),
                                                     env.tempFolder.getChildFile ("Absturz") } };
    fk::services::MarkerService markers { env.projectService };
    fk::services::RackService racks { env.projectService, env.trackService };
    fk::services::ExternalMidiService externalMidi { env.projectService, env.trackService, transport };
    fk::services::DrumMapService drumMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("DrumMaps") };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    fk::services::ExpressionMapService expressionMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("ExpressionMaps") };
    fk::services::ContentService content { env.tempFolder.getChildFile ("Content") };
    fk::services::LibraryService library { plugins, content, sampler, env.trackService, env.tempFolder.getChildFile ("Bibliothek.json") };
    fk::services::UpdateService updates { {}, "0.1.0" };

    fk::ui::UiServices ui()
    {
        return { env.engineContext,
                 env.projectService,
                 transport,
                 env.timelineService,
                 env.trackService,
                 env.clipService,
                 waveforms,
                 plugins,
                 mixer,
                 midi,
                 midiClips,
                 midiControllers,
                 notePreview,
                 bake,
                 exporter,
                 sends,
                 automation,
                 signature,
                 preview,
                 midiLearn,
                 autosave,
                 projectSettings,
                 templates,
                 diagnostics,
                 markers,
                 racks,
                 externalMidi,
                 drumMaps,
                 sampler,
                 expressionMaps,
                 content,
                 library,
                 updates };
    }
};
} // namespace

TEST_CASE ("ProjectComponent: Werkzeugleiste passt in schmale und breite Fenster", "[ui][layout]")
{
    TestEnvironment env;
    FullServices services (env);
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    REQUIRE (env.projectService.createProject (env.tempFolder, "Layout").has_value());
    auto* drums = *env.trackService.createFolderTrack ("Drums");
    auto* kick = *env.trackService.createAudioTrack ("Kick", 1);
    REQUIRE (env.trackService.createAudioTrack ("Bass").has_value());
    REQUIRE (env.trackService.moveIntoFolder (*kick, drums).has_value());
    REQUIRE (env.clipService.importAudioFile (*kick, env.createSilentWav ("kick", 2.0), 1.0).has_value());

    {
        fk::ui::ProjectComponent component (services.ui());
        const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

        // Sample-Editor mit dem Clip zeigen (Doppelklick im Arrangement tut dasselbe).
        for (auto* child : component.getChildren())
            if (auto* lowerZone = dynamic_cast<fk::ui::LowerZone*> (child))
                lowerZone->showSampleEditor();

        for (const int width : { 1024, 1280, 1920 })
        {
            component.setSize (width, 800);
            TestEnvironment::pumpMessages (50);

            // Jeder Button der Werkzeugleiste ist sichtbar, ragt nicht aus dem Fenster und ist nicht zusammengedrückt.
            for (auto* child : component.getChildren())
                if (auto* button = dynamic_cast<fk::ui::IconButton*> (child))
                {
                    INFO ("Breite " << width << ", Button: " << button->getTooltip());
                    REQUIRE (button->isVisible());
                    REQUIRE (button->getRight() <= width);
                    REQUIRE (button->getWidth() >= 24);
                }

            // Transportleiste: alle Elemente innerhalb der Leiste, die Leiste selbst im Fenster.
            for (auto* child : component.getChildren())
                if (auto* bar = dynamic_cast<fk::ui::TransportBar*> (child))
                {
                    REQUIRE (bar->getBottom() <= component.getHeight());

                    for (auto* element : bar->getChildren())
                    {
                        INFO ("Breite " << width << ", Transportleiste: " << element->getName());
                        REQUIRE (element->isVisible());
                        REQUIRE (element->getRight() <= bar->getWidth());
                        REQUIRE (element->getBottom() <= bar->getHeight());
                        REQUIRE (element->getWidth() >= 24);
                    }
                }

            if (directory.isNotEmpty())
            {
                REQUIRE (juce::File (directory).createDirectory());
                const auto image = component.createComponentSnapshot (component.getLocalBounds());
                // FileOutputStream hängt an vorhandene Dateien an: vorher löschen, sonst bleibt das alte Bild vorn.
                const auto file =
                    juce::File (directory).getChildFile ("ProjectComponent_" + juce::String (width) + ".png");
                file.deleteFile();
                juce::FileOutputStream stream (file);
                REQUIRE (stream.openedOk());
                REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
            }
        }
    }

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
