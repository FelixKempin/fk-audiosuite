#pragma once

#include "fk/services/AutomationService.h"
#include "fk/services/AutosaveService.h"
#include "fk/services/BakeService.h"
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
#include "fk/services/MidiLearnService.h"
#include "fk/services/MidiService.h"
#include "fk/services/MixerService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/PluginService.h"
#include "fk/services/PreviewService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/RackService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/SendService.h"
#include "fk/services/SignatureService.h"
#include "fk/services/TemplateService.h"
#include "fk/services/TransportService.h"
#include "fk/services/UpdateService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/ProjectComponent.h"

#include <catch2/catch_test_macros.hpp>

#include "../services/TestEnvironment.h"
#include "ManualShot.h"

#include <cmath>
#include <memory>
#include <vector>

/// Szenen mit dem ganzen Projektfenster für das Benutzerhandbuch (docs/manual/MAINTAINING.md).
/// Kapitel wie Projektfenster, Transport, Spuren und MixConsole bauen hiermit dasselbe Demo-Projekt
/// und fotografieren das echte fk::ui::ProjectComponent oder Ausschnitte davon.
namespace fk::tests::manual
{

/// Alle Services, die das Projektfenster braucht (wie FullServices in ProjectComponentLayoutTest.cpp).
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

/// Erstes direktes Kind vom Typ T (z. B. ArrangeView im Projektfenster). Fehlt es, schlägt die Szene fehl.
template <typename ComponentType>
ComponentType& findDirectChild (juce::Component& parent)
{
    ComponentType* found = nullptr;

    for (auto* child : parent.getChildren())
        if (found == nullptr)
            found = dynamic_cast<ComponentType*> (child);

    INFO ("Kein direktes Kind des gesuchten Typs");
    REQUIRE (found != nullptr);
    return *found;
}

/// Erstes Kind vom Typ T in beliebiger Tiefe (Tiefensuche in Kind-Reihenfolge).
template <typename ComponentType>
ComponentType* searchChild (juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* direct = dynamic_cast<ComponentType*> (child))
            return direct;

        if (auto* nested = searchChild<ComponentType> (*child))
            return nested;
    }

    return nullptr;
}

template <typename ComponentType>
ComponentType& findChild (juce::Component& root)
{
    auto* found = searchChild<ComponentType> (root);
    INFO ("Kein Kind des gesuchten Typs");
    REQUIRE (found != nullptr);
    return *found;
}

/// Rechteck eines Kindes (beliebig tief) in Koordinaten von root.
inline juce::Rectangle<int> boundsIn (juce::Component& root, juce::Component& child)
{
    return root.getLocalArea (&child, child.getLocalBounds());
}

/// Markierung für eine niedrige, breite Fläche (Lineal, Markerspur, Trenner): Rahmen etwas kürzer,
/// damit die Nummer rechts daneben noch im Bild liegt statt hinter dem Fensterrand.
inline Callout stripCallout (int number, juce::Rectangle<int> area)
{
    return { number, area.withTrimmedRight (26), Callout::Placement::inside };
}

/// Mono-WAV mit abklingenden Schlägen, damit Wellenformen im Bild nach Musik aussehen statt nach einer
/// Nulllinie: ab offset alle interval Sekunden ein Ton mit frequency Hz (0 = Rauschen, z. B. Snare).
inline juce::File writeHitsWav (const juce::File& file,
                                double seconds,
                                double interval,
                                double offset,
                                double frequency,
                                double decaySeconds)
{
    constexpr double sampleRate = 48000.0;
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::AudioBuffer<float> buffer (1, static_cast<int> (seconds * sampleRate));
    juce::Random random (42); // fester Startwert: jedes Bild gleich

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double t = static_cast<double> (i) / sampleRate;
        float value = 0.0f;

        if (t >= offset)
        {
            const double sinceHit = std::fmod (t - offset, interval);
            const double envelope = std::exp (-sinceHit / decaySeconds);
            const double tone = frequency > 0.0 ? std::sin (2.0 * juce::MathConstants<double>::pi * frequency * sinceHit)
                                                : static_cast<double> (random.nextFloat() * 2.0f - 1.0f);
            value = static_cast<float> (0.8 * envelope * tone);
        }

        buffer.setSample (0, i, value);
    }

    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = format.createWriterFor (
            stream,
            juce::AudioFormatWriterOptions {}.withSampleRate (sampleRate).withNumChannels (1).withBitsPerSample (16));
        REQUIRE (writer != nullptr);
        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
    } // Writer schließt die Datei hier, vor dem Import

    return file;
}

/// Demo-Projekt "Demo Song" (120 BPM, 4/4) und das Projektfenster dazu:
///   Ordner "Drums" mit Audiospuren "Kick" und "Snare" (je ein Clip, 8 Takte),
///   Audiospur "Bass" (Clip ab Takt 5), Instrumentenspur "Synth" (FK Synth, MIDI-Clip mit Akkorden, 8 Takte),
///   Marker "Intro" (Takt 1) und "Verse" (Takt 5), Locatoren Takt 1 bis 5.
/// Das Projekt entsteht vollständig vor dem Fenster: Spätere Änderungen lösen im Fenster die
/// Abtastraten-Rückfrage aus (eigenes Dialogfenster, nicht im Bild, aber unnötig).
struct ProjectWindowScene
{
    ProjectWindowScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        env.engineContext.registerInternalPlugins();
        REQUIRE (env.projectService.createProject (env.tempFolder, "Demo Song").has_value());
        buildDemoProject();
    }

    ~ProjectWindowScene()
    {
        window.reset(); // Fenster vor den Services abbauen (es hängt als Listener daran)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    /// Baut das Projektfenster in der gewünschten Größe und lässt Wellenformen und Layout fertig werden.
    fk::ui::ProjectComponent& open (int width, int height)
    {
        window = std::make_unique<fk::ui::ProjectComponent> (services.ui());
        window->setSize (width, height);
        TestEnvironment::pumpMessages (300);
        return *window;
    }

    /// Ganzes Projekt in der Breite zeigen (wie View › Show whole project).
    void zoomToProject()
    {
        REQUIRE (window != nullptr);
        findDirectChild<fk::ui::ArrangeView> (*window).zoomToProject();
        TestEnvironment::pumpMessages (100);
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    FullServices services { env };
    double barSeconds = 2.0;
    fk::services::TrackHandle* synthTrack = nullptr;
    juce::String synthClipId;
    std::unique_ptr<fk::ui::ProjectComponent> window;

private:
    template <typename Value>
    static Value valueOf (fk::core::Result<Value> result)
    {
        INFO ((result.has_value() ? juce::String() : juce::String (result.error().message)));
        REQUIRE (result.has_value());
        return *result;
    }

    void buildDemoProject()
    {
        auto& trackService = env.trackService;
        barSeconds = env.timelineService.barLengthSeconds (0.0);
        const double songSeconds = 8.0 * barSeconds;
        const double beat = barSeconds / 4.0;
        const auto audioFolder = env.tempFolder.getChildFile ("DemoAudio");

        auto* drums = valueOf (trackService.createFolderTrack ("Drums"));
        auto* kick = valueOf (trackService.createAudioTrack ("Kick", 1));
        auto* snare = valueOf (trackService.createAudioTrack ("Snare", 1));
        REQUIRE (trackService.moveIntoFolder (*kick, drums).has_value());
        REQUIRE (trackService.moveIntoFolder (*snare, drums).has_value());
        auto* bass = valueOf (trackService.createAudioTrack ("Bass", 1));

        const auto synthId = TestEnvironment::synthId (services.plugins);
        REQUIRE (synthId.isNotEmpty());
        synthTrack = valueOf (trackService.createInstrumentTrack ("Synth", synthId));

        // Kick auf jedem Schlag, Snare auf 2 und 4, Bass in Achteln ab Takt 5.
        const auto kickFile = writeHitsWav (audioFolder.getChildFile ("Kick.wav"), songSeconds, beat, 0.0, 55.0, 0.12);
        const auto snareFile = writeHitsWav (audioFolder.getChildFile ("Snare.wav"), songSeconds, 2.0 * beat, beat, 0.0, 0.08);
        const auto bassFile = writeHitsWav (audioFolder.getChildFile ("Bass.wav"), 4.0 * barSeconds, beat / 2.0, 0.0, 82.4, 0.2);
        (void) valueOf (env.clipService.importAudioFile (*kick, kickFile, 0.0));
        (void) valueOf (env.clipService.importAudioFile (*snare, snareFile, 0.0));
        (void) valueOf (env.clipService.importAudioFile (*bass, bassFile, 4.0 * barSeconds));

        // Synth: je Takt ein Akkord (C, Am, F, G, zweimal), ganze Noten.
        synthClipId = valueOf (services.midiClips.createEmptyClip (*synthTrack, 0.0, songSeconds));
        const std::vector<std::vector<int>> chords { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 } };
        std::vector<fk::services::EditableNote> notes;

        for (int bar = 0; bar < 8; ++bar)
            for (const int pitch : chords[static_cast<std::size_t> (bar % 4)])
                notes.push_back ({ -1, pitch, 4.0 * bar, 4.0, 90, {} });

        REQUIRE (services.midiClips.addNotes (synthClipId, notes, "Chords").has_value());

        (void) valueOf (services.markers.addMarker (0.0, "Intro"));
        (void) valueOf (services.markers.addMarker (4.0 * barSeconds, "Verse"));
        env.timelineService.setLoopRange ({ 0.0, 4.0 * barSeconds });
    }
};

} // namespace fk::tests::manual
