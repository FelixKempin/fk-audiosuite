#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/SampleEditor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("SampleEditor: Schnappschuss mit Clip und Auswahl als PNG (FK_UI_SNAPSHOT_DIR)", "[ui][snapshot]")
{
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

    if (directory.isEmpty())
        SKIP ("FK_UI_SNAPSHOT_DIR nicht gesetzt");

    TestEnvironment env;
    fk::services::WaveformService waveforms { env.projectService };
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    REQUIRE (env.projectService.createProject (env.tempFolder, "Sample").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gitarre", 1);

    // Abklingende Töne, damit die Wellenform Form hat.
    const auto file = env.tempFolder.getChildFile ("toene.wav");
    {
        juce::AudioBuffer<float> buffer (1, 48000 * 3);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto t = static_cast<float> (i % 24000) / 48000.0f;
            buffer.setSample (0,
                              i,
                              0.8f * std::exp (-6.0f * t) *
                                  std::sin (2.0f * 3.14159f * 220.0f * static_cast<float> (i) / 48000.0f));
        }

        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = format.createWriterFor (
            stream,
            juce::AudioFormatWriterOptions {}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    const auto clipId = *env.clipService.importAudioFile (*track, file, 0.0);

    {
        fk::ui::SampleEditor editor (env.clipService, env.trackService, waveforms);
        editor.setSize (900, 260);
        editor.setClip (env.trackService.getTrackId (*track), clipId);
        TestEnvironment::pumpMessages (600); // Wellenform rechnet im Hintergrund

        // Auswahl per Maus: Ziehen von 25 % bis 45 % der Breite.
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto event = [&] (float x, float downX)
        {
            return juce::MouseEvent (source,
                                     { x, 130.0f },
                                     juce::ModifierKeys::leftButtonModifier,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     &editor,
                                     &editor,
                                     juce::Time::getCurrentTime(),
                                     { downX, 130.0f },
                                     juce::Time::getCurrentTime(),
                                     1,
                                     x != downX);
        };
        editor.mouseDown (event (225.0f, 225.0f));
        editor.mouseDrag (event (405.0f, 225.0f));
        editor.mouseUp (event (405.0f, 225.0f));

        const juce::File folder (directory);
        REQUIRE (folder.createDirectory());
        const auto target = folder.getChildFile ("SampleEditor.png");
        target.deleteFile();
        juce::FileOutputStream stream (target);
        REQUIRE (juce::PNGImageFormat().writeImageToStream (editor.createComponentSnapshot (editor.getLocalBounds()),
                                                            stream));
    }

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
