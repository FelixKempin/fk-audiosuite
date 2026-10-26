#include "fk/ui/InstrumentBrowser.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

namespace fk::ui
{

namespace
{
constexpr int starWidth = 24;
constexpr int allTagsId = 1;

juce::String kindName (services::LibraryItem::Kind kind)
{
    switch (kind)
    {
        case services::LibraryItem::Kind::plugin:
            return TRANS ("Instrument");
        case services::LibraryItem::Kind::sfz:
            return TRANS ("Library");
        case services::LibraryItem::Kind::preset:
            return TRANS ("Preset");
    }

    return {};
}
} // namespace

InstrumentBrowser::InstrumentBrowser (services::LibraryService& libraryService)
    : library (libraryService)
    , favouritesButton (Icon::star, TRANS ("Show favorites only"))
    , newTrackButton (Icon::addInstrumentTrack, TRANS ("New track with the selected entry (or double-click)"), TRANS ("New Track"))
    , contentButton (Icon::exportFile, TRANS ("Content Manager: download free libraries"), TRANS ("Libraries"))
{
    search.setTextToShowWhenEmpty (TRANS ("Search: name, vendor, tag (e.g. piano, strings, favorite)"), theme::textSecondary);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    tagBox.setTooltip (TRANS ("Filter by tag"));
    tagBox.onChange = [this] { refresh(); };
    addAndMakeVisible (tagBox);

    favouritesButton.setClickingTogglesState (true);
    favouritesButton.onClick = [this] { refresh(); };
    addAndMakeVisible (favouritesButton);

    newTrackButton.onClick = [this] { createTrackForRow (list.getSelectedRow()); };
    addAndMakeVisible (newTrackButton);

    contentButton.onClick = [this]
    {
        if (onOpenContentManager)
            onOpenContentManager();
    };
    addAndMakeVisible (contentButton);

    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    list.setTooltip (TRANS ("Double-click: new track. Star: favorite. Right-click: tags."));
    addAndMakeVisible (list);

    emptyHint.setText (TRANS ("Nothing found. The Content Manager downloads libraries, your own presets are made by right-clicking an insert > Presets."),
                       juce::dontSendNotification);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, theme::textSecondary);
    addChildComponent (emptyHint);

    rebuildTags();
    refresh();
}

InstrumentBrowser::~InstrumentBrowser()
{
    list.setModel (nullptr);
}

void InstrumentBrowser::rebuildTags()
{
    const auto current = tagBox.getText();
    tagBox.clear (juce::dontSendNotification);
    tagBox.addItem (TRANS ("All tags"), allTagsId);
    const auto tags = library.getAllTags();

    for (int i = 0; i < tags.size(); ++i)
        tagBox.addItem (tags[i], allTagsId + 1 + i);

    const auto index = tags.indexOf (current);
    tagBox.setSelectedId (index >= 0 ? allTagsId + 1 + index : allTagsId, juce::dontSendNotification);
}

void InstrumentBrowser::refresh()
{
    const auto tag = tagBox.getSelectedId() > allTagsId ? tagBox.getText() : juce::String();
    items = library.search (search.getText(), tag, favouritesButton.getToggleState());
    list.updateContent();
    list.repaint();
    emptyHint.setVisible (items.empty());
}

void InstrumentBrowser::visibilityChanged()
{
    if (isVisible())
    {
        rebuildTags();
        refresh();
    }
}

void InstrumentBrowser::setSearchText (const juce::String& text)
{
    search.setText (text, false);
    refresh();
}

void InstrumentBrowser::setTagFilter (const juce::String& tag)
{
    rebuildTags();

    for (int i = 0; i < tagBox.getNumItems(); ++i)
        if (tagBox.getItemText (i) == tag)
            tagBox.setSelectedItemIndex (i, juce::dontSendNotification);

    refresh();
}

void InstrumentBrowser::setFavouritesOnly (bool only)
{
    favouritesButton.setToggleState (only, juce::dontSendNotification);
    refresh();
}

int InstrumentBrowser::getNumRows()
{
    return static_cast<int> (items.size());
}

void InstrumentBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (items.size()))
        return;

    const auto& item = items[static_cast<std::size_t> (row)];

    if (selected)
        g.fillAll (theme::accentSoft);

    g.setColour (item.favourite ? theme::solo : theme::textDisabled);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (juce::String::fromUTF8 (item.favourite ? "★" : "☆"), 0, 0, starWidth, height, juce::Justification::centred);

    auto area = juce::Rectangle<int> (starWidth + 4, 0, width - starWidth - 8, height);
    g.setColour (theme::textPrimary);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (item.name, area.removeFromLeft (area.getWidth() * 2 / 5), juce::Justification::centredLeft, true);

    g.setColour (theme::textSecondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{kind} · {source}")), { { "kind", kindName (item.kind) }, { "source", item.source } }), area.removeFromLeft (area.getWidth() / 2),
                juce::Justification::centredLeft, true);
    g.drawText (item.tags.joinIntoString (", "), area, juce::Justification::centredLeft, true);
}

void InstrumentBrowser::listBoxItemClicked (int row, const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        showItemMenu (row);
    else if (event.x < starWidth)
        toggleFavourite (row);
}

void InstrumentBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    createTrackForRow (row);
}

void InstrumentBrowser::createTrackForRow (int row)
{
    if (row < 0 || row >= static_cast<int> (items.size()))
    {
        if (onError)
            onError (TRANS ("Select an entry in the list first."));

        return;
    }

    if (const auto created = library.createTrack (items[static_cast<std::size_t> (row)].key); ! created && onError)
        onError (services::errorText (created.error()));
}

void InstrumentBrowser::toggleFavourite (int row)
{
    if (row < 0 || row >= static_cast<int> (items.size()))
        return;

    const auto& item = items[static_cast<std::size_t> (row)];
    report (library.setFavourite (item.key, ! item.favourite));
    refresh();
}

void InstrumentBrowser::showItemMenu (int row)
{
    if (row < 0 || row >= static_cast<int> (items.size()))
        return;

    const auto item = items[static_cast<std::size_t> (row)];
    juce::PopupMenu menu;
    menu.addItem (1, TRANS ("New track with this"));
    menu.addItem (2, item.favourite ? TRANS ("Remove favorite") : TRANS ("Mark as favorite"));
    menu.addItem (3, TRANS ("Add tag..."));

    juce::PopupMenu removeTags;

    for (int i = 0; i < item.tags.size(); ++i)
        removeTags.addItem (100 + i, item.tags[i]);

    menu.addSubMenu (TRANS ("Remove own tag"), removeTags);

    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [safeThis = juce::Component::SafePointer<InstrumentBrowser> (this), row, item] (int chosen)
                        {
                            if (safeThis == nullptr || chosen <= 0)
                                return;

                            if (chosen == 1)
                                safeThis->createTrackForRow (row);
                            else if (chosen == 2)
                                safeThis->toggleFavourite (row);
                            else if (chosen == 3)
                                safeThis->askForTag (row);
                            else if (chosen >= 100)
                            {
                                safeThis->report (safeThis->library.removeTag (item.key, item.tags[chosen - 100]));
                                safeThis->rebuildTags();
                                safeThis->refresh();
                            }
                        });
}

void InstrumentBrowser::askForTag (int row)
{
    const auto key = items[static_cast<std::size_t> (row)].key;
    auto window = std::make_shared<juce::AlertWindow> (TRANS ("Tag"), TRANS ("New tag for this entry:"),
                                                       juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("tag", {});
    window->addButton (TRANS ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create (
                                 [safeThis = juce::Component::SafePointer<InstrumentBrowser> (this), window, key] (int result)
                                 {
                                     if (safeThis == nullptr || result != 1)
                                         return;

                                     safeThis->report (safeThis->library.addTag (key, window->getTextEditorContents ("tag")));
                                     safeThis->rebuildTags();
                                     safeThis->refresh();
                                 }));
}

void InstrumentBrowser::report (const core::Result<void>& result)
{
    if (! result && onError)
        onError (services::errorText (result.error()));
}

void InstrumentBrowser::resized()
{
    auto area = getLocalBounds().reduced (theme::spacingMedium);
    auto top = area.removeFromTop (28);
    contentButton.setBounds (top.removeFromRight (130));
    top.removeFromRight (theme::spacingSmall);
    newTrackButton.setBounds (top.removeFromRight (120));
    top.removeFromRight (theme::spacingSmall);
    favouritesButton.setBounds (top.removeFromRight (28));
    top.removeFromRight (theme::spacingSmall);
    tagBox.setBounds (top.removeFromRight (160));
    top.removeFromRight (theme::spacingSmall);
    search.setBounds (top);
    area.removeFromTop (theme::spacingSmall);
    list.setBounds (area);
    emptyHint.setBounds (area.reduced (theme::spacingLarge));
}

void InstrumentBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
}

} // namespace fk::ui
