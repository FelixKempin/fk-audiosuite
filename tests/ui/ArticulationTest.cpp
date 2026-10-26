#include "fk/engine/ExpressionMap.h"
#include "fk/services/ExportService.h"
#include "fk/services/ExpressionMapService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/MidiControllerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/KeyEditor.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"

#include <algorithm>

using fk::tests::TestEnvironment;

namespace
{
struct NoteOn
{
    double tick;
    int channel;
    int note;
};

std::vector<NoteOn> noteOnsOf (const juce::File& file)
{
    juce::MidiFile midi;
    juce::FileInputStream in (file);
    REQUIRE (midi.readFrom (in));
    std::vector<NoteOn> result;

    for (int track = 0; track < midi.getNumTracks(); ++track)
        for (const auto* event : *midi.getTrack (track))
            if (event->message.isNoteOn())
                result.push_back ({ event->message.getTimeStamp(), event->message.getChannel(), event->message.getNoteNumber() });

    std::ranges::stable_sort (result, {}, &NoteOn::tick);
    return result;
}
} // namespace

TEST_CASE ("B4: Expression Map als JSON", "[engine][articulation][b4]")
{
    fk::engine::ExpressionMap map;
    map.id = "test";
    map.name = "Test";
    map.articulations.push_back ({ "stacc", "Staccato", ".", { .channel = 3 } });
    map.articulations.push_back ({ "pizz", "Pizzicato", "pizz", { .keyswitch = 29, .cc = 32, .ccValue = 5 } });

    const auto parsed = fk::engine::ExpressionMap::fromJson (map.toJson());
    REQUIRE (parsed.has_value());
    REQUIRE (parsed->articulations.size() == 2);
    REQUIRE (parsed->find ("stacc")->output.channel == 3);
    REQUIRE (parsed->find ("pizz")->output.keyswitch == 29);
    REQUIRE (parsed->find ("pizz")->output.cc == 32);
    REQUIRE (parsed->find ("pizz")->output.ccValue == 5);
    REQUIRE (parsed->find ("fehlt") == nullptr);
    REQUIRE_FALSE (fk::engine::ExpressionMap::fromJson ("{\"articulations\":[{\"name\":\"ohne id\"}]}").has_value());
}

TEST_CASE ("B4: Spielweisen kommen als Kanal und Keyswitch beim Instrument an", "[services][articulation][b4]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::ExportService exporter { env.projectService };
    fk::services::ExpressionMapService maps { env.projectService, env.trackService, env.tempFolder.getChildFile ("Maps") };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Spielweisen").has_value());

    REQUIRE (maps.getMaps().size() == 2);
    auto* strings = *env.trackService.createInstrumentTrack ("Violinen", TestEnvironment::synthId (plugins));
    REQUIRE (maps.setMapForTrack (*strings, fk::services::ExpressionMapService::orchestraStringsId).has_value());
    REQUIRE (maps.getMapForTrack (*strings)->find ("pizz")->output.channel == 13);

    const auto clipId = *midiClips.createEmptyClip (*strings, 0.0, 2.0);
    REQUIRE (midiClips.addNote (clipId, 60, 0.0, 1.0, 100).has_value());
    REQUIRE (midiClips.addNote (clipId, 62, 1.0, 1.0, 100).has_value());
    REQUIRE (midiClips.addNote (clipId, 64, 2.0, 1.0, 100).has_value());

    auto clip = *midiClips.getClip (clipId);
    const auto indexOf = [&clip] (int note)
    { return std::ranges::find (clip.notes, note, &fk::services::EditableNote::noteNumber)->index; };

    REQUIRE (midiClips.setArticulation (clipId, { indexOf (60) }, "stacc").has_value());
    REQUIRE (midiClips.setArticulation (clipId, { indexOf (62) }, "pizz").has_value());
    REQUIRE (env.projectService.getUndoHistory().done.back() == "Articulation");
    clip = *midiClips.getClip (clipId);
    REQUIRE (std::ranges::find (clip.notes, 62, &fk::services::EditableNote::noteNumber)->articulation == "pizz");

    // Kanäle: Staccato 3, Pizzicato 13, ohne Spielweise der Kanal des Clips (1).
    auto ons = noteOnsOf (*exporter.exportMidi (env.tempFolder.getChildFile ("kanaele.mid")));
    REQUIRE (ons.size() == 3);
    REQUIRE (ons[0].channel == 3);
    REQUIRE (ons[1].channel == 13);
    REQUIRE (ons[2].channel == 1);

    // Keyswitches: vor jeder Note mit Spielweise ihre Keyswitch-Note (Staccato D0 = 26, Pizzicato F0 = 29).
    REQUIRE (maps.setMapForTrack (*strings, fk::services::ExpressionMapService::keyswitchesId).has_value());
    ons = noteOnsOf (*exporter.exportMidi (env.tempFolder.getChildFile ("keyswitch.mid")));
    REQUIRE (ons.size() == 5); // drei Noten, zwei Keyswitches
    REQUIRE (ons[0].note == 26);  // Staccato-Keyswitch ...
    REQUIRE (ons[1].note == 60);  // ... vor der Note
    REQUIRE (ons[2].note == 29);  // Pizzicato-Keyswitch
    REQUIRE (ons[3].note == 62);
    REQUIRE (ons[4].note == 64);  // ohne Spielweise: kein Keyswitch
    REQUIRE (ons[0].tick <= ons[1].tick);

    // Spielweise entfernen, Map entfernen: wieder die einfache Folge.
    REQUIRE (midiClips.setArticulation (clipId, { indexOf (60), indexOf (62) }, "").has_value());
    ons = noteOnsOf (*exporter.exportMidi (env.tempFolder.getChildFile ("ohne.mid")));
    REQUIRE (ons.size() == 3);
    REQUIRE (maps.setMapForTrack (*strings, "").has_value());
    REQUIRE_FALSE (maps.getMapForTrack (*strings).has_value());

    // Speichern und öffnen: Map an der Spur und Spielweise an der Note bleiben.
    REQUIRE (maps.setMapForTrack (*strings, fk::services::ExpressionMapService::orchestraStringsId).has_value());
    REQUIRE (midiClips.setArticulation (clipId, { indexOf (64) }, "trem").has_value());
    REQUIRE (env.projectService.saveProject().has_value());
    const auto file = env.tempFolder.getChildFile ("Spielweisen/Spielweisen.tracktionedit");
    env.projectService.closeProject();
    REQUIRE (env.projectService.openProject (file).has_value());
    auto* reopened = env.trackService.getAudioTracks().front();
    REQUIRE (maps.getMapForTrack (*reopened)->id == fk::services::ExpressionMapService::orchestraStringsId);
    const auto reopenedClip = *midiClips.getClip (env.trackService.getClips (*reopened).front().id);
    REQUIRE (std::ranges::find (reopenedClip.notes, 64, &fk::services::EditableNote::noteNumber)->articulation == "trem");

    // Eigene Spielweisen-Liste importieren.
    const auto own = env.tempFolder.getChildFile ("eigen.json");
    REQUIRE (maps.exportMap (fk::services::ExpressionMapService::keyswitchesId, own).has_value());
    REQUIRE (maps.importMap (own).has_value());
    REQUIRE (maps.getMaps().size() == 3);
}

TEST_CASE ("B4: Artikulations-Lane im Key Editor setzt Spielweisen per Klick", "[ui][articulation][b4]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::MidiControllerService controllers { env.projectService };
    fk::services::NotePreviewService preview { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::SignatureService signature { env.projectService };
    fk::services::ExpressionMapService maps { env.projectService, env.trackService, env.tempFolder.getChildFile ("Maps") };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Lane").has_value());

    auto* track = *env.trackService.createInstrumentTrack ("Celli", TestEnvironment::synthId (plugins));
    REQUIRE (maps.setMapForTrack (*track, fk::services::ExpressionMapService::orchestraStringsId).has_value());
    const auto clipId = *midiClips.createEmptyClip (*track, 0.0, 2.0);
    REQUIRE (midiClips.addNote (clipId, 48, 0.0, 1.0, 100).has_value());

    fk::ui::KeyEditor editor ({ midiClips, controllers, preview, transport, env.timelineService, signature, nullptr, &maps });
    editor.setSize (1000, 700);
    editor.setClip (clipId);

    auto& lane = editor.getArticulationLane();
    REQUIRE (lane.hasMap());
    REQUIRE (lane.isVisible());

    // Klick in Zeile 3 (Staccato) über der Note.
    const auto row = 2;
    const auto at = juce::Point<float> (static_cast<float> (56 + 5), static_cast<float> (row * fk::ui::ArticulationLane::rowHeight + 5));
    const juce::MouseEvent click { juce::Desktop::getInstance().getMainMouseSource(), at,
                                   juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   &lane, &lane, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false };
    lane.mouseDown (click);
    TestEnvironment::pumpMessages (50);
    REQUIRE (midiClips.getClip (clipId)->notes.front().articulation == "stacc");

    // Nochmal: weg.
    lane.mouseDown (click);
    TestEnvironment::pumpMessages (50);
    REQUIRE (midiClips.getClip (clipId)->notes.front().articulation.isEmpty());
}
