#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace fk::plugins::ui
{

/// Ein Bedienelement in der Beschreibung einer Plugin-Oberfläche.
/// Parameter-Elemente nennen nur die ID; welcher Baustein daraus wird, entscheidet der Typ des
/// Parameters (Bool → Switch, Choice → Choice, sonst Knob). Anzeigen bringen eine Lesefunktion mit.
struct Control
{
    enum class Kind
    {
        parameter,     ///< Knob, Switch oder Choice je nach Parametertyp
        levelMeter,    ///< Pegelanzeige, liest readDb
        gainReduction, ///< Gain-Reduction-Anzeige, liest readDb (≤ 0)
        text,          ///< Lcd mit festem Text (Hinweis, Version)
        action,        ///< Schaltfläche mit Beschriftung, ruft onClick (Laden…, Zurücksetzen)
        status         ///< Lcd, das seinen Text laufend aus readText liest (Dateiname, Stimmen)
    };

    Kind kind = Kind::parameter;
    juce::String parameterId;
    juce::String label;                     ///< Anzeigen und Text; Parameter nehmen ihren Namen
    std::function<float()> readDb;          ///< für Anzeigen: liest ein Atomic des Prozessors
    std::function<void()> onClick;          ///< für action
    std::function<juce::String()> readText; ///< für status
    juce::String tooltip;                   ///< Erklärung für Parameter-Elemente; leer = nur der Name
};

/// Ein Abschnitt: Titel und eine Reihe Bedienelemente, als ModulePanel gezeichnet. Mit scene
/// ist er stattdessen eine Szene: eine eigene Komponente über die volle Breite,
/// z. B. Spektrum mit Kurve und Griffen. Die Fabrik läuft beim Bau des Editors im Message-Thread.
struct Section
{
    juce::String title;
    std::vector<Control> controls;
    bool proOnly = false; ///< in Lite gesperrt sichtbar (Schloss), nie versteckt
    std::function<std::unique_ptr<juce::Component>()> scene;
    int sceneHeight = 220;
};

/// Die ganze Oberfläche als Daten. Ein Plugin liefert sie in describePanel(); PanelEditor baut
/// daraus Bausteine. Kein Layout-Code im Plugin.
struct PanelDescription
{
    juce::String title;
    std::vector<Section> sections;
};

// Kurzschreibweisen für describePanel(): { "Dynamik", { knob ("threshold"), knob ("ratio") } }.
// "knob" steht für jedes Parameter-Element; Bool und Choice werden automatisch zum passenden Baustein.
[[nodiscard]] inline Control knob (juce::String parameterId)
{
    return { .kind = Control::Kind::parameter, .parameterId = std::move (parameterId) };
}

/// Parameter-Element mit Erklärung im Tooltip: was der Regler klanglich bewirkt.
/// Regler mit eigenem Anzeigenamen (statt des Parameternamens).
[[nodiscard]] inline Control namedKnob (juce::String parameterId, juce::String label)
{
    Control control = knob (std::move (parameterId));
    control.label = std::move (label);
    return control;
}

[[nodiscard]] inline Control knob (juce::String parameterId, juce::String tooltip)
{
    Control control = knob (std::move (parameterId));
    control.tooltip = std::move (tooltip);
    return control;
}

[[nodiscard]] inline Control levelMeter (juce::String label, std::function<float()> readDb)
{
    return { .kind = Control::Kind::levelMeter, .label = std::move (label), .readDb = std::move (readDb) };
}

[[nodiscard]] inline Control gainReduction (std::function<float()> readDb)
{
    return { .kind = Control::Kind::gainReduction, .label = "GR", .readDb = std::move (readDb) };
}

[[nodiscard]] inline Control action (juce::String label, std::function<void()> onClick)
{
    Control control;
    control.kind = Control::Kind::action;
    control.label = std::move (label);
    control.onClick = std::move (onClick);
    return control;
}

[[nodiscard]] inline Control status (std::function<juce::String()> readText)
{
    Control control;
    control.kind = Control::Kind::status;
    control.readText = std::move (readText);
    return control;
}

[[nodiscard]] inline Control text (juce::String label)
{
    return { .kind = Control::Kind::text, .label = std::move (label) };
}

} // namespace fk::plugins::ui
