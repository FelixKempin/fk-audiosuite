#include "fk/ui/ListEditor.h"

#include "fk/plugins/i18n/Text.h"
#include "fk/services/ErrorText.h"
#include "fk/ui/Theme.h"

#include <algorithm>

namespace fk::ui
{

namespace
{
constexpr int toolbarHeight = 34;

/// Editierbare Zelle: Doppelklick öffnet das Textfeld, Enter übernimmt.
class CellLabel final : public juce::Label
{
public:
    int row = 0;
    int column = 0;
};
} // namespace

ListEditor::ListEditor (services::MidiClipService& midiClipService, services::TimelineService& timelineService)
    : midiClips (midiClipService)
    , timeline (timelineService)
    , deleteButton (Icon::trash, TRANS ("Delete selected notes (Del)"))
{
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    deleteButton.onClick = [this] { deleteSelected(); };
    addAndMakeVisible (deleteButton);

    auto& header = table.getHeader();
    header.addColumn (TRANS ("Position"), positionColumn, 120);
    header.addColumn (TRANS ("Note"), noteColumn, 100);
    header.addColumn (TRANS ("Length (beats)"), lengthColumn, 120);
    header.addColumn (TRANS ("Velocity"), velocityColumn, 90);
    table.setModel (this);
    table.setRowHeight (22);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::backgroundColourId, theme::backgroundPanel);
    table.setTooltip (TRANS ("Double-click a cell to edit it. Position as bar.beat.tick, note as C3 or 60, length in beats (0.25 = sixteenth)."));
    addAndMakeVisible (table);

    emptyHint.setText (TRANS ("List Editor: double-clicking a MIDI clip opens it here as well. One row per note, edit values by double-clicking."),
                       juce::dontSendNotification);
    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (emptyHint);

    midiClips.addListener (*this);
    reload();
}

ListEditor::~ListEditor()
{
    midiClips.removeListener (*this);
    table.setModel (nullptr);
}

void ListEditor::setClip (const juce::String& newClipId)
{
    clipId = newClipId;
    reload();
}

void ListEditor::reload()
{
    const auto clip = clipId.isNotEmpty() ? midiClips.getClip (clipId)
                                          : core::Result<services::MidiClipInfo> (core::fail (core::ErrorCode::notFound, "-"));
    const bool hasClip = clip.has_value();
    titleLabel.setVisible (hasClip);
    deleteButton.setVisible (hasClip);
    table.setVisible (hasClip);
    emptyHint.setVisible (! hasClip);
    notes.clear();

    if (hasClip)
    {
        clipStartSeconds = clip->startSeconds;
        clipStartBeats = timeline.secondsToBeats (clipStartSeconds);
        notes = clip->notes;
        std::ranges::stable_sort (notes, [] (const auto& a, const auto& b)
                                  { return a.startBeats != b.startBeats ? a.startBeats < b.startBeats : a.noteNumber < b.noteNumber; });
        const auto noteCount = static_cast<int> (notes.size());
        titleLabel.setText (plugins::i18n::format (TRANS (juce::String::fromUTF8 ("{name} · {notes}")),
                                                   { { "name", clip->name },
                                                     { "notes", plugins::i18n::plural (noteCount, "{count} note", "{count} notes") } }),
                            juce::dontSendNotification);
    }

    table.updateContent();
    table.repaint();
}

int ListEditor::getNumRows()
{
    return static_cast<int> (notes.size());
}

juce::String ListEditor::getCellText (int row, int column) const
{
    if (row < 0 || row >= static_cast<int> (notes.size()))
        return {};

    const auto& note = notes[static_cast<std::size_t> (row)];

    switch (column)
    {
        case positionColumn:
            return timeline.formatBarsBeats (timeline.beatsToSeconds (clipStartBeats + note.startBeats));
        case noteColumn:
            return juce::MidiMessage::getMidiNoteName (note.noteNumber, true, true, 3) + " (" + juce::String (note.noteNumber) + ")";
        case lengthColumn:
            return juce::String (note.lengthBeats, 3);
        case velocityColumn:
            return juce::String (note.velocity);
        default:
            return {};
    }
}

std::optional<int> ListEditor::parseNoteName (const juce::String& text)
{
    const auto trimmed = text.trim().upToFirstOccurrenceOf (" ", false, false);

    if (trimmed.containsOnly ("0123456789") && trimmed.isNotEmpty())
    {
        const int number = trimmed.getIntValue();
        return number >= 0 && number <= 127 ? std::optional (number) : std::nullopt;
    }

    static const juce::String letters ("CDEFGAB");
    static const int pitchClasses[] = { 0, 2, 4, 5, 7, 9, 11 };
    const auto upper = trimmed.toUpperCase();

    if (upper.isEmpty() || ! letters.containsChar (upper[0]))
        return std::nullopt;

    int pitch = pitchClasses[letters.indexOfChar (upper[0])];
    int position = 1;

    if (position < upper.length() && (upper[position] == '#'))
    {
        ++pitch;
        ++position;
    }
    else if (position < trimmed.length() && trimmed[position] == 'b')
    {
        --pitch;
        ++position;
    }

    const auto octaveText = trimmed.substring (position);

    if (octaveText.isEmpty() || ! octaveText.trimCharactersAtStart ("-").containsOnly ("0123456789"))
        return std::nullopt;

    const int number = (octaveText.getIntValue() + 2) * 12 + pitch; // Mitte C = C3 = 60
    return number >= 0 && number <= 127 ? std::optional (number) : std::nullopt;
}

bool ListEditor::editCell (int row, int column, const juce::String& text)
{
    if (row < 0 || row >= static_cast<int> (notes.size()))
        return false;

    auto note = notes[static_cast<std::size_t> (row)];

    switch (column)
    {
        case positionColumn:
        {
            const auto seconds = timeline.parseBarsBeats (text);

            if (! seconds)
                return false;

            note.startBeats = std::max (0.0, timeline.secondsToBeats (*seconds) - clipStartBeats);
            break;
        }
        case noteColumn:
        {
            const auto number = parseNoteName (text);

            if (! number)
                return false;

            note.noteNumber = *number;
            break;
        }
        case lengthColumn:
        {
            const auto beats = text.replaceCharacter (',', '.').getDoubleValue();

            if (beats <= 0.0)
                return false;

            note.lengthBeats = std::max (services::MidiClipService::minNoteLengthBeats, beats);
            break;
        }
        case velocityColumn:
        {
            if (! text.trim().containsOnly ("0123456789") || text.trim().isEmpty())
                return false;

            note.velocity = std::clamp (text.getIntValue(), 1, 127);
            break;
        }
        default:
            return false;
    }

    if (const auto result = midiClips.updateNotes (clipId, { note }, TRANS ("List Editor")); ! result)
    {
        reportError (result.error());
        return false;
    }

    reload();
    return true;
}

void ListEditor::selectRow (int row)
{
    table.selectRow (row);
}

void ListEditor::deleteSelected()
{
    std::vector<int> indices;

    for (int i = 0; i < table.getSelectedRows().size(); ++i)
        if (const int row = table.getSelectedRows()[i]; row >= 0 && row < static_cast<int> (notes.size()))
            indices.push_back (notes[static_cast<std::size_t> (row)].index);

    if (indices.empty())
        return;

    if (const auto result = midiClips.removeNotes (clipId, indices); ! result)
        reportError (result.error());

    table.deselectAllRows();
    reload();
}

void ListEditor::deleteKeyPressed (int)
{
    deleteSelected();
}

void ListEditor::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    if (selected)
        g.fillAll (theme::accentSoft);
    else if (row % 2 == 1)
        g.fillAll (theme::backgroundElevated.withAlpha (0.4f));
}

void ListEditor::paintCell (juce::Graphics&, int, int, int, int, bool)
{
    // Alle Zellen sind CellLabels (refreshComponentForCell); sie zeichnen sich selbst.
}

juce::Component* ListEditor::refreshComponentForCell (int row, int column, bool, juce::Component* existing)
{
    auto* label = dynamic_cast<CellLabel*> (existing);

    if (label == nullptr)
    {
        delete existing;
        label = new CellLabel(); // JUCE-Vertrag: die Tabelle besitzt die Zellkomponente
        label->setEditable (false, true, false);
        label->setColour (juce::Label::textColourId, theme::textPrimary);
        label->setInterceptsMouseClicks (true, true);
        label->onTextChange = [this, label]
        {
            if (! editCell (label->row, label->column, label->getText()))
                label->setText (getCellText (label->row, label->column), juce::dontSendNotification);
        };
    }

    label->row = row;
    label->column = column;
    label->setText (getCellText (row, column), juce::dontSendNotification);
    return label;
}

void ListEditor::resized()
{
    auto area = getLocalBounds();
    emptyHint.setBounds (area.reduced (theme::spacingLarge));
    auto toolbar = area.removeFromTop (toolbarHeight).reduced (theme::spacingMedium, theme::spacingSmall);
    titleLabel.setBounds (toolbar.removeFromLeft (300));
    deleteButton.setBounds (toolbar.removeFromLeft (toolbar.getHeight()));
    table.setBounds (area);
}

void ListEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::backgroundPanel);
}

void ListEditor::midiClipChanged()
{
    reload();
}

void ListEditor::reportError (const core::Error& error)
{
    if (onError)
        onError (services::errorText (error));
}

} // namespace fk::ui
