#pragma once

#include "fk/services/AutomationService.h"
#include "fk/services/MarkerService.h"
#include "fk/services/ProjectSettingsService.h"
#include "fk/services/TimelineService.h"
#include "fk/services/TransportService.h"
#include "fk/ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include <vector>

namespace fk::ui
{

/// Transportleiste am unteren Fensterrand, in Gruppen: Navigation (Anfang, Rück-/Vorlauf, Marker),
/// Transport (Play, Stop, Record, Loop, Punch, Pre-/Post-Roll, Metronom, Vorzähler), Automation und
/// Anzeigen (Takte, Zeit in wählbarem Format, Locatoren, Tempo, Taktart, DSP-Last). Wird das Fenster
/// zu schmal, bricht die Leiste in eine zweite Reihe um (getRequiredHeight).
/// Zustand kommt per Listener, die Position per Timer (30 Hz), weil sie sich dauernd ändert.
class TransportBar final : public juce::Component,
                           private services::TransportService::Listener,
                           private services::TimelineService::Listener,
                           private services::AutomationService::Listener,
                           private juce::Timer
{
public:
    /// Alle Services der Leiste (CODING_GUIDELINES.md §9: mehr als vier Parameter → struct).
    struct Services
    {
        services::TransportService& transport;
        services::TimelineService& timeline;
        services::AutomationService& automation;
        services::MarkerService& markers;
        services::ProjectSettingsService& projectSettings;
    };

    explicit TransportBar (Services services);
    ~TransportBar() override;

    /// Höhe, die die Leiste bei dieser Breite braucht (eine oder mehrere Reihen).
    [[nodiscard]] int getRequiredHeight (int width) const;

    /// Aktionen, die auch Menü und Tastatur auslösen (ein Weg für alle).
    void stepBars (int direction);
    void jumpToLocator (bool left);
    void setLocatorToPlayhead (bool left);
    void togglePunch();

    /// Retrospektive Aufnahme auslösen (Button und Tastenbefehl); Ergebnis über onMessage.
    void captureRetrospective();

    /// Hinweis für den Benutzer (Projektfenster zeigt ihn), z. B. "nichts gespielt".
    std::function<void (const juce::String& message)> onMessage;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& event) override; ///< Klicks auf Zeit- und Locator-Anzeige

private:
    struct Item
    {
        juce::Component* component = nullptr;
        int width = 40;
        bool startsGroup = false;
    };

    void transportStateChanged() override;
    void timelineChanged() override;
    void automationChanged() override;
    void timerCallback() override;
    void updateButtons();
    void updateTimelineFields();
    void updateDspLoad();
    void applyTempoFromLabel();
    void applyTimeSignatureFromLabel();
    void showRollMenu (bool preRoll);
    void showTimeDisplayMenu();
    void showLocatorMenu (bool left);
    /// Rechnet die Reihen aus; cells (falls gesetzt) bekommt je Element ein Rechteck, zuletzt das der DSP-Last.
    int layoutItems (int width, std::vector<juce::Rectangle<int>>* cells) const;

    void styleDisplayLabel (juce::Label& label, float fontSize);

    services::TransportService& transport;
    services::TimelineService& timeline;
    services::AutomationService& automation;
    services::MarkerService& markers;
    services::ProjectSettingsService& projectSettings;

    IconButton returnToStartButton;
    IconButton rewindButton;
    IconButton forwardButton;
    IconButton previousMarkerButton;
    IconButton nextMarkerButton;
    IconButton addMarkerButton;
    IconButton playButton;
    IconButton stopButton;
    IconButton recordButton;
    IconButton retrospectiveButton;
    IconButton loopButton;
    IconButton punchButton;
    IconButton preRollButton;
    IconButton postRollButton;
    IconButton metronomeButton;
    IconButton automationReadButton;
    IconButton automationWriteButton;
    juce::ComboBox countInBox;
    juce::Label barsLabel;
    juce::Label timeLabel;
    juce::Label leftLocatorLabel;
    juce::Label rightLocatorLabel;
    juce::Label tempoLabel;
    juce::Label timeSignatureLabel;
    juce::Label dspLoadLabel;  ///< Audio-Last; rot mit Hinweis, wenn die Engine wegen Überlast stummschaltet
    int overloadHoldTicks = 0; ///< Überlast-Hinweis ein paar Sekunden stehen lassen
    std::vector<Item> items;   ///< Reihenfolge und Breiten der Leiste, einmal im Konstruktor gebaut

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace fk::ui
