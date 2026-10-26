#include "fk/services/AutomationService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MixerService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ArrangeView.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("ArrangeView: Audiodateien per Drag-and-drop landen als Clips an der Abwurfstelle", "[ui][drop]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Drop").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);

    fk::services::MarkerService markers { env.projectService };
    fk::ui::ArrangeView view ({ env.trackService,
                                env.clipService,
                                midiClips,
                                transport,
                                env.timelineService,
                                waveforms,
                                mixer,
                                automation,
                                signatureService,
                                markers });
    view.setSize (900, 400);
    view.setSnapEnabled (false);
    TestEnvironment::pumpMessages (50);

    const juce::StringArray files { env.createSilentWav ("eins", 1.0).getFullPathName(),
                                    env.createSilentWav ("zwei", 1.0).getFullPathName() };
    REQUIRE (fk::ui::ArrangeView::isAudioFile (juce::File (files[0])));
    REQUIRE (view.isInterestedInFileDrag (files));
    REQUIRE_FALSE (view.isInterestedInFileDrag ({ "/tmp/notiz.txt" }));

    // Abwurf auf die erste Spur bei 2 Sekunden: x aus der Zeitabbildung, y in der ersten Zeile.
    const int x = 220 + view.getTimeMapping().secondsToX (2.0);
    const int y = 24 + fk::ui::SignatureTrack::totalHeight + 30;
    view.filesDropped (files, x, y);
    TestEnvironment::pumpMessages (50);

    const auto tracksNow = env.trackService.getAudioTracks();
    REQUIRE (tracksNow.size() == 2); // zweite Datei auf neuer Spur darunter
    const auto first = env.trackService.getClips (*vocals);
    REQUIRE (first.size() == 1);
    REQUIRE (first.front().startSeconds == Catch::Approx (2.0).margin (0.05));
    REQUIRE (first.front().audioFile.isAChildOf (env.projectService.getProjectFolder()));
    REQUIRE (env.trackService.getClips (*tracksNow[1]).size() == 1);

    // Ziehen aus dem Medien-Browser: Beschreibung mit Präfix und Pfaden, Abwurf wie eine Datei.
    juce::DragAndDropTarget::SourceDetails details (juce::String ("fk-media:") + files[0], &view, { x, y });
    REQUIRE (view.isInterestedInDragSource (details));
    REQUIRE_FALSE (view.isInterestedInDragSource ({ juce::String ("etwas-anderes"), &view, { x, y } }));
    view.itemDropped (details);
    REQUIRE (env.trackService.getClips (*vocals).size() == 2);

    // Abwurf ins Leere: neue Spur.
    view.filesDropped ({ files[0] }, x, 390);
    REQUIRE (env.trackService.getAudioTracks().size() == 3);
}

TEST_CASE ("ArrangeView: MIDI-Datei per Drag-and-drop legt je Spur der Datei eine Instrumentenspur an",
           "[ui][drop][midi]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "MidiDrop").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);

    fk::services::MarkerService markers { env.projectService };
    fk::ui::ArrangeView view ({ env.trackService,
                                env.clipService,
                                midiClips,
                                transport,
                                env.timelineService,
                                waveforms,
                                mixer,
                                automation,
                                signatureService,
                                markers });
    view.createInstrumentTrack = [&] (const juce::String& name) -> fk::services::TrackHandle*
    {
        const auto created = env.trackService.createInstrumentTrack (name, TestEnvironment::synthId (pluginService));
        return created ? *created : nullptr;
    };
    view.setSize (900, 400);
    view.setSnapEnabled (false);
    TestEnvironment::pumpMessages (50);

    const auto midiFile = env.createMidiFile ("lied");
    REQUIRE (fk::ui::ArrangeView::isImportableFile (midiFile));
    REQUIRE (view.isInterestedInFileDrag ({ midiFile.getFullPathName() }));

    // Abwurf auf die Audiospur: passt nicht, also zwei neue Instrumentenspuren (zwei Spuren in der Datei).
    const int x = 220 + view.getTimeMapping().secondsToX (2.0);
    const int y = 24 + fk::ui::SignatureTrack::totalHeight + 30;
    view.filesDropped ({ midiFile.getFullPathName() }, x, y);
    TestEnvironment::pumpMessages (100);

    const auto tracksNow = env.trackService.getAudioTracks();
    REQUIRE (tracksNow.size() == 3);
    REQUIRE (env.trackService.getClips (*vocals).empty());
    REQUIRE (env.trackService.getTrackInfo (*tracksNow[1]).isInstrument);
    REQUIRE (env.trackService.getTrackInfo (*tracksNow[2]).isInstrument);
    const auto firstClips = env.trackService.getClips (*tracksNow[1]);
    REQUIRE (firstClips.size() == 1);
    REQUIRE (firstClips[0].isMidi);
    REQUIRE (firstClips[0].notes.size() == 2);
    REQUIRE (firstClips[0].startSeconds == Catch::Approx (2.0).margin (0.05));
    REQUIRE (env.trackService.getClips (*tracksNow[2])[0].notes.size() == 1);
}

TEST_CASE ("ArrangeView: MIDI-Datei mit eigenem Tempo fragt nach und übernimmt es auf Wunsch", "[ui][drop][midi][a7]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "TempoDrop").has_value());

    fk::services::MarkerService markers { env.projectService };
    fk::ui::ArrangeView view ({ env.trackService,
                                env.clipService,
                                midiClips,
                                transport,
                                env.timelineService,
                                waveforms,
                                mixer,
                                automation,
                                signatureService,
                                markers });
    view.createInstrumentTrack = [&] (const juce::String& name) -> fk::services::TrackHandle*
    {
        const auto created = env.trackService.createInstrumentTrack (name, TestEnvironment::synthId (pluginService));
        return created ? *created : nullptr;
    };
    view.setSize (900, 400);
    view.setSnapEnabled (false);
    TestEnvironment::pumpMessages (50);

    // Datei mit 90 BPM.
    const auto file = env.tempFolder.getChildFile ("langsam.mid");
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence track;
        track.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60.0e6 / 90.0)), 0.0);
        track.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0.0);
        track.addEvent (juce::MidiMessage::noteOff (1, 60), 4.0 * 480.0); // ein Takt
        track.updateMatchedPairs();
        midi.addTrack (track);
        juce::FileOutputStream out (file);
        REQUIRE (midi.writeTo (out));
    }

    int questions = 0;
    double askedBpm = 0.0;
    bool adopt = false;
    view.onAskMidiTempo = [&] (const juce::String&, double bpm, std::function<void (bool)> answer)
    {
        ++questions;
        askedBpm = bpm;
        answer (adopt);
    };

    const int y = 24 + fk::ui::SignatureTrack::totalHeight + 30;

    // Behalten: Projekt bleibt bei 120, die Note ist ein Takt in Schlägen (2 s).
    view.importFiles ({ file.getFullPathName() }, 220, y);
    TestEnvironment::pumpMessages (50);
    REQUIRE (questions == 1);
    REQUIRE (askedBpm == Catch::Approx (90.0).margin (0.01));
    REQUIRE (signatureService.getTempoAt (0.0) == Catch::Approx (120.0));

    // Übernehmen: Tempo 90, der Clip ist dann ein Takt bei 90 BPM lang (2,67 s).
    adopt = true;
    view.importFiles ({ file.getFullPathName() }, 220, y + 200);
    TestEnvironment::pumpMessages (50);
    REQUIRE (questions == 2);
    REQUIRE (signatureService.getTempoAt (0.0) == Catch::Approx (90.0).margin (0.01));

    const auto tracksNow = env.trackService.getAudioTracks();
    REQUIRE (tracksNow.size() == 2);
    const auto adopted = env.trackService.getClips (*tracksNow.back());
    REQUIRE (adopted.size() == 1);
    REQUIRE (adopted[0].lengthSeconds == Catch::Approx (8.0 / 3.0).margin (0.05));

    // Jetzt passt das Projekt zur Datei: keine Frage mehr.
    view.importFiles ({ file.getFullPathName() }, 220, y + 200);
    TestEnvironment::pumpMessages (50);
    REQUIRE (questions == 2);
}
