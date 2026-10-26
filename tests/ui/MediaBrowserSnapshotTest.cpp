#include "fk/services/PreviewService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/MediaBrowser.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("MediaBrowser: Schnappschuss als PNG (FK_UI_SNAPSHOT_DIR)", "[ui][snapshot]")
{
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

    if (directory.isEmpty())
        SKIP ("FK_UI_SNAPSHOT_DIR nicht gesetzt");

    TestEnvironment env;
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::PreviewService preview { env.engineContext };
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    REQUIRE (env.projectService.createProject (env.tempFolder, "Medien").has_value());

    fk::ui::MediaBrowser browser (preview, waveforms);
    browser.setSize (700, 360);
    TestEnvironment::pumpMessages (400); // Ordner-Scan läuft im Hintergrund

    const juce::File folder (directory);
    REQUIRE (folder.createDirectory());
    const auto image = browser.createComponentSnapshot (browser.getLocalBounds());
    folder.getChildFile ("MediaBrowser.png").deleteFile(); // FileOutputStream hängt sonst an das alte Bild an
    juce::FileOutputStream stream (folder.getChildFile ("MediaBrowser.png"));
    REQUIRE (stream.openedOk());
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
