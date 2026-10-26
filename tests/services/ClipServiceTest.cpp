#include "fk/services/MidiClipService.h"
#include "fk/services/PluginService.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestEnvironment.h"

using fk::tests::TestEnvironment;

namespace
{

struct ProjectWithClip
{
    explicit ProjectWithClip (TestEnvironment& environment)
        : env (environment)
    {
        REQUIRE (env.projectService.createProject (env.tempFolder, "Clips").has_value());
        track = *env.trackService.createAudioTrack ("Gitarre");

        const auto wav = env.createSilentWav ("vier_sekunden", 4.0);
        clipId = *env.clipService.importAudioFile (*track, wav, 2.0);
    }

    TestEnvironment& env;
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};

} // namespace

TEST_CASE ("ClipService: importAudioFile legt einen Clip mit Dateilänge an", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);

    const auto clips = env.trackService.getClips (*project.track);
    REQUIRE (clips.size() == 1);
    REQUIRE (clips[0].id == project.clipId);
    REQUIRE (clips[0].startSeconds == Catch::Approx (2.0));
    REQUIRE (clips[0].lengthSeconds == Catch::Approx (4.0));
}

TEST_CASE ("ClipService: moveClip verschiebt, nicht vor Null", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);

    REQUIRE (env.clipService.moveClip (project.clipId, 5.5).has_value());
    REQUIRE (env.trackService.getClips (*project.track)[0].startSeconds == Catch::Approx (5.5));

    REQUIRE (env.clipService.moveClip (project.clipId, -3.0).has_value());
    REQUIRE (env.trackService.getClips (*project.track)[0].startSeconds == Catch::Approx (0.0));
}

TEST_CASE ("ClipService: splitClip erzeugt zwei nahtlose Teile", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);

    REQUIRE (env.clipService.splitClip (project.clipId, 3.0).has_value());

    const auto clips = env.trackService.getClips (*project.track);
    REQUIRE (clips.size() == 2);
    REQUIRE (clips[0].startSeconds == Catch::Approx (2.0));
    REQUIRE (clips[0].lengthSeconds == Catch::Approx (1.0));
    REQUIRE (clips[1].startSeconds == Catch::Approx (3.0));
    REQUIRE (clips[1].lengthSeconds == Catch::Approx (3.0));
    REQUIRE (clips[1].offsetSeconds == Catch::Approx (1.0));
}

TEST_CASE ("ClipService: splitClip außerhalb des Clips ändert nichts", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);

    REQUIRE (env.clipService.splitClip (project.clipId, 10.0).has_value());
    REQUIRE (env.trackService.getClips (*project.track).size() == 1);
}

TEST_CASE ("ClipService: deleteClip entfernt den Clip, Undo holt ihn zurück", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);

    REQUIRE (env.clipService.deleteClip (project.clipId).has_value());
    REQUIRE (env.trackService.getClips (*project.track).empty());

    env.projectService.undo();
    REQUIRE (env.trackService.getClips (*project.track).size() == 1);

    const auto missing = env.clipService.deleteClip ("gibt-es-nicht");
    REQUIRE_FALSE (missing.has_value());
    REQUIRE (missing.error().code == fk::core::ErrorCode::notFound);
}

TEST_CASE ("ClipService: Fades und Clip-Gain nur bei Audio-Clips, begrenzt auf die Cliplänge", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env); // 4 Sekunden Audio

    env.clipService.beginChange ("Fade ziehen");
    REQUIRE (env.clipService.setFades (project.clipId, 0.5, 1.0).has_value());
    auto info = env.trackService.getClips (*project.track).front();
    REQUIRE (info.fadeInSeconds == Catch::Approx (0.5));
    REQUIRE (info.fadeOutSeconds == Catch::Approx (1.0));

    // Länger als der Clip: Fade-in auf die Länge, Fade-out auf den Rest begrenzt.
    REQUIRE (env.clipService.setFades (project.clipId, 3.0, 3.0).has_value());
    info = env.trackService.getClips (*project.track).front();
    REQUIRE (info.fadeInSeconds == Catch::Approx (3.0));
    REQUIRE (info.fadeOutSeconds == Catch::Approx (1.0));

    env.clipService.beginChange ("Clip-Gain");
    REQUIRE (env.clipService.setGainDb (project.clipId, -6.0f).has_value());
    REQUIRE (env.trackService.getClips (*project.track).front().gainDb == Catch::Approx (-6.0f));
    REQUIRE (env.clipService.setGainDb (project.clipId, 90.0f).has_value());
    REQUIRE (env.trackService.getClips (*project.track).front().gainDb == Catch::Approx (24.0f));

    env.projectService.undo();
    REQUIRE (env.trackService.getClips (*project.track).front().gainDb == Catch::Approx (0.0f));

    REQUIRE_FALSE (env.clipService.setFades ("gibt-es-nicht", 0.1, 0.1).has_value());
}

TEST_CASE ("ClipService: Takes wählen, für Bereich übernehmen, auspacken und aufräumen", "[services][clips][takes]")
{
    TestEnvironment env;
    ProjectWithClip project (env); // Clip 2..6 s
    const auto secondTake = env.createSilentWav ("take_zwei", 4.0);

    REQUIRE (env.clipService.getTakes (project.clipId).empty());
    REQUIRE (env.clipService.addTakeFromFile (project.clipId, secondTake).has_value());

    // Dateien von außerhalb werden in den Projektordner kopiert (Audio/), wie beim Import:
    // Takes und Aufnahmen kennt Tracktion nur innerhalb des Projekts.
    REQUIRE (env.trackService.getClips (*project.track)
                 .front()
                 .audioFile.isAChildOf (env.projectService.getProjectFolder()));

    auto takes = env.clipService.getTakes (project.clipId);
    REQUIRE (takes.size() == 2);
    REQUIRE (takes[0].isCurrent);
    REQUIRE (env.trackService.getClips (*project.track).front().numTakes == 2);

    REQUIRE (env.clipService.setCurrentTake (project.clipId, 1).has_value());
    REQUIRE (env.clipService.getTakes (project.clipId)[1].isCurrent);
    REQUIRE_FALSE (env.clipService.setCurrentTake (project.clipId, 5).has_value());

    // Bereich 3..4 s auf Take 0: drei Stücke, das mittlere spielt Take 0, die anderen Take 1.
    const auto middle = env.clipService.useTakeForRange (project.clipId, 0, 3.0, 4.0);
    REQUIRE (middle.has_value());
    auto clips = env.trackService.getClips (*project.track);
    REQUIRE (clips.size() == 3);
    std::sort (
        clips.begin(), clips.end(), [] (const auto& a, const auto& b) { return a.startSeconds < b.startSeconds; });
    REQUIRE (clips[0].currentTake == 1);
    REQUIRE (clips[1].id == *middle);
    REQUIRE (clips[1].currentTake == 0);
    // Das Mittelstück ragt je 10 ms in die Nachbarn: Platz für die Crossfades an den Schnitten.
    REQUIRE (clips[1].startSeconds == Catch::Approx (2.99));
    REQUIRE (clips[1].lengthSeconds == Catch::Approx (1.02));
    REQUIRE (clips[0].startSeconds + clips[0].lengthSeconds == Catch::Approx (3.0));
    REQUIRE (clips[2].startSeconds == Catch::Approx (4.0));
    REQUIRE (clips[2].currentTake == 1);
    REQUIRE (clips[1].numTakes == 2); // alle Stücke behalten alle Takes

    env.projectService.undo();
    REQUIRE (env.trackService.getClips (*project.track).size() == 1);

    REQUIRE (env.clipService.setShowingTakes (project.clipId, true).has_value());
    REQUIRE (env.trackService.getClips (*project.track).front().showingTakes);

    // Verteilen: je Take eine Spur unter der Spur, der Clip selbst bleibt.
    REQUIRE (env.trackService.getAudioTracks().size() == 1);
    REQUIRE (env.clipService.unpackTakes (project.clipId).has_value());
    REQUIRE (env.trackService.getAudioTracks().size() == 3);
    REQUIRE (env.trackService.getClips (*env.trackService.getAudioTracks()[1]).size() == 1);
    REQUIRE (env.trackService.getClips (*env.trackService.getAudioTracks()[2]).size() == 1);

    env.projectService.undo();
    REQUIRE (env.trackService.getAudioTracks().size() == 1);

    // Aufräumen lässt nur den aktuellen Take übrig; Tracktion löst die Take-Liste dann ganz auf.
    REQUIRE (env.clipService.deleteUnusedTakes (project.clipId).has_value());
    REQUIRE (env.clipService.getTakes (project.clipId).size() <= 1);
    REQUIRE (env.trackService.getClips (*project.track).front().numTakes <= 1);
}

TEST_CASE ("ClipService: trimClip verschiebt links den Quellversatz, rechts nur die Länge", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env); // 2..6 s

    REQUIRE (env.clipService.trimClip (project.clipId, 3.0, 5.0).has_value());
    auto info = env.trackService.getClips (*project.track)[0];
    REQUIRE (info.startSeconds == Catch::Approx (3.0));
    REQUIRE (info.lengthSeconds == Catch::Approx (2.0));
    REQUIRE (info.offsetSeconds == Catch::Approx (1.0)); // Material bleibt an seiner Stelle

    // Links nicht vor den Anfang des Materials (Versatz 0 = Projektzeit 2 s).
    REQUIRE (env.clipService.trimClip (project.clipId, 0.0, 5.0).has_value());
    info = env.trackService.getClips (*project.track)[0];
    REQUIRE (info.startSeconds == Catch::Approx (2.0));
    REQUIRE (info.offsetSeconds == Catch::Approx (0.0));

    // Ende nie vor dem Anfang.
    REQUIRE (env.clipService.trimClip (project.clipId, 2.0, 1.0).has_value());
    info = env.trackService.getClips (*project.track)[0];
    REQUIRE (info.lengthSeconds > 0.0);
    REQUIRE_FALSE (env.clipService.trimClip ("999", 0.0, 1.0).has_value());
}

TEST_CASE ("ClipService: moveClips verschiebt gemeinsam, auch auf eine andere Spur passender Art", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env);
    auto* second = *env.trackService.createAudioTrack ("Bass");
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    auto* synth = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto midiId = *midiClips.createEmptyClip (*synth, 2.0, 2.0);

    // Zwei Audio-Clips zusammen eine Spur nach unten und 1 s nach rechts.
    const auto secondClip = *env.clipService.importAudioFile (*project.track, env.createSilentWav ("b", 1.0), 8.0);
    REQUIRE (env.clipService.moveClips ({ project.clipId, secondClip }, 1.0, 1).has_value());
    REQUIRE (env.trackService.getClips (*project.track).empty());
    const auto moved = env.trackService.getClips (*second);
    REQUIRE (moved.size() == 2);
    REQUIRE (moved[0].startSeconds == Catch::Approx (3.0));
    REQUIRE (moved[1].startSeconds == Catch::Approx (9.0));

    // Audio-Clip auf die Instrumentenspur: abgelehnt, nichts verändert.
    REQUIRE_FALSE (env.clipService.moveClips ({ project.clipId }, 0.0, 1).has_value());
    REQUIRE (env.trackService.getClips (*second).size() == 2);

    // MIDI-Clip auf eine Audiospur: abgelehnt; nach oben über die erste Spur hinaus: abgelehnt.
    REQUIRE_FALSE (env.clipService.moveClips ({ midiId }, 0.0, -1).has_value());
    REQUIRE_FALSE (env.clipService.moveClips ({ project.clipId }, 0.0, -5).has_value());

    // Block nie vor Null: Beide um 10 s nach links rückt nur bis der erste bei 0 steht.
    REQUIRE (env.clipService.moveClips ({ project.clipId, secondClip }, -10.0, 0).has_value());
    const auto clamped = env.trackService.getClips (*second);
    REQUIRE (clamped[0].startSeconds == Catch::Approx (0.0));
    REQUIRE (clamped[1].startSeconds == Catch::Approx (6.0));
}

TEST_CASE ("ClipService: kopieren, ausschneiden, einfügen und duplizieren behalten Abstände", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env); // 2..6 s
    auto* second = *env.trackService.createAudioTrack ("Bass");
    const auto onSecond = *env.clipService.importAudioFile (*second, env.createSilentWav ("b", 1.0), 3.0);

    REQUIRE_FALSE (env.clipService.hasClipboardContent());
    REQUIRE_FALSE (env.clipService.pasteClips (*project.track, 0.0).has_value());
    REQUIRE (env.clipService.copyClips ({ project.clipId, onSecond }).has_value());
    REQUIRE (env.clipService.hasClipboardContent());

    // Einfügen bei 10 s auf der ersten Spur: Block mit Zeit- und Spurabstand.
    const auto pasted = env.clipService.pasteClips (*project.track, 10.0);
    REQUIRE (pasted.has_value());
    REQUIRE (pasted->size() == 2);
    const auto firstTrackClips = env.trackService.getClips (*project.track);
    const auto secondTrackClips = env.trackService.getClips (*second);
    REQUIRE (firstTrackClips.size() == 2);
    REQUIRE (firstTrackClips[1].startSeconds == Catch::Approx (10.0));
    REQUIRE (firstTrackClips[1].lengthSeconds == Catch::Approx (4.0));
    REQUIRE (firstTrackClips[1].id != project.clipId);
    REQUIRE (secondTrackClips.size() == 2);
    REQUIRE (secondTrackClips[1].startSeconds == Catch::Approx (11.0));

    // Einfügen auf der letzten Spur: Für die zweite Zeile fehlt die Spur, nichts passiert.
    REQUIRE_FALSE (env.clipService.pasteClips (*second, 20.0).has_value());
    REQUIRE (env.trackService.getClips (*second).size() == 2);

    // Ausschneiden entfernt, Einfügen holt zurück; Undo macht das Ausschneiden rückgängig.
    REQUIRE (env.clipService.cutClips ({ onSecond }).has_value());
    REQUIRE (env.trackService.getClips (*second).size() == 1);
    REQUIRE (env.clipService.pasteClips (*second, 0.0).has_value());
    REQUIRE (env.trackService.getClips (*second).size() == 2);
    const auto restored = env.trackService.getClips (*second); // Reihenfolge = Anlage, nicht Zeit
    REQUIRE (std::any_of (restored.begin(),
                          restored.end(),
                          [] (const fk::services::ClipInfo& clip) { return std::abs (clip.startSeconds) < 0.01; }));

    // Duplizieren: Kopie direkt hinter dem Ende der Auswahl (Clip 2..6 → Kopie bei 6).
    const auto duplicated = env.clipService.duplicateClips ({ project.clipId });
    REQUIRE (duplicated.has_value());
    auto afterDuplicate = env.trackService.getClips (*project.track);
    std::sort (afterDuplicate.begin(),
               afterDuplicate.end(),
               [] (const fk::services::ClipInfo& a, const fk::services::ClipInfo& b)
               { return a.startSeconds < b.startSeconds; });
    REQUIRE (afterDuplicate.size() == 3);
    REQUIRE (afterDuplicate[1].startSeconds == Catch::Approx (6.0));
    REQUIRE (afterDuplicate[1].lengthSeconds == Catch::Approx (4.0));

    env.projectService.undo();
    REQUIRE (env.trackService.getClips (*project.track).size() == 2);
}

TEST_CASE ("ClipService: mergeClips verbindet MIDI-Noten und rendert Audio in eine Datei", "[services][clips]")
{
    TestEnvironment env;
    ProjectWithClip project (env); // 2..6 s
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    auto* synth = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));

    // MIDI: zwei Clips mit je einer Note, geklebt = ein Clip mit zwei Noten über beide Takte.
    const auto midiA = *midiClips.createEmptyClip (*synth, 0.0, 2.0);
    const auto midiB = *midiClips.createEmptyClip (*synth, 2.0, 2.0);
    REQUIRE (midiClips.addNote (midiA, 60, 0.0, 1.0, 100).has_value());
    REQUIRE (midiClips.addNote (midiB, 64, 1.0, 1.0, 100).has_value());
    const auto mergedMidi = env.clipService.mergeClips ({ midiA, midiB });
    REQUIRE (mergedMidi.has_value());
    const auto synthClips = env.trackService.getClips (*synth);
    REQUIRE (synthClips.size() == 1);
    REQUIRE (synthClips[0].id == *mergedMidi);
    REQUIRE (synthClips[0].lengthSeconds == Catch::Approx (4.0));
    const auto notes = midiClips.getClip (*mergedMidi)->notes;
    REQUIRE (notes.size() == 2);
    REQUIRE (notes[1].startBeats == Catch::Approx (5.0)); // 4 Schläge + 1

    // Audio: zweiter Clip 7..8 s, geklebt = ein Clip 2..8 s aus einer neuen Datei im Projekt.
    const auto audioB = *env.clipService.importAudioFile (*project.track, env.createSilentWav ("b", 1.0), 7.0);
    const auto mergedAudio = env.clipService.mergeClips ({ project.clipId, audioB });
    REQUIRE (mergedAudio.has_value());
    const auto audioClips = env.trackService.getClips (*project.track);
    REQUIRE (audioClips.size() == 1);
    REQUIRE (audioClips[0].startSeconds == Catch::Approx (2.0));
    REQUIRE (audioClips[0].lengthSeconds == Catch::Approx (6.0).margin (0.01));
    REQUIRE (audioClips[0].audioFile.existsAsFile());
    REQUIRE (audioClips[0].audioFile.isAChildOf (env.projectService.getProjectFolder().getChildFile ("Audio")));

    // Gemischt oder auf verschiedenen Spuren: abgelehnt. Nur ein Clip: abgelehnt.
    REQUIRE_FALSE (env.clipService.mergeClips ({ audioClips[0].id, *mergedMidi }).has_value());
    REQUIRE_FALSE (env.clipService.mergeClips ({ audioClips[0].id }).has_value());

    // deleteClips entfernt mehrere in einem Undo-Schritt.
    REQUIRE (env.clipService.deleteClips ({ audioClips[0].id, *mergedMidi }).has_value());
    REQUIRE (env.trackService.getClips (*project.track).empty());
    REQUIRE (env.trackService.getClips (*synth).empty());
    env.projectService.undo();
    REQUIRE (env.trackService.getClips (*project.track).size() == 1);
    REQUIRE (env.trackService.getClips (*synth).size() == 1);
}

TEST_CASE ("ClipService: importMidiFile legt Clips mit den Noten der Datei an", "[services][clips][midi]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService pluginService { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    REQUIRE (env.projectService.createProject (env.tempFolder, "MidiImport").has_value());
    auto* synth = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (pluginService));
    const auto file = env.createMidiFile ("zwei_spuren");

    REQUIRE (fk::services::ClipService::countMidiTracks (file) == 2);
    REQUIRE (fk::services::ClipService::countMidiTracks (env.createSilentWav ("kein_midi", 1.0)) == 0);

    // Alle Spuren zusammen bei 4 s (120 BPM): drei Noten, Länge bis zum Ende der letzten (3 Schläge).
    const auto merged = env.clipService.importMidiFile (*synth, file, 4.0);
    REQUIRE (merged.has_value());
    const auto all = *midiClips.getClip (*merged);
    REQUIRE (all.notes.size() == 3);
    REQUIRE (all.startSeconds == Catch::Approx (4.0));
    REQUIRE (all.lengthBeats == Catch::Approx (3.0));

    // Nur die zweite Spur: eine Note bei 2 Schlägen.
    const auto second = env.clipService.importMidiFile (*synth, file, 0.0, 1);
    REQUIRE (second.has_value());
    const auto secondClip = *midiClips.getClip (*second);
    REQUIRE (secondClip.notes.size() == 1);
    REQUIRE (secondClip.notes[0].noteNumber == 55);
    REQUIRE (secondClip.notes[0].startBeats == Catch::Approx (2.0));

    REQUIRE_FALSE (env.clipService.importMidiFile (*synth, file, 0.0, 5).has_value());
    REQUIRE_FALSE (env.clipService.importMidiFile (*synth, env.createSilentWav ("x", 1.0), 0.0).has_value());
}
