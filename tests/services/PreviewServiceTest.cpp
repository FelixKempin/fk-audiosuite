#include "fk/services/PreviewService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("PreviewService: spielt eine Datei vor, kennt ihre Länge und stoppt", "[services][preview]")
{
    TestEnvironment env;
    fk::services::PreviewService preview { env.engineContext };
    const auto wav = env.createSilentWav ("vorschau", 2.0);

    REQUIRE (preview.getFileLengthSeconds (wav) == Catch::Approx (2.0).margin (0.01));
    REQUIRE (preview.getFileLengthSeconds (env.tempFolder.getChildFile ("fehlt.wav")) == Catch::Approx (0.0));
    REQUIRE_FALSE (preview.play (env.tempFolder.getChildFile ("fehlt.wav")).has_value());

    REQUIRE (preview.play (wav).has_value());
    REQUIRE (preview.isPlaying());
    REQUIRE (preview.getCurrentFile() == wav);
    REQUIRE (preview.getLengthSeconds() == Catch::Approx (2.0).margin (0.01));

    preview.stop();
    REQUIRE_FALSE (preview.isPlaying());
    REQUIRE (preview.getPositionSeconds() == Catch::Approx (0.0).margin (0.01));
}
