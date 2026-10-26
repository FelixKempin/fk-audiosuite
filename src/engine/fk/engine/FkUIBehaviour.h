#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace fk::engine
{

namespace te = tracktion;

/// Antworten der Oberfläche auf Fragen der Engine. Tracktion fragt zum Beispiel, welches Edit
/// gerade im Vordergrund ist, um MIDI-Controller-Bewegungen (MIDI Learn, Zuordnungen) dorthin
/// zu leiten. Wir haben immer höchstens ein Edit; der ProjectService trägt es hier ein.
class FkUIBehaviour final : public te::UIBehaviour
{
public:
    FkUIBehaviour() = default;

    /// Das offene Edit oder nullptr. Nur der ProjectService ruft das.
    void setFocusedEdit (te::Edit* edit) noexcept;

    te::Edit* getCurrentlyFocusedEdit() override;
    te::Edit* getLastFocusedEdit() override;
    juce::Array<te::Edit*> getAllOpenEdits() override;

    /// Tracktion rendert Track-Freeze (Bake, ADR 0009) über diese Methode und wartet, bis sie
    /// zurückkehrt. Mit Fenstern läuft die Aufgabe in einem Hintergrund-Thread, solange ein
    /// modales Fenster Fortschritt und Abbrechen zeigt; ohne (Tests, Kommandozeile) direkt.
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& task) override;

    /// Fortschrittsfenster zeigen (die App schaltet das ein; Tests laufen ohne).
    void setProgressWindowsEnabled (bool shouldShow) noexcept;

    /// Die letzte Aufgabe wurde über "Abbrechen" beendet; ihr Ergebnis ist unvollständig.
    [[nodiscard]] bool wasLastTaskCancelled() const noexcept;

private:
    te::Edit* focusedEdit = nullptr;
    bool progressWindowsEnabled = false;
    bool lastTaskCancelled = false;
};

} // namespace fk::engine
