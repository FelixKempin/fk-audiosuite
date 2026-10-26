#pragma once

#include "fk/plugins/edition/FeatureGate.h"
#include "fk/plugins/ui/ModulePanel.h"
#include "fk/plugins/ui/PanelDescription.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace fk::plugins::ui
{

/// Baut aus einer PanelDescription die Abschnitte als ModulePanels mit Bausteinen in einer Reihe.
/// Abschnitte fließen von links nach rechts und brechen bei maxWidth um; Szenen nehmen eine ganze
/// Reihe. Die bevorzugte Größe folgt dem Inhalt. Eine Komponente ohne Fenster: PanelEditor
/// setzt sie unter seine Kopfzeile, Produkte zeigen damit das gewählte Modul ihrer Kette.
///
/// Mit FeatureGate werden proOnly-Abschnitte in Lite gesperrt gezeichnet und folgen einem
/// Editionswechsel. Ohne Gate (DAW-Effekte) gibt es keine Sperren.
class PanelView final : public juce::Component, private edition::FeatureGate::Listener
{
public:
    static constexpr int maxWidth = 900;

    PanelView (juce::AudioProcessorValueTreeState& parameters,
               PanelDescription description,
               edition::FeatureGate* featureGate = nullptr);
    ~PanelView() override;

    /// Breite und Höhe, die der Inhalt braucht (ohne Kopfzeile).
    [[nodiscard]] int getPreferredWidth() const;
    [[nodiscard]] int getPreferredHeight() const;

    /// Zahl der gebauten Bausteine (Tests: jeder Parameter genau einmal).
    [[nodiscard]] int getNumControls() const noexcept;

    /// Bausteine, deren Parameter-ID es nicht gibt, erscheinen als Lcd mit "?" statt zu fehlen.
    [[nodiscard]] int getNumUnresolvedControls() const noexcept;

    /// Zugriff auf einen Baustein über die Parameter-ID (Tests, Szenen, die Knobs spiegeln).
    [[nodiscard]] juce::Component* findControl (const juce::String& parameterId) const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct BuiltControl
    {
        juce::String parameterId;
        int width = 0;
        std::unique_ptr<juce::Component> component;
    };

    struct BuiltSection
    {
        std::unique_ptr<ModulePanel> panel;
        std::vector<BuiltControl> controls;
        std::unique_ptr<juce::Component> scene; ///< gesetzt statt controls bei Szenen-Abschnitten
        int sceneHeight = 0;
        bool proOnly = false;
        [[nodiscard]] int width() const;
        [[nodiscard]] int height() const;
    };

    /// Platz jedes Abschnitts (Reihenfolge wie sections) und die Gesamtgröße.
    struct Layout
    {
        std::vector<juce::Rectangle<int>> panels;
        int width = 0;
        int height = 0;
    };

    void editionChanged (edition::Edition newEdition) override;
    void applyLocks();

    BuiltControl build (const Control& control);
    [[nodiscard]] Layout planLayout() const;
    [[nodiscard]] static int widthFor (const juce::Component& component);
    [[nodiscard]] static int sectionHeight();

    juce::AudioProcessorValueTreeState& parameters;
    PanelDescription description;
    edition::FeatureGate* gate;
    std::vector<BuiltSection> sections;
    int unresolvedControls = 0;
};

} // namespace fk::plugins::ui
