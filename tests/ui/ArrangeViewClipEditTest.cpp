#include "fk/services/AutomationService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MixerService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ArrangeView.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "../services/TestEnvironment.h"

using fk::tests::TestEnvironment;

TEST_CASE ("ArrangeView: Clip-Auswahl und Aktionen (Kopieren, Einfügen, Duplizieren, Löschen)", "[ui][clipedit]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "ClipEdit").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* guitar = *env.trackService.createAudioTrack ("Gitarre", 1);
    const auto first = *env.clipService.importAudioFile (*vocals, env.createSilentWav ("eins", 1.0), 1.0);
    const auto second = *env.clipService.importAudioFile (*vocals, env.createSilentWav ("zwei", 1.0), 4.0);

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
    int selectionChanges = 0;
    juce::String lastError;
    view.onClipSelectionChanged = [&selectionChanges] { ++selectionChanges; };
    view.onError = [&lastError] (const juce::String& message) { lastError = message; };
    TestEnvironment::pumpMessages (50);

    // Ohne Auswahl geht nichts außer Einfügen, und das erst mit Inhalt in der Zwischenablage.
    REQUIRE (view.getSelectedClipIds().empty());
    REQUIRE_FALSE (view.canPerformClipAction (fk::ui::ClipAction::copy));
    REQUIRE_FALSE (view.canPerformClipAction (fk::ui::ClipAction::paste));

    // Klick wählt einen, Strg+Klick nimmt den zweiten dazu, Strg+Klick auf den ersten nimmt ihn weg.
    view.selectClip (first, juce::ModifierKeys());
    REQUIRE (view.getSelectedClipIds() == std::vector<juce::String> { first });
    view.selectClip (second, juce::ModifierKeys::ctrlModifier);
    REQUIRE (view.getSelectedClipIds().size() == 2);
    REQUIRE (view.canPerformClipAction (fk::ui::ClipAction::merge));
    view.selectClip (first, juce::ModifierKeys::ctrlModifier);
    REQUIRE (view.getSelectedClipIds() == std::vector<juce::String> { second });
    REQUIRE (selectionChanges == 3);

    // Kopieren, dann Einfügen bei 10 s auf der gewählten Spur (die zuletzt angelegte: Gitarre).
    view.performClipAction (fk::ui::ClipAction::copy);
    REQUIRE (view.canPerformClipAction (fk::ui::ClipAction::paste));
    transport.setPositionSeconds (10.0);
    view.performClipAction (fk::ui::ClipAction::paste);
    TestEnvironment::pumpMessages (50);
    const auto guitarClips = env.trackService.getClips (*guitar);
    REQUIRE (guitarClips.size() == 1);
    REQUIRE (guitarClips[0].startSeconds == Catch::Approx (10.0));
    REQUIRE (view.getSelectedClipIds() == std::vector<juce::String> { guitarClips[0].id }); // Kopie ist Auswahl

    // Duplizieren legt die Kopie direkt dahinter; Löschen entfernt die Auswahl und leert sie.
    view.performClipAction (fk::ui::ClipAction::duplicate);
    TestEnvironment::pumpMessages (50);
    REQUIRE (env.trackService.getClips (*guitar).size() == 2);
    view.selectAllClips();
    REQUIRE (view.getSelectedClipIds().size() == 4);
    view.performClipAction (fk::ui::ClipAction::remove);
    TestEnvironment::pumpMessages (50);
    REQUIRE (env.trackService.getClips (*guitar).empty());
    REQUIRE (env.trackService.getClips (*vocals).empty());
    REQUIRE (view.getSelectedClipIds().empty());
    REQUIRE (lastError.isEmpty());

    // Fehler kommen als Text beim Fenster an, nicht nur im Log.
    view.performClipAction (fk::ui::ClipAction::merge);
    REQUIRE (lastError.isNotEmpty());
}

namespace
{
juce::Component* findDescendantWithID (juce::Component& root, const juce::String& id)
{
    for (auto* child : root.getChildren())
    {
        if (child->getComponentID() == id)
            return child;

        if (auto* found = findDescendantWithID (*child, id))
            return found;
    }

    return nullptr;
}

juce::MouseEvent mouseEventAt (juce::Component& target,
                               juce::Point<int> screenPosition,
                               juce::Point<int> screenDownPosition,
                               juce::ModifierKeys extraMods = {})
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto local = target.getLocalPoint (nullptr, screenPosition).toFloat();
    const auto down = target.getLocalPoint (nullptr, screenDownPosition).toFloat();
    return { source,
             local,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | extraMods.getRawFlags()),
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             0.0f,
             &target,
             &target,
             juce::Time::getCurrentTime(),
             down,
             juce::Time::getCurrentTime(),
             1,
             local != down };
}
} // namespace

TEST_CASE ("ArrangeView: Clip per Maus auf die Spur darunter ziehen", "[ui][clipedit]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "DragTrack").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* guitar = *env.trackService.createAudioTrack ("Gitarre", 1);
    const auto clipId = *env.clipService.importAudioFile (*vocals, env.createSilentWav ("eins", 2.0), 1.0);

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
    view.setSnapEnabled (false);
    view.setSize (900, 400);
    view.addToDesktop (0); // Bildschirmkoordinaten brauchen ein Fenster
    view.setTopLeftPosition (0, 0);
    TestEnvironment::pumpMessages (100);

    auto* lane0 = findDescendantWithID (view, "lane0");
    auto* lane1 = findDescendantWithID (view, "lane1");
    REQUIRE (lane0 != nullptr);
    REQUIRE (lane1 != nullptr);

    // Mitte des Clips (1..3 s) in der ersten Spur anfassen, 40 px nach rechts und in die zweite Spur loslassen.
    const auto clipX = view.getTimeMapping().secondsToX (2.0);
    const auto down = lane0->localPointToGlobal (juce::Point<int> (clipX, lane0->getHeight() / 2));
    const auto up = lane1->localPointToGlobal (juce::Point<int> (clipX + 40, lane1->getHeight() / 2));

    lane0->mouseDown (mouseEventAt (*lane0, down, down));
    lane0->mouseDrag (mouseEventAt (*lane0, up, down));
    lane0->mouseUp (mouseEventAt (*lane0, up, down));
    TestEnvironment::pumpMessages (100);

    REQUIRE (env.trackService.getClips (*vocals).empty());
    const auto moved = env.trackService.getClips (*guitar);
    REQUIRE (moved.size() == 1);
    REQUIRE (moved[0].id == clipId);
    REQUIRE (moved[0].startSeconds == Catch::Approx (1.0 + 40.0 / view.getTimeMapping().pixelsPerSecond).margin (0.05));
    view.removeFromDesktop();
}

TEST_CASE ("ArrangeView: verschobener MIDI-Clip erscheint an der neuen Stelle", "[ui][clipedit]")
{
    // Regression 2026-10-11: TrackService meldete nur Audio-Clips; ein verschobener MIDI-Clip
    // blieb in der Ansicht an der alten Stelle stehen.
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "MidiMove").has_value());
    auto* track = *env.trackService.createAudioTrack ("Synth", 2);
    const auto clipId = *midiClips.createEmptyClip (*track, 1.0, 2.0);

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
    TestEnvironment::pumpMessages (50);

    auto* lane = dynamic_cast<fk::ui::TrackLane*> (findDescendantWithID (view, "lane0"));
    REQUIRE (lane != nullptr);
    const auto xFor = [&view] (double seconds) { return view.getTimeMapping().secondsToX (seconds); };
    REQUIRE (lane->getClipIdAt (xFor (2.0)) == clipId);

    REQUIRE (env.clipService.moveClip (clipId, 8.0).has_value());
    TestEnvironment::pumpMessages (50);

    REQUIRE (lane->getClipIdAt (xFor (2.0)).isEmpty());
    REQUIRE (lane->getClipIdAt (xFor (9.0)) == clipId);
}

TEST_CASE ("ArrangeView: Clips stumm schalten und per Nudge um einen Rasterschritt schieben", "[ui][clipedit][a4]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };
    fk::services::MarkerService markers { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Nudge").has_value());
    env.timelineService.setTempoBpm (120.0);
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    const auto clipId = *env.clipService.importAudioFile (*track, env.createSilentWav ("eins", 1.0), 2.0);

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
    TestEnvironment::pumpMessages (50);

    const auto clipInfo = [&] { return env.trackService.getClips (*track).front(); };

    view.selectClip (clipId, juce::ModifierKeys());
    view.performClipAction (fk::ui::ClipAction::toggleMute);
    REQUIRE (clipInfo().muted);
    view.performClipAction (fk::ui::ClipAction::toggleMute);
    REQUIRE_FALSE (clipInfo().muted);

    view.setGridResolution (fk::services::GridResolution::beat()); // 0,5 s
    view.nudgeSelectedClips (1);
    REQUIRE (clipInfo().startSeconds == Catch::Approx (2.5));
    view.nudgeSelectedClips (-1);
    view.nudgeSelectedClips (-1);
    REQUIRE (clipInfo().startSeconds == Catch::Approx (1.5));

    env.projectService.undo();
    REQUIRE (clipInfo().startSeconds == Catch::Approx (2.0));
}

TEST_CASE ("ArrangeView: Lasso wählt Clips auf zwei Spuren, Bereichswerkzeug löscht einen Bereich",
           "[ui][clipedit][a4]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };
    fk::services::MarkerService markers { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Lasso").has_value());
    auto* vocals = *env.trackService.createAudioTrack ("Gesang", 1);
    auto* guitar = *env.trackService.createAudioTrack ("Gitarre", 1);
    const auto a = *env.clipService.importAudioFile (*vocals, env.createSilentWav ("a", 1.0), 1.0);
    const auto b = *env.clipService.importAudioFile (*guitar, env.createSilentWav ("b", 1.0), 1.5);
    const auto later = *env.clipService.importAudioFile (*guitar, env.createSilentWav ("c", 1.0), 6.0);

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
    view.setSnapEnabled (false);
    view.setSize (900, 400);
    view.addToDesktop (0);
    view.setTopLeftPosition (0, 0);
    TestEnvironment::pumpMessages (100);

    auto* lane0 = findDescendantWithID (view, "lane0");
    auto* lane1 = findDescendantWithID (view, "lane1");
    REQUIRE (lane0 != nullptr);
    REQUIRE (lane1 != nullptr);
    const auto& mapping = view.getTimeMapping();

    // Lasso auf leerer Fläche bei 0,5 s in Spur 1 starten, bis 2 s in Spur 2: trifft a und b, nicht den späten Clip.
    const auto lassoDown =
        lane0->localPointToGlobal (juce::Point<int> (mapping.secondsToX (0.5), lane0->getHeight() / 2));
    const auto lassoUp =
        lane1->localPointToGlobal (juce::Point<int> (mapping.secondsToX (2.0), lane1->getHeight() / 2));
    lane0->mouseDown (mouseEventAt (*lane0, lassoDown, lassoDown));
    lane0->mouseDrag (mouseEventAt (*lane0, lassoUp, lassoDown));
    lane0->mouseUp (mouseEventAt (*lane0, lassoUp, lassoDown));

    auto selected = view.getSelectedClipIds();
    std::ranges::sort (selected);
    auto expected = std::vector<juce::String> { a, b };
    std::ranges::sort (expected);
    REQUIRE (selected == expected);
    REQUIRE (std::ranges::find (selected, later) == selected.end());

    // Bereich 1,2..1,8 s auf beiden Spuren aufziehen und mit Lücke löschen.
    view.setTool (fk::ui::Tool::range);
    const auto rangeDown =
        lane0->localPointToGlobal (juce::Point<int> (mapping.secondsToX (1.2), lane0->getHeight() / 2));
    const auto rangeUp =
        lane1->localPointToGlobal (juce::Point<int> (mapping.secondsToX (1.8), lane1->getHeight() / 2));
    lane0->mouseDown (mouseEventAt (*lane0, rangeDown, rangeDown));
    lane0->mouseDrag (mouseEventAt (*lane0, rangeUp, rangeDown));
    lane0->mouseUp (mouseEventAt (*lane0, rangeUp, rangeDown));

    const auto& range = view.getRangeSelection();
    REQUIRE (range.has_value());
    REQUIRE (range->tracks.size() == 2);
    REQUIRE (range->fromSeconds == Catch::Approx (1.2).margin (0.05));
    REQUIRE (range->toSeconds == Catch::Approx (1.8).margin (0.05));

    view.performRangeAction (fk::ui::ArrangeView::RangeAction::remove);
    REQUIRE (env.trackService.getClips (*vocals).size() == 2); // 1..1,2 und 1,8..2
    REQUIRE (env.trackService.getClips (*guitar).size() ==
             2); // b beginnt bei 1,5: nur 1,8..2,5 bleibt, dazu der späte Clip

    // Werkzeugwechsel hebt die Bereichsauswahl auf.
    view.setTool (fk::ui::Tool::select);
    REQUIRE_FALSE (view.getRangeSelection().has_value());
    view.removeFromDesktop();
}

TEST_CASE ("ArrangeView: Alt+Ziehen an der rechten Clip-Kante streckt statt zu trimmen", "[ui][clipedit][a5]")
{
    TestEnvironment env;
    fk::services::TransportService transport { env.projectService };
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::MixerService mixer { env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::AutomationService automation { env.projectService };
    fk::services::SignatureService signatureService { env.projectService };
    fk::services::MarkerService markers { env.projectService };

    REQUIRE (env.projectService.createProject (env.tempFolder, "Strecken").has_value());
    auto* track = *env.trackService.createAudioTrack ("Gesang", 1);
    REQUIRE (env.clipService.importAudioFile (*track, env.createSilentWav ("eins", 2.0), 1.0).has_value());

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
    view.setSnapEnabled (false);
    view.setSize (900, 400);
    view.addToDesktop (0);
    view.setTopLeftPosition (0, 0);
    TestEnvironment::pumpMessages (100);

    auto* lane = findDescendantWithID (view, "lane0");
    REQUIRE (lane != nullptr);
    const auto& mapping = view.getTimeMapping();
    const auto edgeX = mapping.secondsToX (3.0) - 2; // rechte Kante des Clips (1..3 s), innerhalb der Trim-Zone
    const auto down = lane->localPointToGlobal (juce::Point<int> (edgeX, lane->getHeight() / 3));
    const auto up =
        down + juce::Point<int> (static_cast<int> (2.0 * mapping.pixelsPerSecond), 0); // zwei Sekunden länger

    lane->mouseDown (mouseEventAt (*lane, down, down, juce::ModifierKeys::altModifier));
    lane->mouseDrag (mouseEventAt (*lane, up, down, juce::ModifierKeys::altModifier));
    lane->mouseUp (mouseEventAt (*lane, up, down, juce::ModifierKeys::altModifier));
    TestEnvironment::pumpMessages (50);

    const auto clip = env.trackService.getClips (*track).front();
    REQUIRE (clip.lengthSeconds == Catch::Approx (4.0).margin (0.1));
    REQUIRE (clip.speedRatio == Catch::Approx (0.5).margin (0.02)); // gestreckt, nicht nur verlängert
    view.removeFromDesktop();
}
