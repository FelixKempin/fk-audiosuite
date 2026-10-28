#include "fk/services/DrumMapService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/DrumEditor.h"
#include "fk/ui/FkLookAndFeel.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"
#include "ManualShot.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <vector>

// Bilder für docs/manual/drum-editor.md. Ändert sich der Drum-Editor, hier die Szene anpassen und
// tools/manual.py shots laufen lassen (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::saveShot;

namespace
{
/// Erstes sichtbares Label (beliebig tief), dessen Text so beginnt. Labels haben keinen Tooltip.
juce::Label& findDrumLabel (juce::Component& root, const juce::String& textStart)
{
    std::function<juce::Label*(juce::Component&)> search = [&] (juce::Component& parent) -> juce::Label*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->isVisible() && label->getText().startsWith (textStart))
                    return label;

            if (auto* found = search (*child))
                return found;
        }

        return nullptr;
    };

    auto* found = search (root);
    INFO ("Kein sichtbares Label mit Text \"" << textStart << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Projekt mit Standardtempo (120 BPM: 4 s = zwei Takte). createDrumTrack legt eine Synth-Spur mit
/// GM-Map an, createSamplerKit ein leeres Sampler-Kit; addBeat schreibt zwei Takte Rock-Beat mit Tom-Fill.
struct DrumEditorScene
{
    DrumEditorScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Manual").has_value());
    }

    ~DrumEditorScene() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    void createDrumTrack()
    {
        auto created = env.trackService.createInstrumentTrack ("Drums", TestEnvironment::synthId (plugins));
        REQUIRE (created.has_value());
        track = *created;
        REQUIRE (drumMaps.setMapForTrack (*track, fk::services::DrumMapService::gmMapId).has_value());
        createClip();
    }

    void createSamplerKit()
    {
        auto created = sampler.createSamplerTrack();
        REQUIRE (created.has_value());
        track = *created;
        createClip();
    }

    void createClip()
    {
        auto created = midiClips.createEmptyClip (*track, 0.0, 4.0);
        REQUIRE (created.has_value());
        clipId = *created;
    }

    void addBeat()
    {
        std::vector<fk::services::EditableNote> beat;
        const auto hit = [&beat] (int noteNumber, double start, int velocity)
        { beat.push_back ({ -1, noteNumber, start, 0.25, velocity, {} }); };

        hit (49, 0.0, 120); // Crash 1 auf der Eins

        for (const double start : { 0.0, 1.5, 2.5, 4.0, 5.5 })
            hit (36, start, 110); // Bassdrum

        for (const double start : { 1.0, 3.0, 5.0 })
            hit (38, start, 110); // Snare

        hit (38, 3.75, 40); // leise Ghost-Note

        // Hi-Hat in Achteln, betont und unbetont; auf 3.5 offen, ab Takt 2 Schlag 4 der Fill.
        for (int step = 1; step < 14; ++step)
            if (step != 7)
                hit (42, step * 0.5, step % 2 == 0 ? 90 : 60);

        hit (46, 3.5, 90);

        const std::array<int, 4> toms { 50, 48, 45, 43 };

        for (std::size_t i = 0; i < toms.size(); ++i)
            hit (toms[i], 7.0 + 0.25 * static_cast<double> (i), 100);

        REQUIRE (midiClips.addNotes (clipId, beat, "Beat").has_value());
    }

    fk::ui::DrumEditor::Services services()
    {
        return { midiClips, preview, transport, env.timelineService, drumMaps, env.trackService, sampler };
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::MidiClipService midiClips { env.projectService };
    fk::services::NotePreviewService preview { env.projectService };
    fk::services::TransportService transport { env.projectService };
    fk::services::DrumMapService drumMaps { env.projectService, env.trackService, env.tempFolder.getChildFile ("DrumMaps") };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    fk::services::TrackHandle* track = nullptr;
    juce::String clipId;
};

/// Markierung für eine Zeile der Namensspalte (Koordinaten des Editors).
Callout rowCallout (int number, fk::ui::DrumEditor& editor, int row)
{
    auto& names = findByTooltip (editor, "Click plays the instrument");
    const auto rowArea = juce::Rectangle<int> (0, row * fk::ui::DrumGrid::rowHeight, fk::ui::DrumNames::width, fk::ui::DrumGrid::rowHeight);
    return { number, editor.getLocalArea (&names, rowArea), Callout::Placement::inside };
}
} // namespace

// FIX F-909
TEST_CASE ("Handbuch: Drum-Editor ohne Clip", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    DrumEditorScene scene;
    fk::ui::DrumEditor editor (scene.services());
    editor.setSize (900, 220);
    TestEnvironment::pumpMessages (100);

    saveShot (editor, "drum-editor/empty");
}

// FIX F-001
// FIX F-910
TEST_CASE ("Handbuch: Drum-Editor Überblick mit nummerierten Elementen", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    DrumEditorScene scene;
    scene.createDrumTrack();
    scene.addBeat();
    fk::ui::DrumEditor editor (scene.services());
    editor.setSize (900, 430);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    // Nummern wie in der Tabelle in drum-editor.md, Abschnitt "The editor at a glance". Die Leiste ist
    // rund 750 px breit und passt in ein Bild.
    const std::array<const char*, 6> toolbar { "Drum map of the track", "Show only instruments", "Grid:",
                                               "Velocity of new hits",  "Quantize:",             "Audition:" };
    std::vector<Callout> callouts { calloutFor (1, editor, findDrumLabel (editor, "MIDI"), Callout::Placement::above) };

    for (std::size_t i = 0; i < toolbar.size(); ++i)
        callouts.push_back (calloutFor (static_cast<int> (i) + 2, editor, findByTooltip (editor, toolbar[i]), Callout::Placement::above));

    callouts.push_back (calloutFor (8, editor, findByTooltip (editor, "Click plays the instrument"), Callout::Placement::inside));
    callouts.push_back (calloutFor (9, editor, findByTooltip (editor, "Click: add or delete a hit"), Callout::Placement::inside));

    saveShot (editor, "drum-editor/overview", callouts);
}

// FIX F-001
TEST_CASE ("Handbuch: Drum-Editor nur benutzte Instrumente", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    DrumEditorScene scene;
    scene.createDrumTrack();
    scene.addBeat();
    REQUIRE (scene.midiClips.addNote (scene.clipId, 82, 2.0, 0.25, 90).has_value()); // nicht in der GM-Map

    fk::ui::DrumEditor editor (scene.services());
    editor.setSize (900, 300);
    editor.setClip (scene.clipId);
    editor.setShowUsedOnly (true);
    TestEnvironment::pumpMessages (100);

    const auto& rows = editor.getRows();
    const auto foreign = std::ranges::find (rows, false, &fk::ui::DrumRow::inMap);
    REQUIRE (foreign != rows.end());
    const auto foreignRow = static_cast<int> (std::distance (rows.begin(), foreign));

    saveShot (editor,
              "drum-editor/used-only",
              { calloutFor (1, editor, findByTooltip (editor, "Show only instruments"), Callout::Placement::above),
                rowCallout (2, editor, foreignRow) });
}

// FIX F-001
TEST_CASE ("Handbuch: Drum-Editor Sampler-Kit mit eigenen Samples", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    DrumEditorScene scene;
    scene.createSamplerKit();

    // Drei Samples auf Bassdrum, Snare und geschlossene Hi-Hat; Toms und Becken bleiben leer.
    struct Pad
    {
        int note;
        const char* file;
    };

    const std::array<Pad, 3> pads { { { 36, "Kick 01" }, { 38, "Snare tight" }, { 42, "Hat closed" } } };

    for (const auto& pad : pads)
        REQUIRE (scene.sampler.assignSample (*scene.track, scene.env.createSilentWav (pad.file, 0.2), pad.note).has_value());

    scene.addBeat();

    fk::ui::DrumEditor editor (scene.services());
    editor.setSize (900, 350);
    editor.setClip (scene.clipId);
    TestEnvironment::pumpMessages (100);

    const int kickRow = editor.getGrid().rowForNote (36);
    REQUIRE (kickRow >= 0);
    REQUIRE (editor.getRows()[static_cast<std::size_t> (kickRow)].sampleName == "Kick 01");

    saveShot (editor, "drum-editor/sampler-kit", { rowCallout (1, editor, kickRow) });
}
