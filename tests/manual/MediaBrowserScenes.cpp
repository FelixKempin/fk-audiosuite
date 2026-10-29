#include "fk/services/ContentService.h"
#include "fk/services/PreviewService.h"
#include "fk/services/WaveformService.h"
#include "fk/ui/ContentManagerDialog.h"
#include "fk/ui/FkLookAndFeel.h"
#include "fk/ui/MediaBrowser.h"
#include "fk/ui/Theme.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <catch2/catch_test_macros.hpp>

#include "ManualShot.h"
#include "ProjectWindowScene.h"

#include <functional>
#include <memory>
#include <vector>

// Bilder für docs/manual/media-browser.md. Ändern sich Medien-Browser oder Content-Manager (Tooltips,
// Aufteilung, Texte), hier die Szenen anpassen und tools/manual.py shots laufen lassen
// (docs/manual/MAINTAINING.md).

using fk::tests::TestEnvironment;
using fk::tests::manual::boundsIn;
using fk::tests::manual::Callout;
using fk::tests::manual::calloutFor;
using fk::tests::manual::findByTooltip;
using fk::tests::manual::saveShot;
using fk::tests::manual::searchChild;
using fk::tests::manual::writeHitsWav;

namespace
{
constexpr int browserWidth = 900;
constexpr int browserHeight = 380;

/// Höhe der Vorschauleiste unten im Medien-Browser (MediaBrowser.cpp, previewHeight). Die Wellenform ist
/// kein eigenes Kind, deshalb hier nachgebaut; ändert sich die Aufteilung, hier mitziehen.
constexpr int mediaPreviewHeight = 72;

/// Platz rechts neben dem Content-Manager für die Nummern der Buttons und Statustexte am rechten Rand.
constexpr int mediaCalloutMargin = 30;

/// Wellenformfläche wie MediaBrowser::paint (Vorschauleiste ohne die Spalte der beiden Buttons).
juce::Rectangle<int> mediaWaveformArea (const juce::Component& browser)
{
    auto area = browser.getLocalBounds()
                    .removeFromBottom (mediaPreviewHeight)
                    .reduced (fk::ui::theme::spacingMedium, fk::ui::theme::spacingSmall);
    area.removeFromLeft (48 + fk::ui::theme::spacingMedium);
    return area;
}

/// Erstes sichtbares Label (beliebig tief), dessen Text so beginnt. Labels haben keinen Tooltip.
juce::Label& mediaLabelStartingWith (juce::Component& root, const juce::String& start)
{
    std::function<juce::Label*(juce::Component&)> search = [&] (juce::Component& parent) -> juce::Label*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->isVisible() && label->getText().startsWith (start))
                    return label;

            if (auto* found = search (*child))
                return found;
        }

        return nullptr;
    };

    auto* found = search (root);
    INFO ("Kein sichtbares Label mit Text \"" << start << "...\"");
    REQUIRE (found != nullptr);
    return *found;
}

/// Feste Änderungszeit, damit die Datumsspalte des Dateibaums in jedem Lauf gleich aussieht.
void mediaFixTime (const juce::File& file)
{
    const juce::Time fixed (2026, 9, 12, 14, 30); // 12. Oktober 2026, 14:30 (Monat ab 0)

    if (! file.setLastModificationTime (fixed))
        WARN ("Änderungszeit nicht gesetzt: " << file.getFullPathName());
}

/// Kleine MIDI-Datei mit einer Spur: ein Takt Achtel auf der angegebenen Note.
juce::File mediaWriteMidi (const juce::File& file, int noteNumber)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::MidiMessageSequence sequence;
    constexpr int ticksPerQuarter = 480;

    for (int i = 0; i < 8; ++i)
    {
        const double start = i * ticksPerQuarter / 2.0;
        sequence.addEvent (juce::MidiMessage::noteOn (1, noteNumber, static_cast<juce::uint8> (100)).withTimeStamp (start));
        sequence.addEvent (juce::MidiMessage::noteOff (1, noteNumber).withTimeStamp (start + ticksPerQuarter / 4.0));
    }

    sequence.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerQuarter);
    midi.addTrack (sequence);

    {
        juce::FileOutputStream stream (file);
        REQUIRE (stream.openedOk());
        REQUIRE (midi.writeTo (stream));
    }

    return file;
}

/// Medien-Browser mit einem festen Demo-Ordner statt des echten Home-Ordners:
///   Ordner "Drum Loops" (aufgeklappt: Kick Loop.wav, Snare Loop.wav) und "Vocals" (zu),
///   dazu Bass Line.wav, Pad.wav, Chords.mid, Groove.mid und Notes.txt (vom Filter ausgeblendet).
struct MediaBrowserScene
{
    MediaBrowserScene()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        // Die Wellenform braucht ein offenes Projekt (WaveformService).
        REQUIRE (env.projectService.createProject (env.tempFolder, "Media").has_value());
        writeDemoFiles();

        browser = std::make_unique<fk::ui::MediaBrowser> (preview, waveforms);
        browser->setSize (browserWidth, browserHeight);

        // Der Browser startet im Home-Ordner; die Szene zeigt stattdessen den Demo-Ordner (der Baum
        // liest seinen Inhalt aus der Liste, die der Startordner sonst setzt).
        tree = searchChild<juce::FileTreeComponent> (*browser);
        REQUIRE (tree != nullptr);
        tree->directoryContentsList.setDirectory (demoFolder, true, true);
        tree->directoryContentsList.refresh();
        waitForScan (tree->directoryContentsList);

        openFolder (demoFolder.getChildFile ("Drum Loops"));
    }

    ~MediaBrowserScene()
    {
        preview.stop();
        browser.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    /// Eine Datei auswählen wie mit einem Klick; Infozeile, Wellenform und Buttons folgen der Auswahl.
    void select (const juce::File& file)
    {
        tree->setSelectedFile (file);
        TestEnvironment::pumpMessages (800); // Wellenform lädt im Hintergrund

        if (tree->getNumSelectedFiles() != 1 || tree->getSelectedFile (0) != file)
            WARN ("Auswahl im Dateibaum nicht wie erwartet: " << file.getFileName());
    }

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    fk::services::WaveformService waveforms { env.projectService };
    fk::services::PreviewService preview { env.engineContext };
    juce::File demoFolder = env.tempFolder.getChildFile ("Media Demo");
    std::unique_ptr<fk::ui::MediaBrowser> browser;
    juce::FileTreeComponent* tree = nullptr;

private:
    static void waitForScan (const juce::DirectoryContentsList& list)
    {
        for (int i = 0; i < 40 && list.isStillLoading(); ++i)
            TestEnvironment::pumpMessages (50);

        TestEnvironment::pumpMessages (200);

        if (list.isStillLoading())
            WARN ("Ordner-Scan nicht fertig: " << list.getDirectory().getFullPathName());
    }

    void openFolder (const juce::File& folder)
    {
        auto* root = tree->getRootItem();
        REQUIRE (root != nullptr);
        juce::TreeViewItem* item = nullptr;

        for (int i = 0; i < root->getNumSubItems(); ++i)
            if (auto* sub = root->getSubItem (i); sub != nullptr && sub->getUniqueName() == folder.getFullPathName())
                item = sub;

        if (item == nullptr)
        {
            WARN ("Ordner nicht im Dateibaum: " << folder.getFullPathName());
            return;
        }

        item->setOpen (true);
        TestEnvironment::pumpMessages (600); // Unterordner wird im Hintergrund gelesen
    }

    void writeDemoFiles()
    {
        constexpr double beat = 0.5; // 120 BPM

        const auto loops = demoFolder.getChildFile ("Drum Loops");
        const auto vocals = demoFolder.getChildFile ("Vocals");
        const std::vector<juce::File> files {
            writeHitsWav (loops.getChildFile ("Kick Loop.wav"), 8.0, beat, 0.0, 55.0, 0.12),
            writeHitsWav (loops.getChildFile ("Snare Loop.wav"), 8.0, 2.0 * beat, beat, 0.0, 0.08),
            writeHitsWav (vocals.getChildFile ("Take 1.wav"), 6.0, 2.0, 0.0, 220.0, 0.8),
            writeHitsWav (demoFolder.getChildFile ("Bass Line.wav"), 8.0, beat / 2.0, 0.0, 82.4, 0.2),
            writeHitsWav (demoFolder.getChildFile ("Pad.wav"), 8.0, 4.0, 0.0, 261.6, 2.5),
            mediaWriteMidi (demoFolder.getChildFile ("Chords.mid"), 60),
            mediaWriteMidi (demoFolder.getChildFile ("Groove.mid"), 36),
        };

        const auto text = demoFolder.getChildFile ("Notes.txt");
        REQUIRE (text.replaceWithText ("Lyrics, second verse"));

        for (const auto& file : files)
            mediaFixTime (file);

        for (const auto& file : { text, loops, vocals })
            mediaFixTime (file);
    }
};
} // namespace

TEST_CASE ("Handbuch: Medien-Browser Übersicht", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    MediaBrowserScene scene;
    auto& browser = *scene.browser;
    scene.select (scene.demoFolder.getChildFile ("Bass Line.wav"));

    // Nummern wie die Tabelle in media-browser.md, Abschnitt "The media browser at a glance".
    // FIX F-1606 (Ordnerwahl, Suche: die obere Leiste würde sich ändern)
    std::vector<Callout> callouts {
        calloutFor (1, browser, findByTooltip (browser, "Start folder of the file tree"), Callout::Placement::above),
        calloutFor (2, browser, mediaLabelStartingWith (browser, "Bass Line.wav"), Callout::Placement::above),
        calloutFor (3, browser, *scene.tree, Callout::Placement::inside),
        calloutFor (4, browser, findByTooltip (browser, "Audition the selected file"), Callout::Placement::inside),
        calloutFor (5, browser, findByTooltip (browser, "Insert at the cursor"), Callout::Placement::inside),
        Callout { 6, mediaWaveformArea (browser), Callout::Placement::inside },
    };

    saveShot (browser, "media-browser/overview", callouts);
}

TEST_CASE ("Handbuch: Medien-Browser mit MIDI-Datei", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    MediaBrowserScene scene;
    auto& browser = *scene.browser;
    scene.select (scene.demoFolder.getChildFile ("Groove.mid"));

    // Nummern wie die Liste in media-browser.md, Abschnitt "MIDI files".
    std::vector<Callout> callouts {
        calloutFor (1, browser, mediaLabelStartingWith (browser, "Groove.mid"), Callout::Placement::above),
        calloutFor (2, browser, findByTooltip (browser, "Audition the selected file"), Callout::Placement::inside),
    };

    saveShot (browser, "media-browser/midi-file", callouts);
}

TEST_CASE ("Handbuch: Content-Manager", "[manual]")
{
    if (fk::tests::manual::imageFolder() == juce::File())
        SKIP ("FK_MANUAL_IMAGE_DIR nicht gesetzt");

    TestEnvironment env;
    fk::ui::FkLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    // Ohne Netz: Das erste Paket gilt als installiert, sobald seine Manifest-Datei im Paketordner liegt
    // (ContentService::isInstalled); heruntergeladen wird nichts.
    fk::services::ContentService content { env.tempFolder.getChildFile ("Content") };
    const auto packages = content.getPackages();
    REQUIRE (packages.size() >= 2);
    const auto installedFolder = content.getPackageFolder (packages.front().id);
    REQUIRE (installedFolder.createDirectory());
    REQUIRE (installedFolder.getChildFile ("fk-package.json").replaceWithText ("{}"));
    REQUIRE (content.isInstalled (packages.front().id));

    {
        fk::ui::ContentManagerDialog dialog (content);

        // Hülle mit Rand rechts: Nummern kleiner Ziele am rechten Rand stehen rechts daneben.
        juce::Component holder;
        holder.setSize (dialog.getWidth() + mediaCalloutMargin, dialog.getHeight());
        holder.addAndMakeVisible (dialog);
        dialog.setTopLeftPosition (0, 0);
        TestEnvironment::pumpMessages (200);

        auto& title = mediaLabelStartingWith (dialog, packages.front().name);
        auto& details = mediaLabelStartingWith (dialog, juce::translate (packages.front().description));
        const auto packageArea = boundsIn (holder, title).getUnion (boundsIn (holder, details));

        // Nummern wie die Tabelle in media-browser.md, Abschnitt "The Content Manager at a glance".
        // FIX F-1605 (Hinweistext im Bild)
        std::vector<Callout> callouts {
            calloutFor (1, holder, mediaLabelStartingWith (dialog, "The libraries come directly"), Callout::Placement::above),
            Callout { 2, packageArea, Callout::Placement::inside },
            calloutFor (3, holder, findByTooltip (dialog, "Delete the package from disk"), Callout::Placement::inside),
            calloutFor (4, holder, mediaLabelStartingWith (dialog, "installed"), Callout::Placement::inside),
        };

        saveShot (holder, "media-browser/content-manager", callouts);
        holder.removeChildComponent (&dialog);
    }

    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
