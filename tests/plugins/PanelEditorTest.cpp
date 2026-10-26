#include "fk/plugins/ui/PanelEditor.h"

#include "fk/plugins/CompressorProcessor.h"
#include "fk/plugins/DistortionProcessor.h"
#include "fk/plugins/EqProcessor.h"
#include "fk/plugins/GainProcessor.h"
#include "fk/plugins/LimiterProcessor.h"
#include "fk/plugins/PluginCatalog.h"
#include "fk/plugins/SamplerProcessor.h"
#include "fk/plugins/SubtractiveSynthProcessor.h"
#include "fk/plugins/ui/Choice.h"
#include "fk/plugins/ui/Knob.h"
#include "fk/plugins/ui/Switch.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace fk::plugins;

namespace
{
/// Erzeugt den Editor wie ein Host (createEditorAndMakeActive) und gibt ihn als PanelEditor zurück.
struct EditorHolder
{
    explicit EditorHolder (juce::AudioProcessor& processor)
        : editor (dynamic_cast<ui::PanelEditor*> (processor.createEditorAndMakeActive()))
    {
    }

    ~EditorHolder()
    {
        if (editor != nullptr)
            editor->getAudioProcessor()->editorBeingDeleted (editor.get());
    }

    std::unique_ptr<ui::PanelEditor> editor;
};

int numParameters (const juce::AudioProcessor& processor)
{
    return processor.getParameters().size();
}

/// Ein Plugin ohne eigene Beschreibung bekommt die Standardoberfläche (alle Parameter).
class BarePlugin final : public InternalPlugin
{
public:
    BarePlugin()
        : InternalPlugin ({ .name = "Bare", .category = "Test", .uniqueId = 9999 }, createLayout())
    {
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    using InternalPlugin::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        for (int i = 0; i < 8; ++i)
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "p" + juce::String (i), 1 }, "P" + juce::String (i), 0.0f, 1.0f, 0.5f));

        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "On", true));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "A", "B" }, 0));
        return layout;
    }
};
} // namespace

TEST_CASE ("PanelEditor: jedes interne Plugin zeigt jeden Parameter genau einmal", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // Alle Plugins aus dem Katalog: Neue erscheinen hier ohne Änderung am Test.
    std::vector<std::unique_ptr<InternalPlugin>> plugins;

    for (const auto& description : fk::plugins::PluginCatalog::getDescriptions())
    {
        auto instance = fk::plugins::PluginCatalog::createInstance (description);

        if (auto* internal = dynamic_cast<InternalPlugin*> (instance.get()))
        {
            instance.release();
            plugins.emplace_back (internal);
        }
    }

    for (auto& plugin : plugins)
    {
        INFO (plugin->getName());
        EditorHolder holder (*plugin);
        REQUIRE (holder.editor != nullptr);
        REQUIRE (holder.editor->getNumUnresolvedControls() == 0);

        // Jeder Parameter hat einen Baustein; Anzeigen (GR) kommen dazu.
        int parameterControls = 0;
        for (auto* parameter : plugin->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (holder.editor->findControl (ranged->getParameterID()) != nullptr)
                    ++parameterControls;

        REQUIRE (parameterControls == numParameters (*plugin));
        REQUIRE (holder.editor->getWidth() > 0);
        REQUIRE (holder.editor->getWidth() <= ui::PanelEditor::maxWidth + 16);
        REQUIRE (holder.editor->getHeight() > 0);
    }
}

TEST_CASE ("PanelEditor: Standardbeschreibung zeigt alle Parameter, Typ bestimmt den Baustein", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    BarePlugin plugin;
    EditorHolder holder (plugin);

    REQUIRE (holder.editor != nullptr);
    REQUIRE (holder.editor->getNumControls() == numParameters (plugin));
    REQUIRE (dynamic_cast<ui::Knob*> (holder.editor->findControl ("p0")) != nullptr);
    REQUIRE (dynamic_cast<ui::Switch*> (holder.editor->findControl ("on")) != nullptr);
    REQUIRE (dynamic_cast<ui::Choice*> (holder.editor->findControl ("mode")) != nullptr);
}

TEST_CASE ("PanelEditor: Knob schreibt den Parameter, unbekannte IDs werden sichtbar statt zu fehlen", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    BarePlugin plugin;
    auto& parameters = plugin.getParameterState();

    ui::PanelEditor editor (
        plugin,
        parameters,
        { .title = "Test",
          .sections = { { .title = "A", .controls = { ui::knob ("p0"), ui::knob ("gibtEsNicht") } } } });

    REQUIRE (editor.getNumControls() == 2);
    REQUIRE (editor.getNumUnresolvedControls() == 1);
    REQUIRE (editor.findControl ("gibtEsNicht") != nullptr); // Lcd mit "?" statt Lücke

    auto* knob = dynamic_cast<ui::Knob*> (editor.findControl ("p0"));
    REQUIRE (knob != nullptr);

    knob->getSlider().setValue (0.9, juce::sendNotificationSync);

    REQUIRE (parameters.getRawParameterValue ("p0")->load() == Catch::Approx (0.9f));
    plugin.editorBeingDeleted (&editor);
}

TEST_CASE ("PanelEditor: ein Tooltip erklärt den Regler dort, wo die Maus ist", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    BarePlugin plugin;
    auto& parameters = plugin.getParameterState();

    ui::PanelEditor editor (
        plugin,
        parameters,
        { .title = "Test",
          .sections = { { .title = "A", .controls = { ui::knob ("p0", "macht es heller"), ui::knob ("p1") } } } });

    auto* explained = dynamic_cast<ui::Knob*> (editor.findControl ("p0"));
    auto* plain = dynamic_cast<ui::Knob*> (editor.findControl ("p1"));
    REQUIRE (explained != nullptr);
    REQUIRE (plain != nullptr);

    REQUIRE (explained->getSlider().getTooltip().endsWith ("macht es heller"));
    REQUIRE (! plain->getSlider().getTooltip().contains ("macht es heller"));
    plugin.editorBeingDeleted (&editor);
}

TEST_CASE ("PanelEditor: proOnly-Abschnitte sperren in Lite und folgen dem Editionswechsel", "[plugins][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    BarePlugin plugin;
    edition::FeatureGate gate (edition::Edition::lite);

    ui::PanelEditor editor (plugin,
                            plugin.getParameterState(),
                            { .title = "Test",
                              .sections = { { .title = "Frei", .controls = { ui::knob ("p0") } },
                                            { .title = "Pro", .controls = { ui::knob ("p1") }, .proOnly = true } } },
                            &gate);

    REQUIRE (editor.findControl ("p0")->isEnabled());
    REQUIRE_FALSE (editor.findControl ("p1")->isEnabled());

    gate.setEdition (edition::Edition::pro);

    REQUIRE (editor.findControl ("p1")->isEnabled());
    plugin.editorBeingDeleted (&editor);
}

TEST_CASE ("PanelEditor: Schnappschüsse aller internen Plugins als PNG (FK_UI_SNAPSHOT_DIR)", "[plugins][ui][snapshot]")
{
    // Kein Prüf-Test, sondern ein Werkzeug: Mit gesetztem FK_UI_SNAPSHOT_DIR schreibt er jede
    // Plugin-Oberfläche als PNG, um sie ohne laufende DAW anzusehen (Doku, Review).
    const auto directory = juce::SystemStats::getEnvironmentVariable ("FK_UI_SNAPSHOT_DIR", {});

    if (directory.isEmpty())
        SKIP ("FK_UI_SNAPSHOT_DIR nicht gesetzt");

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const juce::File folder (directory);
    REQUIRE (folder.createDirectory());

    // Alle Plugins aus dem Katalog: Neue erscheinen hier ohne Änderung am Test.
    std::vector<std::unique_ptr<InternalPlugin>> plugins;

    for (const auto& description : fk::plugins::PluginCatalog::getDescriptions())
    {
        auto instance = fk::plugins::PluginCatalog::createInstance (description);

        if (auto* internal = dynamic_cast<InternalPlugin*> (instance.get()))
        {
            instance.release();
            plugins.emplace_back (internal);
        }
    }

    for (auto& plugin : plugins)
    {
        EditorHolder holder (*plugin);
        REQUIRE (holder.editor != nullptr);

        const auto image = holder.editor->createComponentSnapshot (holder.editor->getLocalBounds());
        const auto file = folder.getChildFile (plugin->getName().replaceCharacter (' ', '_') + ".png");
        file.deleteFile(); // FileOutputStream hängt sonst an das alte Bild an
        juce::FileOutputStream stream (file);
        REQUIRE (stream.openedOk());
        REQUIRE (juce::PNGImageFormat().writeImageToStream (image, stream));
    }
}
