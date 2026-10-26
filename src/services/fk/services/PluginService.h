#pragma once

#include "fk/core/Result.h"
#include "fk/engine/EngineContext.h"
#include "fk/services/ProjectService.h"
#include "fk/services/TrackService.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace fk::services
{

/// Ein verfügbares Plugin (eigenes oder später gescanntes), wie es im Menü erscheint.
struct PluginInfo
{
    juce::String id; ///< eindeutige Kennung (Format + Name + ID), stabil über Sitzungen
    juce::String name;
    juce::String category;
    juce::String manufacturer;
    juce::String format; ///< "FK", "VST3", "LV2" ...
    bool isInstrument = false;
    juce::String group;     ///< eigene Kategorie fürs Menü ("Dynamik", "EQ" ...), automatisch oder vom Nutzer
    bool favourite = false; ///< steht im Menü ganz oben
    bool hidden = false;    ///< fehlt in den Menüs, bleibt im Plugin-Manager
};

/// Ein eigenes Preset eines Plugins (für den Instrumenten-Browser).
struct PluginPreset
{
    juce::String pluginId;   ///< Plugin, zu dem das Preset gehört (Kennung aus getAllPlugins)
    juce::String pluginName;
    juce::String name;       ///< Preset-Name
};

/// Ein eingesetztes Plugin in einem Insert-Slot.
struct InsertInfo
{
    juce::String id; ///< Kennung der Instanz im Projekt
    juce::String name;
    bool enabled = true;
    bool baked = false;             ///< gebacken oder in einer gebackenen Gruppe: nicht bearbeitbar (ADR 0009)
    bool canSidechain = false;      ///< hat einen Sidechain-Eingang (Compressor, Gate, viele VST3)
    juce::String sidechainSourceId; ///< Spur, die in die Sidechain geht; leer = keine
    juce::String sidechainSourceName;
};

/// Eine Spur, die als Sidechain-Quelle in Frage kommt.
struct SidechainSource
{
    juce::String trackId;
    juce::String name;
};

/// Löscht eine Plugin-Oberfläche so, wie JUCE es von einem Host verlangt: erst dem Prozessor
/// Bescheid geben, dann freigeben. Das einzige "delete" im Projekt außerhalb von Fremdcode;
/// hier ist es der Vertrag des Frameworks, keine Stilwahl (CODING_GUIDELINES.md §6).
///
/// Der Deleter hält das Plugin des Editors am Leben (Referenzzählung von Tracktion): Wird der
/// Insert entfernt, während sein Fenster offen ist, lebt der Prozessor, bis der Editor weg ist.
/// Andernfalls zeigte der Editor kurz auf einen freigegebenen Prozessor (Absturz im Plugin-GUI).
struct EditorDeleter
{
    juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject> keepAlive;

    void operator() (juce::AudioProcessorEditor* editor) const noexcept
    {
        if (editor == nullptr)
            return;

        editor->processor.editorBeingDeleted (editor);
        delete editor;
    }
};

/// Besitzender Zeiger auf eine Plugin-Oberfläche.
using EditorPtr = std::unique_ptr<juce::AudioProcessorEditor, EditorDeleter>;

/// Ergebnis eines vollständigen Plugin-Scans.
struct ScanResult
{
    int numPluginsBefore = 0;
    int numPluginsAfter = 0;
    std::vector<juce::String> failedFiles; ///< Dateien, die der Scan-Helfer nicht laden konnte (oder die ihn töteten)
};

/// Anwendungsfälle für Plugins: verfügbare auflisten, in Spuren einsetzen, entfernen,
/// umgehen (Bypass), Editor erzeugen. Eigene und fremde Plugins laufen hier über
/// denselben Weg; die Oberfläche sieht keinen Unterschied.
class PluginService final : private ProjectService::Listener,
                            private juce::ValueTree::Listener,
                            private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /// Inserts wurden hinzugefügt, entfernt oder umgeschaltet.
        virtual void insertsChanged() = 0;
    };

    PluginService (engine::EngineContext& engineContext, ProjectService& projectService);
    ~PluginService() override;

    /// Alle bekannten Effekte ohne ausgeblendete, eigene zuerst, dann nach Hersteller und Name sortiert.
    [[nodiscard]] std::vector<PluginInfo> getAvailableEffects() const;
    [[nodiscard]] std::vector<PluginInfo> getAvailableInstruments() const;

    /// Alle Plugins, auch ausgeblendete, für die Verwaltung im Plugin-Manager.
    [[nodiscard]] std::vector<PluginInfo> getAllPlugins() const;

    /// Kategorien für Menü und Plugin-Manager, in Anzeigereihenfolge.
    /// Die Namen sind gespeicherte Kennungen (deutsch, nie ändern); angezeigt wird getGroupDisplayName().
    [[nodiscard]] static const juce::StringArray& getGroupNames();

    /// Anzeigetext einer Kategorie in der Sprache der Oberfläche; unbekannte Namen bleiben unverändert.
    [[nodiscard]] static juce::String getGroupDisplayName (const juce::String& group);

    /// Favoriten, Ausblenden und eigene Kategorie (leer = automatisch). Gelten für alle Projekte.
    [[nodiscard]] core::Result<void> setFavourite (const juce::String& pluginId, bool shouldBeFavourite);
    [[nodiscard]] core::Result<void> setHidden (const juce::String& pluginId, bool shouldBeHidden);
    [[nodiscard]] core::Result<void> setGroup (const juce::String& pluginId, const juce::String& group);

    /// Eigene Presets des Plugins in diesem Insert (alle Instanzen desselben Plugins teilen sie).
    [[nodiscard]] std::vector<juce::String> getUserPresets (const juce::String& insertId) const;
    [[nodiscard]] core::Result<void> saveUserPreset (const juce::String& insertId, const juce::String& name);
    [[nodiscard]] core::Result<void> loadUserPreset (const juce::String& insertId, const juce::String& name);
    [[nodiscard]] core::Result<void> deleteUserPreset (const juce::String& insertId, const juce::String& name);

    /// Alle eigenen Presets aller bekannten Plugins (nur Instrumente: instrumentsOnly).
    [[nodiscard]] std::vector<PluginPreset> getAllUserPresets (bool instrumentsOnly) const;

    /// Werks-Programme, die das Plugin selbst mitbringt (bei vielen VST3 die Factory-Presets).
    [[nodiscard]] std::vector<juce::String> getFactoryPrograms (const juce::String& insertId) const;
    [[nodiscard]] core::Result<void> loadFactoryProgram (const juce::String& insertId, int programIndex);

    /// Zugriff für den Plugin-Manager (juce::PluginListComponent): Formate und bekannte Plugins.
    /// Beides sind JUCE-Objekte, keine Tracktion-Aufrufe (ADR 0004).
    [[nodiscard]] juce::AudioPluginFormatManager& getFormatManager();
    [[nodiscard]] juce::KnownPluginList& getKnownPluginList();

    /// Datei, in der JUCE das zuletzt gescannte Plugin vermerkt; überlebt es den Scan nicht,
    /// wird es beim nächsten Mal übersprungen (zusätzliche Sicherheit zum Unterprozess).
    [[nodiscard]] juce::File getDeadMansPedalFile() const;
    [[nodiscard]] std::vector<InsertInfo> getInserts (TrackHandle& track) const;

    /// Das Instrument der Spur (Plugin mit isSynth), leer auf Audio-, Gruppen- und FX-Spuren.
    [[nodiscard]] std::optional<InsertInfo> getInstrument (TrackHandle& track) const;

    /// Tauscht das Instrument der Spur gegen ein anderes (Kennung aus getAvailableInstruments);
    /// Inserts, Eingang und Clips bleiben. Liefert die Kennung der neuen Instanz.
    [[nodiscard]] core::Result<juce::String> setInstrument (TrackHandle& track, const juce::String& instrumentPluginId);

    /// Setzt das Plugin hinter die vorhandenen Inserts der Spur. Liefert die Insert-Kennung.
    [[nodiscard]] core::Result<juce::String> insertPlugin (TrackHandle& track, const juce::String& pluginId);
    [[nodiscard]] core::Result<void> removeInsert (const juce::String& insertId);

    /// Spuren, die als Sidechain dieses Inserts taugen (alle Audiospuren außer der eigenen).
    [[nodiscard]] std::vector<SidechainSource> getSidechainSources (const juce::String& insertId) const;

    /// Sidechain-Quelle setzen (leere Kennung = keine). Bei unseren Plugins schaltet das auch
    /// "Ext. Sidechain" passend ein oder aus, damit Quelle und Schalter nie auseinanderlaufen.
    [[nodiscard]] core::Result<void> setSidechainSource (const juce::String& insertId,
                                                         const juce::String& sourceTrackId);
    [[nodiscard]] core::Result<void> setInsertEnabled (const juce::String& insertId, bool shouldBeEnabled);

    /// Erzeugt die Bedienoberfläche des Plugins. Der Aufrufer besitzt sie und muss sie
    /// freigeben, bevor das Projekt wechselt (insertsChanged / projectChanged beachten).
    [[nodiscard]] core::Result<EditorPtr> createEditor (const juce::String& insertId);

    /// Aktuelle Pegelabsenkung eines Dynamik-Inserts (FK Compressor/Limiter) in dB (≤ 0), sonst leer.
    [[nodiscard]] std::optional<float> getInsertGainReductionDb (const juce::String& insertId) const;

    /// Dasselbe für alle Inserts einer Spur auf einmal, gleiche Reihenfolge wie getInserts. Für
    /// Pegel-Timer: ein Durchlauf über die Spur statt einer Suche im ganzen Projekt je Insert.
    [[nodiscard]] std::vector<std::optional<float>> getInsertGainReductionsDb (TrackHandle& track) const;

    /// Durchsucht die Standardpfade aller Formate (VST3, LV2 ...) blockierend. Für die
    /// Kommandozeile und Tests; die Oberfläche nutzt den Plugin-Manager mit Fortschrittsanzeige.
    [[nodiscard]] ScanResult
    scanAllFormats (const std::function<void (const juce::String& currentFile)>& progress = nullptr);

    /// Macht einzelne Plugin-Dateien bekannt (die mitgelieferten Lite-Plugins, BundledPlugins), ohne
    /// die übrigen Ordner zu durchsuchen. Schon bekannte, unveränderte Dateien werden nicht neu
    /// gescannt. Blockiert für den Scan im Unterprozess. Liefert die Zahl neuer Plugins.
    int addPluginFiles (const std::vector<juce::File>& files);

    void addListener (Listener& listener);
    void removeListener (Listener& listener);

private:
    void projectChanged() override;
    void handleAsyncUpdate() override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int oldIndex, int newIndex) override;

    [[nodiscard]] std::vector<PluginInfo> getAvailablePlugins (bool instruments) const;
    [[nodiscard]] std::vector<PluginInfo> collectPlugins (std::optional<bool> instruments, bool includeHidden) const;
    [[nodiscard]] juce::var loadPluginPrefs() const;
    [[nodiscard]] core::Result<void> changePluginPref (const juce::String& pluginId,
                                                      const juce::Identifier& property,
                                                      const juce::var& value);
    [[nodiscard]] juce::File presetFolderFor (const te::ExternalPlugin& plugin) const;
    [[nodiscard]] juce::File presetFolderFor (const juce::PluginDescription& description) const;
    [[nodiscard]] te::ExternalPlugin* findInsert (const juce::String& insertId) const;
    [[nodiscard]] static std::vector<te::ExternalPlugin*> getExternalPlugins (TrackHandle& track);
    [[nodiscard]] static std::optional<float> gainReductionOf (te::ExternalPlugin& plugin);
    [[nodiscard]] static InsertInfo infoFor (te::ExternalPlugin& plugin);
    [[nodiscard]] static core::Result<void> checkUnlocked (te::ExternalPlugin& plugin);

    engine::EngineContext& engineContext;
    ProjectService& projectService;
    juce::ValueTree observedEditState;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginService)
};

} // namespace fk::services
