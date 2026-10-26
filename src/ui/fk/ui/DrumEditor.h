#pragma once

#include "fk/services/DrumMapService.h"
#include "fk/services/MidiClipService.h"
#include "fk/services/NotePreviewService.h"
#include "fk/services/NoteValue.h"
#include "fk/services/SamplerService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TrackService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace fk::ui
{

/// Eine Zeile im Drum-Editor: ein Instrument der Drum-Map (oder eine Note, die die Map nicht kennt).
struct DrumRow
{
    int note = 36;
    juce::String name;
    juce::String group;
    bool inMap = true;
    juce::String sampleName; ///< Sampler-Spur: Sample auf dieser Note (leer = keins)
};

/// Raster des Drum-Editors: je Instrument eine Zeile, Schläge als Rauten (die Länge zählt bei
/// Schlagzeug nicht). Klick ins Leere setzt einen Schlag, Klick auf eine Raute löscht ihn,
/// senkrecht ziehen ändert den Anschlag.
class DrumGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int note, double beats)> onAddHit;
    std::function<void (int noteIndex)> onRemoveHit;
    std::function<void (int noteIndex, int velocity)> onVelocityChanged;
    std::function<void (int note, int velocity)> onPreview;

    static constexpr int rowHeight = 22;

    DrumGrid();

    void setRows (std::vector<DrumRow> newRows);
    void setNotes (std::vector<services::EditableNote> newNotes);
    void setLengthBeats (double beats);
    void setGridBeats (double beats);
    void setPixelsPerBeat (double pixels);
    void setPlayheadBeats (std::optional<double> beats);
    void setNewVelocity (int velocity) noexcept { newVelocity = velocity; }

    [[nodiscard]] double getPixelsPerBeat() const noexcept { return pixelsPerBeat; }
    [[nodiscard]] int xForBeats (double beats) const;
    [[nodiscard]] int yForRow (int row) const { return row * rowHeight + rowHeight / 2; }
    [[nodiscard]] int rowForNote (int note) const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    [[nodiscard]] std::optional<int> hitAt (juce::Point<int> position) const;
    [[nodiscard]] double snapped (double beats) const;
    void updateSize();

    std::vector<DrumRow> rows;
    std::vector<services::EditableNote> notes;
    double lengthBeats = 4.0;
    double gridBeats = 0.25;
    double pixelsPerBeat = 80.0;
    std::optional<double> playheadBeats;
    int newVelocity = 100;

    struct VelocityDrag
    {
        int noteIndex = 0;
        int startVelocity = 100;
        int currentVelocity = 100;
        bool moved = false;
    };
    std::optional<VelocityDrag> drag;
};

/// Namen links neben dem Raster; Klick spielt das Instrument an.
class DrumNames final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int note)> onPreview;

    void setRows (std::vector<DrumRow> newRows);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;

    static constexpr int width = 170;

private:
    std::vector<DrumRow> rows;
};

/// Drum-Editor (Cubase: Schlagzeug-Editor) in der unteren Zone: ein MIDI-Clip, gezeigt nach der
/// Drum-Map seiner Spur. Wechsel der Map ordnet sie der Spur zu.
class DrumEditor final : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         public juce::DragAndDropTarget, // Samples aus dem Medien-Browser
                         private services::MidiClipService::Listener,
                         private juce::Timer
{
public:
    std::function<void (const juce::String& message)> onError;

    struct Services
    {
        services::MidiClipService& midiClips;
        services::NotePreviewService& preview;
        services::TransportService& transport;
        services::TimelineService& timeline;
        services::DrumMapService& drumMaps;
        services::TrackService& tracks;
        services::SamplerService& sampler;
    };

    explicit DrumEditor (Services services);
    ~DrumEditor() override;

    void setClip (const juce::String& clipId);
    [[nodiscard]] juce::String getClipId() const { return clipId; }

    /// Für Tests: angezeigte Zeilen, Raster, nur benutzte Instrumente zeigen.
    [[nodiscard]] const std::vector<DrumRow>& getRows() const noexcept { return rows; }
    [[nodiscard]] DrumGrid& getGrid() noexcept { return grid; }
    void setShowUsedOnly (bool usedOnly);

    /// Audiodatei auf eine Zeile ablegen: Sample für diese Note (Sampler-Spuren). y in Editor-Koordinaten.
    void dropSample (const juce::File& file, int y);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    void midiClipChanged() override;
    void timerCallback() override;
    void reload();
    void rebuildMapChoices();
    void reportError (const core::Error& error);
    [[nodiscard]] double currentGridBeats() const;

    services::MidiClipService& midiClips;
    services::NotePreviewService& preview;
    services::TransportService& transport;
    services::TimelineService& timeline;
    services::DrumMapService& drumMaps;
    services::TrackService& tracks;
    services::SamplerService& sampler;
    juce::String clipId;
    juce::String trackId;
    double clipStartBeats = 0.0;
    double clipLengthBeats = 4.0;
    std::vector<DrumRow> rows;
    std::vector<services::DrumMap> mapChoices;
    std::vector<services::NoteValue> noteValues = services::NoteValue::all();

    juce::Label titleLabel;
    juce::ComboBox mapBox;
    IconButton usedOnlyButton;
    juce::ComboBox gridBox;
    juce::ComboBox velocityBox;
    IconButton quantiseButton;
    IconButton previewButton;
    juce::Label emptyHint;

    class ScrollReportingViewport final : public juce::Viewport
    {
    public:
        std::function<void (int y)> onScrolled;
        void visibleAreaChanged (const juce::Rectangle<int>& area) override
        {
            if (onScrolled)
                onScrolled (area.getY());
        }
    };

    ScrollReportingViewport viewport;
    DrumGrid grid;
    juce::Component namesHolder; ///< schneidet die Namen auf die sichtbare Höhe zu
    DrumNames names;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEditor)
};

} // namespace fk::ui
