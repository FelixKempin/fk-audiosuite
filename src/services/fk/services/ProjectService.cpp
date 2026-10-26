#include "fk/services/ProjectService.h"

#include "fk/services/BakeLock.h"

#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto editFileExtension = ".tracktionedit";
constexpr auto audioFolderName = "Audio";
constexpr auto backupFolderName = "Backups";
constexpr auto autosaveSuffix = ".autosave";
constexpr auto recentProjectsItem = "fkEdits"; // eigener Schlüssel; "recentProjects" allein nutzt Tracktion selbst
constexpr std::size_t maxRecentProjects = 10;

juce::File backupFolderFor (const juce::File& editFile)
{
    return editFile.getSiblingFile (backupFolderName);
}

juce::File autosaveFileFor (const juce::File& editFile)
{
    return backupFolderFor (editFile).getChildFile (editFile.getFileNameWithoutExtension() + autosaveSuffix +
                                                    editFileExtension);
}

/// Neuer Backup-Name mit Zeitstempel, z. B. "Song 2026-10-12 14-03-55.tracktionedit".
juce::File newBackupFileFor (const juce::File& editFile, const juce::String& label = {})
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    const auto name = editFile.getFileNameWithoutExtension() + " " + (label.isEmpty() ? stamp : label + " " + stamp);
    return backupFolderFor (editFile).getChildFile (name + editFileExtension).getNonexistentSibling (false);
}

void pruneBackups (const juce::File& editFile)
{
    const auto backups = ProjectService::getBackups (editFile);

    for (std::size_t i = ProjectService::maxBackups; i < backups.size(); ++i)
        backups[i].deleteFile();
}

/// Schreibt den Projektzustand über eine Temp-Datei: Ein Absturz mitten im Schreiben
/// hinterlässt nie eine halbe Datei (→ LEARNING.md, Schreiben und Umbenennen).
bool writeStateAtomically (const juce::ValueTree& state, const juce::File& target)
{
    const auto xml = state.createXml();

    if (xml == nullptr)
        return false;

    juce::TemporaryFile temp (target);
    return xml->writeTo (temp.getFile()) && temp.overwriteTargetFileWithTemporary();
}

/// Lässt alle Dateiverweise ("source") im Zustand auf den neuen Projektordner zeigen: Dateien aus dem
/// alten Ordner relativ in den neuen, alles andere absolut. Projekt-IDs werden dabei zu Pfaden.
void relinkFileReferences (juce::ValueTree tree,
                           te::Edit& edit,
                           const juce::File& oldFolder,
                           const juce::File& newFolder)
{
    if (tree.hasProperty (te::IDs::source))
    {
        const auto file = te::SourceFileReference::findFileFromString (edit, tree[te::IDs::source].toString());

        if (file.existsAsFile())
        {
            const auto target =
                file.isAChildOf (oldFolder) ? newFolder.getChildFile (file.getRelativePathFrom (oldFolder)) : file;
            tree.setProperty (te::IDs::source,
                              target.isAChildOf (newFolder) ? target.getRelativePathFrom (newFolder)
                                                            : target.getFullPathName(),
                              nullptr);
        }
    }

    for (auto child : tree)
        relinkFileReferences (child, edit, oldFolder, newFolder);
}

void removeAudioClips (juce::ValueTree tree)
{
    for (int i = tree.getNumChildren(); --i >= 0;)
    {
        if (auto child = tree.getChild (i); child.hasType (te::IDs::AUDIOCLIP))
            tree.removeChild (i, nullptr);
        else
            removeAudioClips (child);
    }
}
} // namespace

ProjectService::ProjectService (engine::EngineContext& context)
    : engineContext (context)
{
}

ProjectService::~ProjectService()
{
    // Das Edit muss vor der Engine sterben; die Engine gehört dem Aufrufer und lebt länger.
    closeProject();
}

core::Result<void> ProjectService::createProject (const juce::File& parentFolder, const juce::String& name)
{
    if (name.trim().isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project name must not be empty.").toStdString());

    const auto projectFolder = parentFolder.getChildFile (name);

    if (projectFolder.exists())
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("A folder \"{path}\" already exists."), { { "path", projectFolder.getFullPathName() } }).toStdString());

    if (! projectFolder.getChildFile (audioFolderName).createDirectory())
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not create the project folder: {path}"), { { "path", projectFolder.getFullPathName() } }).toStdString());

    const auto editFile = projectFolder.getChildFile (name + editFileExtension);
    auto newEdit = te::createEmptyEdit (engineContext.getEngine(), editFile);

    // Tracktion legt in einem "leeren" Edit bereits eine Audiospur an. Ein neues Projekt
    // soll bei uns wirklich leer sein; der Nutzer fügt Spuren selbst hinzu.
    for (auto* track : te::getAudioTracks (*newEdit))
        newEdit->deleteTrack (track);

    newEdit->getUndoManager().clearUndoHistory();

    setEdit (std::move (newEdit), editFile);

    return saveProject();
}

core::Result<void> ProjectService::createProjectFromTemplate (const juce::File& parentFolder,
                                                              const juce::String& name,
                                                              const juce::File& templateFile)
{
    if (! templateFile.existsAsFile())
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("Template not found: {path}"), { { "path", templateFile.getFullPathName() } }).toStdString());

    if (name.trim().isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project name must not be empty.").toStdString());

    const auto projectFolder = parentFolder.getChildFile (name);

    if (projectFolder.exists())
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("A folder \"{path}\" already exists."), { { "path", projectFolder.getFullPathName() } }).toStdString());

    const auto editFile = projectFolder.getChildFile (name + editFileExtension);

    if (! projectFolder.getChildFile (audioFolderName).createDirectory() || ! templateFile.copyFileTo (editFile))
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not create the project: {path}"), { { "path", projectFolder.getFullPathName() } }).toStdString());

    return openProject (editFile);
}

core::Result<void> ProjectService::writeTemplate (const juce::File& templateFile)
{
    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    for (const auto* track : te::getAudioTracks (*edit))
        if (bakeLock::isBaked (*track))
            return core::fail (
                core::ErrorCode::invalidArgument,
                plugins::i18n::format (
                    TRANS ("Track \"{name}\" is baked. Unbake it first, then save as a template."), { { "name", track->getName() } })
                    .toStdString());

    if (! templateFile.getParentDirectory().createDirectory())
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Template folder is missing: {path}"), { { "path", templateFile.getParentDirectory().getFullPathName() } }).toStdString());

    edit->getParameterControlMappings().saveToEdit();
    edit->flushState();

    auto state = edit->state.createCopy();
    state.removeProperty (te::IDs::projectID, nullptr);
    removeAudioClips (state);

    if (! writeStateAtomically (state, templateFile))
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not write the template: {path}"), { { "path", templateFile.getFullPathName() } }).toStdString());

    return {};
}

core::Result<void> ProjectService::openProject (const juce::File& editFile)
{
    if (! editFile.existsAsFile())
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("Project file not found: {path}"), { { "path", editFile.getFullPathName() } }).toStdString());

    auto loadedEdit = te::loadEditFromFile (engineContext.getEngine(), editFile);

    if (loadedEdit == nullptr)
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not read the project file: {path}"), { { "path", editFile.getFullPathName() } }).toStdString());

    setEdit (std::move (loadedEdit), editFile);
    return {};
}

core::Result<void> ProjectService::saveProject()
{
    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    // Vorige Fassung sichern, aber nur bei echten Änderungen: Sonst verdrängen gleiche Kopien die alten Backups.
    if (edit->hasChangedSinceSaved() && currentEditFile.existsAsFile())
    {
        const auto backup = newBackupFileFor (currentEditFile);

        if (! backup.getParentDirectory().createDirectory() || ! currentEditFile.copyFileTo (backup))
            return core::fail (core::ErrorCode::ioError,
                               plugins::i18n::format (TRANS ("Could not create the backup: {path}"), { { "path", backup.getFullPathName() } }).toStdString());
    }

    // Parameter: keine Tracktion-Warndialoge, auch speichern wenn unverändert, nicht nachfragen.
    // Tracktion schreibt erst eine Temp-Datei und benennt sie dann um.
    te::EditFileOperations (*edit).save (false, true, false);

    if (! currentEditFile.existsAsFile() || edit->hasChangedSinceSaved())
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not save the project: {path}"), { { "path", currentEditFile.getFullPathName() } }).toStdString());

    autosaveFileFor (currentEditFile).deleteFile();
    pruneBackups (currentEditFile);
    return {};
}

core::Result<void> ProjectService::saveProjectAs (const juce::File& parentFolder, const juce::String& name)
{
    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    if (name.trim().isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The project name must not be empty.").toStdString());

    const auto newFolder = parentFolder.getChildFile (name);

    if (newFolder.exists())
        return core::fail (core::ErrorCode::invalidArgument,
                           plugins::i18n::format (TRANS ("A folder \"{path}\" already exists."), { { "path", newFolder.getFullPathName() } }).toStdString());

    const auto oldFolder = getProjectFolder();
    const auto oldAudio = oldFolder.getChildFile (audioFolderName);
    const auto newAudio = newFolder.getChildFile (audioFolderName);

    if (! newAudio.createDirectory() || (oldAudio.isDirectory() && ! oldAudio.copyDirectoryTo (newAudio)))
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not copy the audio files to: {path}"), { { "path", newAudio.getFullPathName() } }).toStdString());

    // Wie Tracktions save(): MIDI-Zuordnungen in den Zustand schreiben, dann den Zustand vollständig machen.
    edit->getParameterControlMappings().saveToEdit();
    edit->flushState();

    auto state = edit->state.createCopy();
    state.removeProperty (te::IDs::projectID, nullptr); // gehört zum alten Projekt; der neue Ordner vergibt eine eigene
    relinkFileReferences (state, *edit, oldFolder, newFolder);

    const auto newEditFile = newFolder.getChildFile (name + editFileExtension);

    if (! writeStateAtomically (state, newEditFile))
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not write the project: {path}"), { { "path", newEditFile.getFullPathName() } }).toStdString());

    return openProject (newEditFile);
}

bool ProjectService::isModified() const
{
    return edit != nullptr && edit->hasChangedSinceSaved();
}

core::Result<void> ProjectService::writeAutosave()
{
    // isSaveInhibited: Tracktion verändert das Edit gerade vorübergehend (z. B. beim Rendern).
    if (edit == nullptr || ! edit->hasChangedSinceSaved() || edit->isSaveInhibited())
        return {};

    const auto file = autosaveFileFor (currentEditFile);

    if (! file.getParentDirectory().createDirectory())
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Backup folder is missing: {path}"), { { "path", file.getParentDirectory().getFullPathName() } }).toStdString());

    edit->getParameterControlMappings().saveToEdit();
    edit->flushState();

    if (! writeStateAtomically (edit->state, file))
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Automatic backup failed: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    return {};
}

std::optional<juce::File> ProjectService::findNewerAutosave (const juce::File& editFile)
{
    const auto autosave = autosaveFileFor (editFile);

    if (autosave.existsAsFile() && autosave.getLastModificationTime() > editFile.getLastModificationTime())
        return autosave;

    return std::nullopt;
}

std::vector<juce::File> ProjectService::getBackups (const juce::File& editFile)
{
    const auto autosave = autosaveFileFor (editFile);
    std::vector<juce::File> backups;

    for (const auto& file : backupFolderFor (editFile).findChildFiles (
             juce::File::findFiles, false, juce::String ("*") + editFileExtension))
        if (file != autosave)
            backups.push_back (file);

    std::ranges::sort (backups,
                       [] (const juce::File& a, const juce::File& b)
                       { return a.getLastModificationTime() > b.getLastModificationTime(); });
    return backups;
}

core::Result<void> ProjectService::restoreBackup (const juce::File& editFile, const juce::File& backupFile)
{
    // Erst lesen: Schließen löscht die automatische Sicherung des offenen Projekts.
    juce::MemoryBlock content;

    if (! backupFile.loadFileAsData (content))
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("Backup is not readable: {path}"), { { "path", backupFile.getFullPathName() } }).toStdString());

    if (currentEditFile == editFile)
        closeProject();

    if (editFile.existsAsFile())
    {
        const auto previous = newBackupFileFor (editFile, "vor Wiederherstellung");

        if (! previous.getParentDirectory().createDirectory() || ! editFile.copyFileTo (previous))
            return core::fail (
                core::ErrorCode::ioError,
                plugins::i18n::format (TRANS ("Could not back up the previous version: {path}"), { { "path", previous.getFullPathName() } }).toStdString());
    }

    if (! editFile.replaceWithData (content.getData(), content.getSize()))
        return core::fail (
            core::ErrorCode::ioError,
            plugins::i18n::format (TRANS ("Could not write the project file: {path}"), { { "path", editFile.getFullPathName() } }).toStdString());

    if (backupFile == autosaveFileFor (editFile))
        backupFile.deleteFile();

    pruneBackups (editFile);
    return openProject (editFile);
}

void ProjectService::archiveAutosave (const juce::File& editFile)
{
    const auto autosave = autosaveFileFor (editFile);

    if (autosave.existsAsFile())
        autosave.moveFileTo (newBackupFileFor (editFile, "Autosave"));

    pruneBackups (editFile);
}

std::vector<juce::File> ProjectService::getRecentProjects() const
{
    juce::StringArray paths;
    paths.addLines (engineContext.getEngine()
                        .getPropertyStorage()
                        .getPropertyItem (te::SettingID::recentProjects, recentProjectsItem)
                        .toString());

    std::vector<juce::File> files;

    for (const auto& path : paths)
        if (juce::File file (path); path.isNotEmpty() && file.existsAsFile())
            files.push_back (file);

    return files;
}

void ProjectService::addToRecentProjects (const juce::File& editFile)
{
    juce::StringArray paths;
    paths.add (editFile.getFullPathName());

    for (const auto& file : getRecentProjects())
        if (file != editFile && static_cast<std::size_t> (paths.size()) < maxRecentProjects)
            paths.add (file.getFullPathName());

    auto& storage = engineContext.getEngine().getPropertyStorage();
    storage.setPropertyItem (te::SettingID::recentProjects, recentProjectsItem, paths.joinIntoString ("\n"));
    storage.flushSettingsToDisk();
}

void ProjectService::closeProject()
{
    if (edit == nullptr)
        return;

    // Reihenfolge ist entscheidend: Erst das Edit aus dem Member herausnehmen und die Zuhörer
    // informieren (getEdit() liefert jetzt nullptr, Oberfläche und Wellenformen lassen ihre
    // Handles los), dann das Edit zerstören. Sonst zeigen Handles kurz ins Leere (→ LEARNING.md §4).
    auto closingEdit = std::move (edit);
    const auto closingFolder = currentEditFile.getParentDirectory();

    // Wer regulär schließt, hat über die Änderungen entschieden (Speichern oder Verwerfen).
    // Nur nach einem Absturz bleibt die automatische Sicherung liegen.
    autosaveFileFor (currentEditFile).deleteFile();
    currentEditFile = juce::File();
    engineContext.setFocusedEdit (nullptr);
    notifyProjectChanged();

    closingEdit->getTransport().stop (false, false);
    closingEdit.reset();

    // Das Projekt nach dem Edit loslassen: Takes und Aufnahmen zeigen in das Projekt.
    project = nullptr;
    engineContext.getEngine().getProjectManager().removeProjectFromList (closingFolder);
}

bool ProjectService::hasProject() const noexcept
{
    return edit != nullptr;
}

juce::String ProjectService::getProjectName() const
{
    return currentEditFile.getFileNameWithoutExtension();
}

juce::File ProjectService::getProjectFolder() const
{
    return currentEditFile.getParentDirectory();
}

juce::File ProjectService::getEditFile() const
{
    return currentEditFile;
}

void ProjectService::registerProjectFile (const juce::File& file)
{
    if (project == nullptr || ! file.existsAsFile())
        return;

    // createNewItem liefert das vorhandene Element oder legt es an; bei ordnerbasierten
    // Projekten ergänzt das den gecachten Ordner-Scan.
    project->createNewItem (file,
                            te::ProjectItem::waveItemType(),
                            file.getFileNameWithoutExtension(),
                            {},
                            te::ProjectItem::Category::imported,
                            false);
}

te::Edit* ProjectService::getEdit() noexcept
{
    return edit.get();
}

const te::Edit* ProjectService::getEdit() const noexcept
{
    return edit.get();
}

void ProjectService::beginUndoStep (const juce::String& actionName)
{
    if (edit != nullptr)
        edit->getUndoManager().beginNewTransaction (actionName);
}

void ProjectService::undo()
{
    if (edit != nullptr)
        edit->getUndoManager().undo();
}

void ProjectService::redo()
{
    if (edit != nullptr)
        edit->getUndoManager().redo();
}

ProjectService::UndoHistory ProjectService::getUndoHistory() const
{
    UndoHistory history;

    if (edit == nullptr)
        return history;

    const auto named = [] (const juce::String& name)
    { return name.isNotEmpty() ? name : TRANS ("Change"); };

    // JUCE liefert die Undo-Liste vom jüngsten Schritt an; der Verlauf liest sich vom ältesten.
    const auto undoable = edit->getUndoManager().getUndoDescriptions();

    for (int i = undoable.size(); --i >= 0;)
        history.done.push_back (named (undoable[i]));

    for (const auto& name : edit->getUndoManager().getRedoDescriptions())
        history.undone.push_back (named (name));

    return history;
}

void ProjectService::jumpToUndoStep (int doneCount)
{
    if (edit == nullptr)
        return;

    auto& undoManager = edit->getUndoManager();
    const auto current = undoManager.getUndoDescriptions().size();
    const auto target = juce::jlimit (0, current + undoManager.getRedoDescriptions().size(), doneCount);

    for (int i = current; i > target; --i)
        undoManager.undo();

    for (int i = current; i < target; ++i)
        undoManager.redo();
}

bool ProjectService::canUndo() const
{
    return edit != nullptr && edit->getUndoManager().canUndo();
}

bool ProjectService::canRedo() const
{
    return edit != nullptr && edit->getUndoManager().canRedo();
}

void ProjectService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void ProjectService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void ProjectService::setEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& editFile)
{
    closeProject();

    edit = std::move (newEdit);
    currentEditFile = editFile;
    engineContext.setFocusedEdit (edit.get());
    attachProject (*edit, editFile);

    // Aufnahmen landen im Projektordner unter Audio/, nicht im System-Temp
    // (FkEngineBehaviour fragt dieses Verzeichnis ab).
    edit->setTempDirectory (editFile.getSiblingFile (audioFolderName));

    // Tracktion legt den Knoten für Clip-Launcher-Szenen (SCENES) erst beim ersten Einfügen
    // einer Spur an, und zwar innerhalb der laufenden Undo-Transaktion. Das Undo dieser ersten
    // Spur verwirrt dann JUCEs ValueTree (Assertion in juce_ValueTree.cpp:513). Deshalb den
    // Knoten jetzt anlegen, bevor der Nutzer etwas tut, und die Historie danach leeren.
    edit->getSceneList();
    edit->getUndoManager().clearUndoHistory();

    // Audio-Graph sofort aufbauen und auch im Stopp laufen lassen (Eingangsmonitoring).
    edit->playInStopEnabled = true;
    edit->getTransport().ensureContextAllocated();

    addToRecentProjects (editFile);
    notifyProjectChanged();
}

void ProjectService::attachProject (te::Edit& newEdit, const juce::File& editFile)
{
    // Tracktion kennt Takes (Loop-Aufnahme), Comps und Aufnahmen nur innerhalb eines Projekts.
    // Ein ordnerbasiertes Projekt ist genau unser Projektordner: Jede Datei darin ist ein
    // Projekt-Element, nichts muss registriert werden. Das Edit meldet sich beim Projekt an.
    auto& projectManager = engineContext.getEngine().getProjectManager();
    const auto folder = editFile.getParentDirectory();

    project = projectManager.getProject (folder);

    if (project == nullptr)
        project = projectManager.addProjectToList (folder, false, projectManager.getActiveProjectsFolder());

    if (project == nullptr)
    {
        juce::Logger::writeToLog (
            juce::String::fromUTF8 ("Projektordner konnte nicht als Tracktion-Projekt geöffnet werden: ") +
            folder.getFullPathName());
        return;
    }

    if (! project->getProjectID().isValid())
    {
        project->createNewProjectId();
        project->save();
    }

    if (auto editItem = project->createNewItem (editFile,
                                                te::ProjectItem::editItemType(),
                                                editFile.getFileNameWithoutExtension(),
                                                {},
                                                te::ProjectItem::Category::edit,
                                                true))
        newEdit.setProjectItemRef (editItem->getProjectItemRef());
}

void ProjectService::notifyProjectChanged()
{
    listeners.call ([] (Listener& listener) { listener.projectChanged(); });
}

} // namespace fk::services
