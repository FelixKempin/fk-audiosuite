#pragma once

#include "fk/core/Result.h"
#include "fk/engine/EngineContext.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace fk::services
{

namespace te = tracktion;

/// Anwendungsfälle rund um das Projekt: anlegen, öffnen, speichern, Undo/Redo.
/// Besitzt das geöffnete Edit (Tracktions Projektmodell) und ist die einzige Stelle,
/// die es erzeugt oder zerstört. Alle anderen Services fragen hier nach dem Edit.
///
/// Ein Projekt ist ein Ordner: <Name>/<Name>.tracktionedit plus Audio/ für Aufnahmen und
/// Backups/ für Sicherungen (ARCHITECTURE.md §5). Nur vom Message-Thread aufrufen.
///
/// Sicherungen: Jedes Speichern legt die vorige Fassung als Backups/<Name> <Zeit>.tracktionedit ab
/// (die neuesten maxBackups bleiben). Die automatische Sicherung schreibt Backups/<Name>.autosave.tracktionedit,
/// ohne den Geändert-Status zu löschen. Schließen löscht sie; überlebt sie, ist die App abgestürzt.
class ProjectService final
{
public:
    /// Wird benachrichtigt, wenn ein anderes Edit geladen wurde (oder keines mehr).
    /// Empfänger müssen alle Zeiger auf das alte Edit verwerfen.
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void projectChanged() = 0;
    };

    explicit ProjectService (engine::EngineContext& engineContext);
    ~ProjectService();

    /// Legt <parentFolder>/<name>/ mit leerem Edit an und öffnet es.
    [[nodiscard]] core::Result<void> createProject (const juce::File& parentFolder, const juce::String& name);

    /// Legt <parentFolder>/<name>/ mit dem Inhalt einer Vorlage an und öffnet es.
    [[nodiscard]] core::Result<void> createProjectFromTemplate (const juce::File& parentFolder,
                                                                const juce::String& name,
                                                                const juce::File& templateFile);

    /// Schreibt das offene Projekt als Vorlage: Spuren, Plugins, Routing, Tempo, Einstellungen und
    /// MIDI-Clips, aber keine Audio-Clips (deren Dateien gehören zum Projektordner). Gebackene Spuren
    /// werden abgelehnt, weil ihre gerenderten Dateien fehlen würden.
    [[nodiscard]] core::Result<void> writeTemplate (const juce::File& templateFile);

    /// Öffnet eine vorhandene .tracktionedit-Datei.
    [[nodiscard]] core::Result<void> openProject (const juce::File& editFile);

    /// Speichert das offene Projekt an seinem Ort; die vorige Fassung wandert nach Backups/.
    [[nodiscard]] core::Result<void> saveProject();

    /// Speichert das Projekt als neuen Projektordner <parentFolder>/<name>/ und öffnet ihn.
    /// Audio/ wird kopiert, Verweise auf Dateien im alten Ordner zeigen danach in den neuen.
    /// Der alte Ordner bleibt unverändert (ungespeicherte Änderungen landen nur im neuen).
    [[nodiscard]] core::Result<void> saveProjectAs (const juce::File& parentFolder, const juce::String& name);

    /// Gibt es Änderungen seit dem letzten Speichern?
    [[nodiscard]] bool isModified() const;

    /// Schreibt die automatische Sicherung, falls es ungespeicherte Änderungen gibt.
    [[nodiscard]] core::Result<void> writeAutosave();

    /// Die automatische Sicherung zu einer Projektdatei, wenn es eine gibt und sie neuer ist als die Datei.
    [[nodiscard]] static std::optional<juce::File> findNewerAutosave (const juce::File& editFile);

    /// Alle Sicherungen eines Projekts, neueste zuerst (ohne die automatische Sicherung).
    [[nodiscard]] static std::vector<juce::File> getBackups (const juce::File& editFile);

    /// Stellt eine Sicherung (Backup oder automatische Sicherung) als Projektdatei wieder her und öffnet
    /// das Projekt. Die bisherige Projektdatei bleibt als Backup erhalten.
    [[nodiscard]] core::Result<void> restoreBackup (const juce::File& editFile, const juce::File& backupFile);

    /// Legt eine nicht gewollte automatische Sicherung zu den Backups, statt sie zu löschen.
    static void archiveAutosave (const juce::File& editFile);

    /// Zuletzt geöffnete Projektdateien, neueste zuerst; nur solche, die es noch gibt.
    [[nodiscard]] std::vector<juce::File> getRecentProjects() const;

    /// Wie viele Backups je Projekt bleiben.
    static constexpr int maxBackups = 10;

    void closeProject();

    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String getProjectName() const;
    [[nodiscard]] juce::File getProjectFolder() const;
    [[nodiscard]] juce::File getEditFile() const;

    /// Meldet eine Audiodatei im Projektordner beim Tracktion-Projekt an (neue Dateien sieht
    /// dessen Ordner-Scan erst nach einem Reload). Nötig für Takes; Aufnahmen melden sich selbst.
    void registerProjectFile (const juce::File& file);

    /// Das offene Edit oder nullptr. Zeiger nur kurz halten, nie über einen
    /// projectChanged()-Aufruf hinaus (→ LEARNING.md §4, Dangling).
    [[nodiscard]] te::Edit* getEdit() noexcept;
    [[nodiscard]] const te::Edit* getEdit() const noexcept;

    /// Beginnt einen neuen Undo-Schritt mit lesbarem Namen. Jeder Service ruft das am Anfang
    /// einer Nutzeraktion auf; sonst fasst Tracktion alle Änderungen zu einem Schritt zusammen.
    /// actionName ist ein Anzeigetext, also `TRANS ("Add track")` (CODING_GUIDELINES §15).
    void beginUndoStep (const juce::String& actionName);

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    /// Bearbeitungsverlauf: done = ausgeführte Schritte (ältester zuerst), undone = rückgängig
    /// gemachte, die sich wiederholen lassen (nächster zuerst). Leere Namen heißen "Änderung".
    struct UndoHistory
    {
        std::vector<juce::String> done;
        std::vector<juce::String> undone;
    };
    [[nodiscard]] UndoHistory getUndoHistory() const;

    /// Springt im Verlauf: Danach sind genau doneCount Schritte ausgeführt (0 = Projektanfang).
    void jumpToUndoStep (int doneCount);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void setEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& editFile);
    void notifyProjectChanged();
    void addToRecentProjects (const juce::File& editFile);

    engine::EngineContext& engineContext;
    void attachProject (te::Edit& newEdit, const juce::File& editFile);

    std::unique_ptr<te::Edit> edit;
    te::Project::Ptr project; ///< ordnerbasiertes Tracktion-Projekt = unser Projektordner (Takes, Aufnahmen)
    juce::File currentEditFile;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectService)
};

} // namespace fk::services
