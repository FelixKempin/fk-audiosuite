#include "fk/plugins/ui/PanelView.h"

#include "fk/plugins/ui/Choice.h"
#include "fk/plugins/ui/Knob.h"
#include "fk/plugins/ui/Lcd.h"
#include "fk/plugins/ui/Meter.h"
#include "fk/plugins/ui/PluginTheme.h"
#include "fk/plugins/ui/StatusText.h"
#include "fk/plugins/ui/Switch.h"

namespace fk::plugins::ui
{

namespace
{
constexpr auto proHint = NEEDS_TRANS ("Included in Pro");
}

PanelView::PanelView (juce::AudioProcessorValueTreeState& parameterState,
                      PanelDescription panelDescription,
                      edition::FeatureGate* featureGate)
    : parameters (parameterState)
    , description (std::move (panelDescription))
    , gate (featureGate)
{
    for (const auto& section : description.sections)
    {
        BuiltSection built;
        built.panel = std::make_unique<ModulePanel> (section.title);
        built.proOnly = section.proOnly;

        if (section.scene)
        {
            built.scene = section.scene();
            built.sceneHeight = section.sceneHeight;
            built.panel->addAndMakeVisible (*built.scene);
        }

        for (const auto& control : section.controls)
        {
            auto builtControl = build (control);
            built.panel->addAndMakeVisible (*builtControl.component);
            built.controls.push_back (std::move (builtControl));
        }

        addAndMakeVisible (*built.panel);
        sections.push_back (std::move (built));
    }

    if (gate != nullptr)
        gate->addListener (this);

    applyLocks();
}

PanelView::~PanelView()
{
    if (gate != nullptr)
        gate->removeListener (this);
}

namespace
{
/// Die Bausteine setzen ihren Tooltip auf ein inneres Element (Slider, ComboBox, Button); die
/// Erklärung muss dorthin, wo die Maus ist.
void setTooltipRecursively (juce::Component& component, const juce::String& tooltip)
{
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (&component))
        client->setTooltip (tooltip);

    for (auto* child : component.getChildren())
        setTooltipRecursively (*child, tooltip);
}
} // namespace

PanelView::BuiltControl PanelView::build (const Control& control)
{
    BuiltControl built;
    built.parameterId = control.parameterId;

    switch (control.kind)
    {
        case Control::Kind::parameter:
        {
            auto* parameter = parameters.getParameter (control.parameterId);

            if (parameter == nullptr)
            {
                ++unresolvedControls;
                built.component = std::make_unique<Lcd> ("? " + control.parameterId);
                break;
            }

            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
                built.component = std::make_unique<Choice> (*choice);
            else if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
                built.component = std::make_unique<Switch> (*parameter);
            else
            {
                auto knob = std::make_unique<Knob> (*parameter);

                if (control.label.isNotEmpty())
                    knob->setDisplayName (control.label);

                built.component = std::move (knob);
            }

            if (control.tooltip.isNotEmpty())
                setTooltipRecursively (*built.component, parameter->getName (64) + ": " + control.tooltip);
            break;
        }
        case Control::Kind::levelMeter:
            built.component = std::make_unique<Meter> (Meter::Style::level, control.readDb, control.label);
            break;
        case Control::Kind::gainReduction:
            built.component = std::make_unique<Meter> (Meter::Style::gainReduction, control.readDb, control.label);
            break;
        case Control::Kind::text:
            built.component = std::make_unique<Lcd> (control.label);
            break;
        case Control::Kind::action:
        {
            auto button = std::make_unique<juce::TextButton> (control.label);
            button->onClick = control.onClick;
            built.component = std::move (button);
            break;
        }
        case Control::Kind::status:
            built.component = std::make_unique<StatusText> (control.readText);
            break;
    }

    built.width = widthFor (*built.component);
    return built;
}

int PanelView::widthFor (const juce::Component& component)
{
    if (dynamic_cast<const Choice*> (&component) != nullptr)
        return theme::choiceWidth;

    if (dynamic_cast<const Switch*> (&component) != nullptr)
        return theme::switchWidth;

    if (dynamic_cast<const Meter*> (&component) != nullptr)
        return theme::meterWidth + theme::spacingLarge;

    if (dynamic_cast<const Lcd*> (&component) != nullptr ||
        dynamic_cast<const juce::TextButton*> (&component) != nullptr)
        return theme::choiceWidth;

    if (dynamic_cast<const StatusText*> (&component) != nullptr)
        return theme::choiceWidth * 3;

    return theme::knobWidth;
}

int PanelView::sectionHeight()
{
    return theme::panelTitleHeight + theme::controlHeight + theme::spacingSmall * 2;
}

int PanelView::BuiltSection::width() const
{
    int total = 0;

    for (const auto& control : controls)
        total += control.width;

    return total + theme::spacingMedium * 2;
}

int PanelView::BuiltSection::height() const
{
    if (scene)
        return theme::panelTitleHeight + sceneHeight + theme::spacingSmall * 2;

    return sectionHeight();
}

PanelView::Layout PanelView::planLayout() const
{
    // Erster Durchlauf: Wie breit werden die Reglerreihen? Szenen passen sich dieser Breite an.
    constexpr int minimumContentWidth = 600;
    int rowWidth = 0;
    int contentWidth = 0;

    for (const auto& section : sections)
    {
        if (section.scene)
            continue;

        const auto width = section.width() + theme::spacingMedium;

        if (rowWidth > 0 && rowWidth + width > maxWidth)
            rowWidth = 0;

        rowWidth += width;
        contentWidth = juce::jmax (contentWidth, rowWidth - theme::spacingMedium);
    }

    contentWidth = juce::jmax (contentWidth, sections.empty() ? 240 : minimumContentWidth);

    // Zweiter Durchlauf: Regler fließen, Szenen nehmen eine ganze Reihe.
    Layout layout;
    int x = 0;
    int y = 0;
    int rowHeight = 0;

    auto newRow = [&]
    {
        if (x > 0)
            y += rowHeight + theme::spacingMedium;

        x = 0;
        rowHeight = 0;
    };

    for (const auto& section : sections)
    {
        const int height = section.height();

        if (section.scene)
        {
            newRow();
            layout.panels.push_back ({ 0, y, contentWidth, height });
            y += height + theme::spacingMedium;
            continue;
        }

        const int width = section.width();

        if (x > 0 && x + width > contentWidth)
            newRow();

        layout.panels.push_back ({ x, y, width, height });
        x += width + theme::spacingMedium;
        rowHeight = juce::jmax (rowHeight, height);
    }

    if (x > 0)
        y += rowHeight;
    else if (y > 0)
        y -= theme::spacingMedium;

    layout.width = contentWidth + theme::spacingMedium * 2;
    layout.height = y + theme::spacingMedium * 2;
    return layout;
}

int PanelView::getPreferredWidth() const
{
    return planLayout().width;
}

int PanelView::getPreferredHeight() const
{
    return planLayout().height;
}

void PanelView::editionChanged (edition::Edition)
{
    applyLocks();
}

void PanelView::applyLocks()
{
    const bool lite = gate != nullptr && ! gate->isPro();

    for (auto& section : sections)
        section.panel->setLocked (section.proOnly && lite, juce::translate (proHint));
}

int PanelView::getNumControls() const noexcept
{
    int count = 0;

    for (const auto& section : sections)
        count += static_cast<int> (section.controls.size());

    return count;
}

int PanelView::getNumUnresolvedControls() const noexcept
{
    return unresolvedControls;
}

juce::Component* PanelView::findControl (const juce::String& parameterId) const
{
    for (const auto& section : sections)
        for (const auto& control : section.controls)
            if (control.parameterId == parameterId)
                return control.component.get();

    return nullptr;
}

void PanelView::paint (juce::Graphics& g)
{
    if (sections.empty())
    {
        g.setColour (theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText (TRANS ("No parameters."), getLocalBounds(), juce::Justification::centred, 2);
    }
}

void PanelView::resized()
{
    const auto layout = planLayout();
    const auto origin = juce::Point<int> (theme::spacingMedium, theme::spacingMedium);

    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        auto& section = sections[i];
        section.panel->setBounds (layout.panels[i] + origin);
        auto content = section.panel->getContentArea();

        if (section.scene)
        {
            section.scene->setBounds (content);
            continue;
        }

        for (auto& control : section.controls)
        {
            auto area = content.removeFromLeft (control.width);

            // Schaltflächen sind flach: mittig in der Reglerhöhe, nicht über die ganze Höhe gestreckt.
            if (dynamic_cast<juce::TextButton*> (control.component.get()) != nullptr)
                area = area.withSizeKeepingCentre (area.getWidth() - theme::spacingSmall * 2, 26);

            control.component->setBounds (area);
        }
    }
}

} // namespace fk::plugins::ui
