#include "fk/services/ContentService.h"

#include "fk/services/BundledPlugins.h"
#include "fk/services/LibraryService.h"
#include "fk/services/PluginService.h"
#include "fk/services/SamplerService.h"
#include "fk/services/UpdateService.h"
#include "fk/ui/ContentManagerDialog.h"
#include "fk/ui/InstrumentBrowser.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include "TestEnvironment.h"

using fk::services::ContentPackage;
using fk::services::ContentService;
using fk::tests::TestEnvironment;

namespace
{
/// Ein kleines Paket wie die echten (Ordner im Archiv, SFZ in Unterordnern, Samples daneben).
juce::File buildPackageZip (const juce::File& folder)
{
    const auto source = folder.getChildFile ("source");
    const auto strings = source.getChildFile ("Lib-1.0/Strings");
    strings.createDirectory();

    juce::AudioBuffer<float> sine (1, 4800);
    for (int i = 0; i < sine.getNumSamples(); ++i)
        sine.setSample (
            0, i, 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0)));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream =
        std::make_unique<juce::FileOutputStream> (strings.getChildFile ("violin.wav"));
    auto writer = format.createWriterFor (
        stream, juce::AudioFormatWriterOptions {}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (16));
    writer->writeFromAudioSampleBuffer (sine, 0, sine.getNumSamples());
    writer.reset();

    strings.getChildFile ("Violin Sustain.sfz").replaceWithText ("<region> sample=violin.wav key=60\n");
    strings.getChildFile ("Violin Pizz.sfz").replaceWithText ("<region> sample=violin.wav key=60\n");
    source.getChildFile ("Lib-1.0/README.txt").replaceWithText ("Test");

    juce::ZipFile::Builder builder;
    for (const auto& file : source.findChildFiles (juce::File::findFiles, true))
        builder.addFile (file, 9, file.getRelativePathFrom (source).replaceCharacter ('\\', '/'));

    const auto zip = folder.getChildFile ("lib.zip");
    juce::FileOutputStream out (zip);
    builder.writeToStream (out, nullptr);
    return zip;
}
} // namespace

TEST_CASE ("B5a: Content-Paket laden, Instrumente finden, entfernen", "[services][content][b5]")
{
    TestEnvironment env;
    const auto zip = buildPackageZip (env.tempFolder.getChildFile ("server"));

    ContentPackage package { "test-lib", "Test-Bibliothek", "Zum Testen", juce::URL (zip).toString (false),
                             "CC0",      zip.getSize() };
    ContentPackage huge { "huge", "Riesig",
                          "",     juce::URL (zip).toString (false),
                          "CC0",  std::numeric_limits<juce::int64>::max() / 4 };
    ContentPackage broken { "broken", "Kaputt", "", "file:///gibt/es/nicht.zip", "CC0", 10 };
    ContentService content { env.tempFolder.getChildFile ("Content"), { package, huge, broken } };

    REQUIRE_FALSE (content.isInstalled ("test-lib"));
    REQUIRE (content.getInstruments().empty());
    REQUIRE_FALSE (content.install ("unbekannt").has_value());
    REQUIRE_FALSE (content.install ("huge").has_value()); // Platzprüfung
    REQUIRE_FALSE (content.install ("broken").has_value());

    // Abbrechen: nichts bleibt liegen.
    const auto cancelled = content.install ("test-lib", nullptr, [] { return true; });
    REQUIRE_FALSE (cancelled.has_value());
    REQUIRE (cancelled.error().code == fk::core::ErrorCode::cancelled);
    REQUIRE_FALSE (content.isInstalled ("test-lib"));
    REQUIRE_FALSE (content.getPackageFolder ("test-lib").exists());

    float lastProgress = 0.0f;
    REQUIRE (content.install ("test-lib", [&lastProgress] (float p) { lastProgress = p; }).has_value());
    REQUIRE (lastProgress >= 0.99f);
    REQUIRE (content.isInstalled ("test-lib"));
    REQUIRE (content.install ("test-lib").has_value()); // schon da: kein Fehler

    const auto instruments = content.getInstruments();
    REQUIRE (instruments.size() == 2);
    REQUIRE (instruments[0].name == "Violin Pizz");
    REQUIRE (instruments[1].name == "Violin Sustain");
    REQUIRE (instruments[0].category == "Lib-1.0/Strings");
    REQUIRE (instruments[0].packageId == "test-lib");

    // Instrument als Sampler-Spur.
    env.engineContext.registerInternalPlugins();
    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Bibliothek").has_value());
    const auto track = sampler.createTrackFromSfz (instruments[1].sfzFile, instruments[1].name);
    REQUIRE (track.has_value());
    REQUIRE (env.trackService.getTrackInfo (**track).name == "Violin Sustain");
    REQUIRE (env.trackService.getTrackInfo (**track).isInstrument);
    REQUIRE_FALSE (sampler.createTrackFromSfz (env.tempFolder.getChildFile ("fehlt.sfz"), "x").has_value());

    REQUIRE (content.remove ("test-lib").has_value());
    REQUIRE_FALSE (content.isInstalled ("test-lib"));
    REQUIRE (content.getInstruments().empty());
    REQUIRE_FALSE (content.remove ("test-lib").has_value());

    // Die mitgelieferte Liste nennt Lizenz und Größe.
    for (const auto& entry : ContentService::defaultCatalog())
    {
        REQUIRE (entry.url.startsWith ("https://"));
        REQUIRE (entry.licence.isNotEmpty());
        REQUIRE (entry.sizeBytes > 0);
    }
}

TEST_CASE ("B5a: Content-Manager lädt im Hintergrund", "[ui][content][b5]")
{
    TestEnvironment env;
    const auto zip = buildPackageZip (env.tempFolder.getChildFile ("server"));
    ContentService content {
        env.tempFolder.getChildFile ("Content"),
        { { "test-lib", "Test-Bibliothek", "Zum Testen", juce::URL (zip).toString (false), "CC0", zip.getSize() } }
    };

    fk::ui::ContentManagerDialog dialog { content };
    int changes = 0;
    dialog.onContentChanged = [&changes] { ++changes; };
    REQUIRE (dialog.getStatusText ("test-lib") == "not installed");

    dialog.startInstall ("test-lib");
    REQUIRE (dialog.isBusy());

    for (int i = 0; i < 100 && dialog.isBusy(); ++i)
        TestEnvironment::pumpMessages (50);

    REQUIRE_FALSE (dialog.isBusy());
    REQUIRE (dialog.getStatusText ("test-lib") == "installed");
    REQUIRE (changes == 1);
}

TEST_CASE ("B5b: Instrumenten-Browser mit Suche, Tags, Favoriten und Spuren", "[services][ui][library][b5]")
{
    TestEnvironment env;
    env.engineContext.registerInternalPlugins();
    const auto zip = buildPackageZip (env.tempFolder.getChildFile ("server"));
    ContentService content {
        env.tempFolder.getChildFile ("Content"),
        { { "test-lib", "Test-Bibliothek", "", juce::URL (zip).toString (false), "CC0", zip.getSize() } }
    };
    REQUIRE (content.install ("test-lib").has_value());

    fk::services::PluginService plugins { env.engineContext, env.projectService };
    fk::services::SamplerService sampler { env.projectService, env.trackService, plugins };
    fk::services::LibraryService library {
        plugins, content, sampler, env.trackService, env.tempFolder.getChildFile ("Bibliothek.json")
    };
    REQUIRE (env.projectService.createProject (env.tempFolder, "Browser").has_value());

    // Ein eigenes Preset für FK Synth anlegen (Lautstärke-Parameter verstellt).
    auto* synthTrack = *env.trackService.createInstrumentTrack ("Synth", TestEnvironment::synthId (plugins));
    const auto synthInsert = plugins.getInstrument (*synthTrack)->id;
    // Ein stufenloser Parameter (Auswahl-Parameter würden 0,123 auf eine Stufe runden).
    const auto continuousParameterIndex = [] (juce::AudioPluginInstance& instance)
    {
        const auto& parameters = instance.getParameters();

        for (int i = 0; i < parameters.size(); ++i)
            if (! parameters[i]->isDiscrete())
                return i;

        return 0;
    };

    juce::AudioProcessorParameter* firstParameter = nullptr;
    int parameterIndex = 0;
    for (auto* plugin : synthTrack->pluginList.getPlugins())
        if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
        {
            parameterIndex = continuousParameterIndex (*external->getAudioPluginInstance());
            firstParameter = external->getAudioPluginInstance()->getParameters()[parameterIndex];
        }
    firstParameter->setValueNotifyingHost (0.123f);
    for (const auto& name : plugins.getUserPresets (synthInsert))
        REQUIRE (plugins.deleteUserPreset (synthInsert, name).has_value());
    REQUIRE (plugins.saveUserPreset (synthInsert, "Weicher Bass").has_value());

    const auto items = library.getItems();
    const auto count = [&items] (fk::services::LibraryItem::Kind kind)
    { return std::ranges::count (items, kind, &fk::services::LibraryItem::kind); };
    REQUIRE (count (fk::services::LibraryItem::Kind::plugin) >= 2); // FK Synth, FK Sampler
    REQUIRE (count (fk::services::LibraryItem::Kind::sfz) == 2);
    REQUIRE (count (fk::services::LibraryItem::Kind::preset) == 1);

    // Suche: alle Wörter müssen passen; Tags aus Paket und Ordner.
    REQUIRE (library.search ("violin pizz", {}, false).size() == 1);
    REQUIRE (library.search ("", "Strings", false).size() == 2);
    REQUIRE (library.search ("bass", {}, false).front().kind == fk::services::LibraryItem::Kind::preset);
    REQUIRE (library.getAllTags().contains ("Test-Bibliothek"));

    // Favoriten: SFZ in der eigenen Datei, Plugins geteilt mit dem Plugin-Manager.
    const auto violin = library.search ("Violin Sustain", {}, false).front();
    REQUIRE (library.setFavourite (violin.key, true).has_value());
    REQUIRE (library.search ("", {}, true).size() == 1);
    REQUIRE (library.getItems().front().key == violin.key); // Favoriten zuerst
    const auto synthItem = library.search ("FK Synth", "Plugin", false).front();
    REQUIRE (library.setFavourite (synthItem.key, true).has_value());
    REQUIRE (std::ranges::any_of (plugins.getAvailableInstruments(),
                                  [] (const auto& p) { return p.name == "FK Synth" && p.favourite; }));
    REQUIRE (library.setFavourite (synthItem.key, false).has_value());
    REQUIRE_FALSE (library.setFavourite ("sfz:/gibt/es/nicht.sfz", true).has_value());

    // Eigene Tags.
    REQUIRE (library.addTag (violin.key, "Lieblingsstreicher").has_value());
    REQUIRE (library.search ("", "Lieblingsstreicher", false).size() == 1);
    REQUIRE_FALSE (library.removeTag (violin.key, "Strings").has_value()); // automatischer Tag
    REQUIRE (library.removeTag (violin.key, "Lieblingsstreicher").has_value());
    REQUIRE_FALSE (library.addTag (violin.key, "  ").has_value());

    // Spuren für alle drei Arten; das Preset kommt geladen an.
    firstParameter->setValueNotifyingHost (0.9f);
    const auto before = env.trackService.getAudioTracks().size();
    REQUIRE (library.createTrack (violin.key).has_value());
    REQUIRE (library.createTrack (synthItem.key).has_value());
    const auto presetTrack = library.createTrack (library.search ("Weicher Bass", {}, false).front().key);
    REQUIRE (presetTrack.has_value());
    REQUIRE (env.trackService.getAudioTracks().size() == before + 3);
    REQUIRE (env.trackService.getTrackInfo (**presetTrack).name == "Weicher Bass");

    for (auto* plugin : (*presetTrack)->pluginList.getPlugins())
        if (auto* external = dynamic_cast<tracktion::ExternalPlugin*> (plugin))
            REQUIRE (external->getAudioPluginInstance()->getParameters()[parameterIndex]->getValue() ==
                     Catch::Approx (0.123f).margin (0.01f));

    // Oberfläche: Suche, Stern, Doppelklick-Weg.
    fk::ui::InstrumentBrowser browser { library };
    browser.setSize (800, 400);
    browser.setSearchText ("Violin");
    REQUIRE (browser.getItems().size() == 2);
    browser.setFavouritesOnly (true);
    REQUIRE (browser.getItems().size() == 1);
    browser.toggleFavourite (0);
    REQUIRE (browser.getItems().empty());
    browser.setFavouritesOnly (false);
    browser.createTrackForRow (0);
    REQUIRE (env.trackService.getAudioTracks().size() == before + 4);
}

TEST_CASE ("C: Update-Hinweis aus latest-<Plattform>.json", "[services][update][c]")
{
    using fk::services::UpdateService;
    REQUIRE (UpdateService::isNewer ("0.10.0", "0.9.5"));
    REQUIRE (UpdateService::isNewer ("1.0", "0.99.99"));
    REQUIRE_FALSE (UpdateService::isNewer ("0.1.0", "0.1.0 (beta, abc123)"));
    REQUIRE_FALSE (UpdateService::isNewer ("0.1.0", "0.2.0"));
    REQUIRE (UpdateService::isNewer ("0.1.1", "0.1"));

    REQUIRE_FALSE (UpdateService::parseManifest ("{\"version\":\"1.0\"}").has_value()); // ohne url
    // Nur https: Die App öffnet die Adresse im Browser.
    REQUIRE_FALSE (UpdateService::parseManifest (R"({"version":"1.0","url":"file:///etc/passwd"})").has_value());
    REQUIRE_FALSE (UpdateService::parseManifest (R"({"version":"1.0","url":"http://example.org/a"})").has_value());
    REQUIRE (UpdateService::parseManifest (R"({"version":"1.0","url":"https://example.org/a"})").has_value());
    REQUIRE (UpdateService::parseManifest (R"json({ "releases": [ { "version": "9.0", "platform": "linux", "file": "x",
               "url": "javascript:alert(1)" } ] })json", "linux-x64").error().code == fk::core::ErrorCode::notFound);

    // Release-Liste von fk-licensing: höchste Version der eigenen Plattform, Installer vor Zip.
    const auto list = juce::String (R"({ "releases": [
        { "version": "0.5.0", "platform": "linux", "file": "a.AppImage", "url": "https://l/v1/downloads/1/" },
        { "version": "0.6.0", "platform": "windows", "file": "b.zip", "url": "https://l/v1/downloads/2/" },
        { "version": "0.6.0", "platform": "windows", "file": "b-setup.exe", "url": "https://l/v1/downloads/3/",
          "published_at": "2026-11-02T10:00:00+00:00" },
        { "version": "0.5.1", "platform": "linux", "file": "c.AppImage", "url": "https://l/v1/downloads/4/" } ] })");
    REQUIRE (UpdateService::parseManifest (list, "linux-x64")->url == "https://l/v1/downloads/4/");
    REQUIRE (UpdateService::parseManifest (list, "windows-x64")->url == "https://l/v1/downloads/3/");
    REQUIRE (UpdateService::parseManifest (list, "windows-x64")->date == "2026-11-02");
    REQUIRE (UpdateService::parseManifest (R"({ "releases": [] })", "linux-x64").error().code
             == fk::core::ErrorCode::notFound);

    TestEnvironment env;
    const auto manifest = env.tempFolder.getChildFile ("latest-linux-x64.json");
    manifest.getParentDirectory().createDirectory();
    manifest.replaceWithText (R"({ "version": "0.2.0", "channel": "beta", "date": "2026-10-20",
                                   "url": "https://example.org/FK-Audio-Suite-0.2.0-linux-x64.AppImage" })");

    const UpdateService older { juce::URL (manifest).toString (false),
                                "0.1.0",
                                env.tempFolder.getChildFile ("Updates.txt") };
    const auto found = older.check();
    REQUIRE (found.has_value());
    REQUIRE (found->has_value());
    REQUIRE ((*found)->version == "0.2.0");

    const UpdateService current { juce::URL (manifest).toString (false), "0.2.0" };
    REQUIRE (current.check().has_value());
    REQUIRE_FALSE (current.check()->has_value());

    // Noch nichts veröffentlicht: kein Fehler, nur kein Update.
    const auto empty = env.tempFolder.getChildFile ("releases.json");
    empty.replaceWithText (R"({ "releases": [] })");
    const UpdateService none { juce::URL (empty).toString (false), "0.5.0", {}, "linux-x64" };
    REQUIRE (none.check().has_value());
    REQUIRE_FALSE (none.check()->has_value());

    UpdateService off { {}, "0.1.0", env.tempFolder.getChildFile ("Updates.txt") };
    REQUIRE_FALSE (off.isAvailable());
    REQUIRE_FALSE (off.check().has_value());
    REQUIRE (off.isAutomaticCheckEnabled());
    off.setAutomaticCheckEnabled (false);
    REQUIRE_FALSE (off.isAutomaticCheckEnabled());
}

TEST_CASE ("C: Mitgelieferte Lite-Plugins in den VST3-Ordner installieren", "[services][bundled][c]")
{
    using fk::services::BundledPlugins;

    TestEnvironment env;
    const auto source = env.tempFolder.getChildFile ("AppDir/usr/lib/vst3");
    const auto target = env.tempFolder.getChildFile ("home/.vst3");
    const auto binary = source.getChildFile ("FK Rig.vst3/Contents/x86_64-linux/FK Rig.so");
    binary.getParentDirectory().createDirectory();
    binary.replaceWithText ("Version 1");

    // Entwickler-Build: nichts mitgeliefert, nichts zu tun.
    REQUIRE (BundledPlugins ({ env.tempFolder.getChildFile ("fehlt"), target }, "0.5.0+a").install()->empty());

    const auto first = BundledPlugins ({ source, target }, "0.5.0+a").install();
    REQUIRE (first.has_value());
    REQUIRE (first->size() == 1);
    REQUIRE (first->front() == target.getChildFile ("FK Rig.vst3"));
    const auto installed = target.getChildFile ("FK Rig.vst3/Contents/x86_64-linux/FK Rig.so");
    REQUIRE (installed.loadFileAsString() == "Version 1");

    // Neue Auslieferung ersetzt die alte, derselbe Stand lässt sie liegen.
    binary.replaceWithText ("Version 2");
    REQUIRE (BundledPlugins ({ source, target }, "0.5.0+a").install().has_value());
    REQUIRE (installed.loadFileAsString() == "Version 1");
    REQUIRE (BundledPlugins ({ source, target }, "0.5.1+b").install().has_value());
    REQUIRE (installed.loadFileAsString() == "Version 2");

    // Selbst installiert (ohne Kennzeichen): nie überschreiben, aber trotzdem melden.
    target.getChildFile ("FK Rig.vst3").getChildFile (BundledPlugins::markerFileName).deleteFile();
    REQUIRE (BundledPlugins ({ source, target }, "0.6.0+c").install()->size() == 1);
    REQUIRE (installed.loadFileAsString() == "Version 2");

    // Windows: Bundles bleiben im Programmordner.
    const auto inPlace = BundledPlugins ({ source, {} }, "0.5.0+a").install();
    REQUIRE (inPlace->front() == source.getChildFile ("FK Rig.vst3"));

    // Windows-Installer: geteilter VST3-Ordner mit fremden Plugins, nur die genannten zählen.
    REQUIRE (source.getChildFile ("Fremdes Plugin.vst3").createDirectory());
    const auto shared = BundledPlugins ({ source, {}, { "FK Rig.vst3" } }, "0.5.0+a").install();
    REQUIRE (shared->size() == 1);
    REQUIRE (shared->front() == source.getChildFile ("FK Rig.vst3"));
}
