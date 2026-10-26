#include "fk/ui/InsertSlot.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Icons.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

void InsertSlot::setInsert (std::optional<services::InsertInfo> newInsert)
{
    insert = std::move (newInsert);
    repaint();
}

void InsertSlot::setMeterDb (std::optional<float> gainReductionDb)
{
    if (meterDb != gainReductionDb)
    {
        meterDb = gainReductionDb;
        repaint();
    }
}

void InsertSlot::setBakeMarked (bool marked)
{
    if (bakeMarked != marked)
    {
        bakeMarked = marked;
        repaint();
    }
}

juce::Rectangle<int> InsertSlot::bakeMarkArea() const
{
    if (! insert.has_value() || onBakeMarkClicked == nullptr)
        return {};

    return getLocalBounds().removeFromLeft (bakeMarkWidth).reduced (2);
}

void InsertSlot::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    const bool occupied = insert.has_value();
    const bool enabled = occupied && insert->enabled;

    g.setColour (occupied ? (enabled ? theme::accentSoft : theme::backgroundElevated) : theme::backgroundInput);
    g.fillRoundedRectangle (area, 3.0f);
    g.setColour (occupied && enabled ? theme::accent : theme::borderSubtle);
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    // Gain-Reduction als Balken von rechts nach links am unteren Rand (0 bis -20 dB).
    if (meterDb.has_value() && enabled)
    {
        const float reduction = juce::jlimit (0.0f, 20.0f, -*meterDb);
        const float width = (area.getWidth() - 8.0f) * reduction / 20.0f;
        g.setColour (theme::meterHigh.withAlpha (0.9f));
        g.fillRect (area.getRight() - 4.0f - width, area.getBottom() - 3.0f, width, 2.0f);
    }

    auto textArea = getLocalBounds().reduced (4, 0);

    // Bake-Feld links: leer, markiert (gefüllt) oder gebacken (Schloss rechts, Text gedimmt).
    if (const auto mark = bakeMarkArea(); ! mark.isEmpty())
    {
        g.setColour (bakeMarked || insert->baked ? theme::accent : theme::borderSubtle);

        if (bakeMarked || insert->baked)
            g.fillRect (mark);
        else
            g.drawRect (mark);

        textArea.removeFromLeft (bakeMarkWidth);
    }

    if (occupied && insert->baked)
        drawIcon (g, Icon::locked, textArea.removeFromRight (14).toFloat().reduced (1.0f), theme::accent);

    // Sidechain gesetzt: kleines "SC" rechts, damit man die Verbindung ohne Menü sieht.
    if (occupied && insert->sidechainSourceId.isNotEmpty())
    {
        g.setColour (theme::solo);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("SC", textArea.removeFromRight (16), juce::Justification::centred);
    }

    g.setColour (occupied && enabled && ! insert->baked ? theme::textPrimary : theme::textDisabled);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (occupied ? insert->name : juce::String ("+"), textArea, juce::Justification::centredLeft, true);
}

void InsertSlot::mouseUp (const juce::MouseEvent& event)
{
    if (! getLocalBounds().contains (event.getPosition()))
        return;

    if (event.mods.isPopupMenu())
    {
        if (insert.has_value())
            showContextMenu();

        return;
    }

    if (bakeMarkArea().expanded (2).contains (event.getPosition()))
    {
        onBakeMarkClicked();
        return;
    }

    if (insert.has_value())
    {
        if (onOpenRequested)
            onOpenRequested();
    }
    else if (onAddRequested)
    {
        onAddRequested();
    }
}

namespace
{
// Menü-IDs: 1, 2 Umgehen/Entfernen, 100+ Sidechain, 1000+ Presets.
constexpr int presetSave = 1000;
constexpr int presetLoadBase = 1100;
constexpr int presetDeleteBase = 2100;
constexpr int programBase = 3100;
constexpr int presetIdLimit = 1000; ///< höchstens so viele Presets oder Programme je Liste im Menü
} // namespace

void InsertSlot::addPresetMenu (juce::PopupMenu& menu,
                                std::vector<juce::String>& userPresets,
                                std::vector<juce::String>& programs)
{
    if (presetSource == nullptr || insert->baked)
        return;

    userPresets = presetSource->getUserPresets (insert->id);
    programs = presetSource->getFactoryPrograms (insert->id);

    juce::PopupMenu presets;
    presets.addItem (presetSave, TRANS ("Save current settings as preset..."));

    if (! userPresets.empty())
    {
        presets.addSectionHeader (TRANS ("Own presets"));
        juce::PopupMenu deleteMenu;

        for (std::size_t i = 0; i < userPresets.size() && i < presetIdLimit; ++i)
        {
            presets.addItem (presetLoadBase + static_cast<int> (i), userPresets[i]);
            deleteMenu.addItem (presetDeleteBase + static_cast<int> (i), userPresets[i]);
        }

        presets.addSubMenu (TRANS ("Delete preset"), deleteMenu);
    }

    if (! programs.empty())
    {
        juce::PopupMenu factory;

        for (std::size_t i = 0; i < programs.size() && i < presetIdLimit; ++i)
            factory.addItem (programBase + static_cast<int> (i),
                             programs[i].isNotEmpty()
                                 ? programs[i]
                                 : plugins::i18n::format (TRANS ("Program {number}"), { { "number", juce::String (i + 1) } }));

        presets.addSubMenu (TRANS ("Factory presets of the plugin"), factory);
    }

    menu.addSeparator();
    menu.addSubMenu (TRANS ("Presets"), presets);
}

void InsertSlot::handlePresetChoice (int result, const std::vector<juce::String>& userPresets)
{
    if (presetSource == nullptr)
        return;

    const auto id = insert->id;

    if (result == presetSave)
        askForPresetName();
    else if (result >= programBase)
        report (presetSource->loadFactoryProgram (id, result - programBase));
    else if (result >= presetDeleteBase)
        report (presetSource->deleteUserPreset (id, userPresets[static_cast<std::size_t> (result - presetDeleteBase)]));
    else if (result >= presetLoadBase)
        report (presetSource->loadUserPreset (id, userPresets[static_cast<std::size_t> (result - presetLoadBase)]));
}

void InsertSlot::askForPresetName()
{
    auto window = std::make_shared<juce::AlertWindow> (
        TRANS ("Save preset"),
        plugins::i18n::format (TRANS ("Name of the preset for {name}:"), { { "name", insert->name } }),
        juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", {});
    window->addButton (TRANS ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create (
                                 [safeThis = juce::Component::SafePointer<InsertSlot> (this), window] (int result)
                                 {
                                     if (safeThis == nullptr || result != 1 || ! safeThis->insert.has_value() ||
                                         safeThis->presetSource == nullptr)
                                         return;

                                     safeThis->report (safeThis->presetSource->saveUserPreset (
                                         safeThis->insert->id, window->getTextEditorContents ("name")));
                                 }));
}

void InsertSlot::report (const core::Result<void>& result) const
{
    if (! result && onError)
        onError (services::errorText (result.error()));
}

void InsertSlot::showContextMenu()
{
    constexpr int sidechainNone = 100;
    constexpr int sidechainBase = 101;

    juce::PopupMenu menu;
    menu.addItem (1, insert->enabled ? TRANS ("Bypass") : TRANS ("Activate"));
    menu.addItem (2, TRANS ("Remove"));

    // Sidechain-Quelle: nur bei Plugins mit Sidechain-Eingang.
    std::vector<services::SidechainSource> sources;

    if (insert->canSidechain && getSidechainSources)
    {
        sources = getSidechainSources();
        juce::PopupMenu sidechain;
        sidechain.addItem (sidechainNone, TRANS ("None"), true, insert->sidechainSourceId.isEmpty());

        for (std::size_t i = 0; i < sources.size(); ++i)
            sidechain.addItem (sidechainBase + static_cast<int> (i),
                               sources[i].name,
                               true,
                               sources[i].trackId == insert->sidechainSourceId);

        menu.addSeparator();
        menu.addSubMenu (TRANS ("Sidechain source"), sidechain);
    }

    std::vector<juce::String> userPresets;
    std::vector<juce::String> programs;
    addPresetMenu (menu, userPresets, programs);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<InsertSlot> (this), sources, userPresets] (int result)
                        {
                            if (safeThis == nullptr || ! safeThis->insert.has_value())
                                return;

                            if (result >= presetSave)
                            {
                                safeThis->handlePresetChoice (result, userPresets);
                                return;
                            }

                            if (result >= sidechainNone && safeThis->onSidechainChosen)
                            {
                                const auto index = static_cast<std::size_t> (result - sidechainBase);
                                safeThis->onSidechainChosen (result == sidechainNone || index >= sources.size()
                                                                 ? juce::String()
                                                                 : sources[index].trackId);
                                return;
                            }

                            if (result == 1 && safeThis->onEnabledChanged)
                                safeThis->onEnabledChanged (! safeThis->insert->enabled);
                            else if (result == 2 && safeThis->onRemoveRequested)
                                safeThis->onRemoveRequested();
                        });
}

} // namespace fk::ui
