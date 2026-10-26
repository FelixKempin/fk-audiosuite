#include "fk/services/PluginService.h"

#include "fk/plugins/CompressorProcessor.h"
#include "fk/plugins/LimiterProcessor.h"
#include "fk/services/BakeLock.h"
#include "fk/services/ChannelTrim.h"
#include "fk/services/InstrumentOutputs.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto pluginPrefsItem = "fkPluginPrefs"; // JSON-Objekt in den Engine-Einstellungen
constexpr auto presetExtension = ".fkpreset";
const juce::Identifier favouriteProperty { "favourite" };
const juce::Identifier hiddenProperty { "hidden" };
const juce::Identifier groupProperty { "group" };

/// Kategorie aus der Beschreibung des Plugins: VST3 meldet z. B. "Fx|Dynamics", unsere Plugins
/// "Dynamik". Wörter aus Kategorie und Name werden gegen Stichwortlisten geprüft.
juce::String automaticGroup (const juce::PluginDescription& description)
{
    if (description.isInstrument)
        return "Instrument";

    if (description.category.containsIgnoreCase ("MIDI"))
        return "MIDI-Effekte";

    juce::StringArray words;
    words.addTokens ((description.category + " " + description.name).toLowerCase(), " |/-_,.()", {});

    const auto hasAny = [&words] (std::initializer_list<const char*> keywords)
    {
        for (const auto* keyword : keywords)
            for (const auto& word : words)
                if (word == keyword || (std::string_view (keyword).size() > 4 && word.startsWith (keyword)))
                    return true;

        return false;
    };

    if (hasAny ({ "dynamics", "dynamik", "compressor", "kompressor", "limiter", "gate", "expander", "deesser" }))
        return "Dynamik";

    if (hasAny ({ "eq", "equalizer", "equaliser" }))
        return "EQ";

    if (hasAny ({ "reverb", "hall", "delay", "echo" }))
        return juce::String ("Hall und Delay");

    if (hasAny ({ "distortion", "verzerrung", "saturation", "saturator", "overdrive", "amp", "fuzz", "rig" }))
        return "Verzerrung";

    if (hasAny ({ "modulation", "chorus", "flanger", "phaser", "tremolo", "vibrato" }))
        return "Modulation";

    if (hasAny ({ "filter" }))
        return "Filter";

    if (hasAny ({ "analyzer", "analyser", "tools", "utility", "meter", "spatial", "pitch", "trim" }))
        return "Werkzeuge";

    return "Sonstige";
}
} // namespace

PluginService::PluginService (engine::EngineContext& context, ProjectService& service)
    : engineContext (context)
    , projectService (service)
{
    projectService.addListener (*this);
    projectChanged();
}

PluginService::~PluginService()
{
    observedEditState.removeListener (this);
    projectService.removeListener (*this);
}

std::vector<PluginInfo> PluginService::getAvailableEffects() const
{
    return getAvailablePlugins (false);
}

std::vector<PluginInfo> PluginService::getAvailableInstruments() const
{
    return getAvailablePlugins (true);
}

std::vector<PluginInfo> PluginService::getAvailablePlugins (bool instruments) const
{
    return collectPlugins (instruments, false);
}

std::vector<PluginInfo> PluginService::getAllPlugins() const
{
    return collectPlugins (std::nullopt, true);
}

const juce::StringArray& PluginService::getGroupNames()
{
    static const juce::StringArray names { "MIDI-Effekte", "Dynamik",  "EQ",       "Filter",   juce::String ("Hall und Delay"),
                                           "Modulation", "Verzerrung", "Werkzeuge", "Sonstige",
                                           "Instrument" };
    return names;
}

juce::String PluginService::getGroupDisplayName (const juce::String& group)
{
    if (group == "MIDI-Effekte")
        return TRANS ("MIDI effects");

    if (group == "Dynamik")
        return TRANS ("Dynamics");

    if (group == "EQ")
        return TRANS ("EQ");

    if (group == "Filter")
        return TRANS ("Filter");

    if (group == "Hall und Delay")
        return TRANS ("Reverb and delay");

    if (group == "Modulation")
        return TRANS ("Modulation");

    if (group == "Verzerrung")
        return TRANS ("Distortion");

    if (group == "Werkzeuge")
        return TRANS ("Tools");

    if (group == "Sonstige")
        return TRANS ("Other");

    if (group == "Instrument")
        return TRANS ("Instrument");

    return group;
}

std::vector<PluginInfo> PluginService::collectPlugins (std::optional<bool> instruments, bool includeHidden) const
{
    const auto prefs = loadPluginPrefs();
    std::vector<PluginInfo> plugins;

    for (const auto& description : engineContext.getEngine().getPluginManager().knownPluginList.getTypes())
    {
        if ((instruments && description.isInstrument != *instruments) || channelTrim::isChannelTrim (description))
            continue;

        PluginInfo info { description.createIdentifierString(),
                          description.name,
                          description.category,
                          description.manufacturerName,
                          description.pluginFormatName,
                          description.isInstrument };

        const auto pref = prefs.getProperty (info.id, {});
        info.favourite = pref.getProperty (favouriteProperty, false);
        info.hidden = pref.getProperty (hiddenProperty, false);
        info.group = pref.getProperty (groupProperty, {}).toString();

        if (info.group.isEmpty())
            info.group = automaticGroup (description);

        if (includeHidden || ! info.hidden)
            plugins.push_back (std::move (info));
    }

    std::ranges::sort (plugins,
                       [] (const PluginInfo& a, const PluginInfo& b)
                       {
                           const bool aInternal = a.format == "FK";
                           const bool bInternal = b.format == "FK";

                           if (aInternal != bInternal)
                               return aInternal;

                           if (a.manufacturer != b.manufacturer)
                               return a.manufacturer.compareIgnoreCase (b.manufacturer) < 0;

                           return a.name.compareIgnoreCase (b.name) < 0;
                       });

    return plugins;
}

juce::var PluginService::loadPluginPrefs() const
{
    const auto stored = juce::JSON::parse (
        engineContext.getEngine().getPropertyStorage().getPropertyItem (te::SettingID::invalid, pluginPrefsItem).toString());
    return stored.isObject() ? stored : juce::var (new juce::DynamicObject());
}

core::Result<void> PluginService::changePluginPref (const juce::String& pluginId,
                                                    const juce::Identifier& property,
                                                    const juce::var& value)
{
    const auto known = std::ranges::any_of (engineContext.getEngine().getPluginManager().knownPluginList.getTypes(),
                                            [&pluginId] (const juce::PluginDescription& description)
                                            { return description.createIdentifierString() == pluginId; });

    if (! known)
        return core::fail (core::ErrorCode::notFound, TRANS ("Plugin not found.").toStdString());

    auto prefs = loadPluginPrefs();
    auto* object = prefs.getDynamicObject();
    auto entry = object->getProperty (pluginId);

    if (! entry.isObject())
        entry = juce::var (new juce::DynamicObject());

    entry.getDynamicObject()->setProperty (property, value);
    object->setProperty (pluginId, entry);

    auto& storage = engineContext.getEngine().getPropertyStorage();
    storage.setPropertyItem (te::SettingID::invalid, pluginPrefsItem, juce::JSON::toString (prefs, true));
    storage.flushSettingsToDisk();
    return {};
}

core::Result<void> PluginService::setFavourite (const juce::String& pluginId, bool shouldBeFavourite)
{
    return changePluginPref (pluginId, favouriteProperty, shouldBeFavourite);
}

core::Result<void> PluginService::setHidden (const juce::String& pluginId, bool shouldBeHidden)
{
    return changePluginPref (pluginId, hiddenProperty, shouldBeHidden);
}

core::Result<void> PluginService::setGroup (const juce::String& pluginId, const juce::String& group)
{
    if (group.isNotEmpty() && ! getGroupNames().contains (group))
        return core::fail (core::ErrorCode::invalidArgument, plugins::i18n::format (TRANS ("Unknown category: {group}"), { { "group", group } }).toStdString());

    return changePluginPref (pluginId, groupProperty, group);
}

juce::File PluginService::presetFolderFor (const te::ExternalPlugin& plugin) const
{
    return presetFolderFor (plugin.desc);
}

juce::File PluginService::presetFolderFor (const juce::PluginDescription& description) const
{
    // Ein Ordner je Plugin (Hersteller und Name), im Einstellungsordner der Engine; Tests haben einen eigenen.
    const auto folderName = juce::File::createLegalFileName (
        (description.manufacturerName.isNotEmpty() ? description.manufacturerName + " - " : juce::String())
        + description.name);
    return engineContext.getEngine().getPropertyStorage().getAppPrefsFolder().getChildFile ("Presets").getChildFile (
        folderName);
}

std::vector<PluginPreset> PluginService::getAllUserPresets (bool instrumentsOnly) const
{
    std::vector<PluginPreset> presets;

    for (const auto& description : engineContext.getEngine().getPluginManager().knownPluginList.getTypes())
    {
        if (instrumentsOnly && ! description.isInstrument)
            continue;

        for (const auto& file :
             presetFolderFor (description).findChildFiles (juce::File::findFiles, false, "*" + juce::String (presetExtension)))
            presets.push_back ({ description.createIdentifierString(), description.name, file.getFileNameWithoutExtension() });
    }

    return presets;
}

std::vector<juce::String> PluginService::getUserPresets (const juce::String& insertId) const
{
    std::vector<juce::String> names;

    if (auto* plugin = findInsert (insertId))
        for (const auto& file : presetFolderFor (*plugin).findChildFiles (juce::File::findFiles, false, "*" + juce::String (presetExtension)))
            names.push_back (file.getFileNameWithoutExtension());

    std::ranges::sort (names, [] (const juce::String& a, const juce::String& b) { return a.compareNatural (b) < 0; });
    return names;
}

core::Result<void> PluginService::saveUserPreset (const juce::String& insertId, const juce::String& name)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    const auto cleanName = juce::File::createLegalFileName (name.trim());

    if (cleanName.isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The preset needs a name.").toStdString());

    auto* instance = plugin->getAudioPluginInstance();

    if (instance == nullptr)
        return core::fail (core::ErrorCode::unsupported, TRANS ("The plugin is not loaded.").toStdString());

    juce::MemoryBlock state;
    instance->getStateInformation (state);

    juce::XmlElement xml ("FKPRESET");
    xml.setAttribute ("plugin", plugin->desc.createIdentifierString());
    xml.setAttribute ("name", cleanName);
    xml.addTextElement (state.toBase64Encoding());

    const auto file = presetFolderFor (*plugin).getChildFile (cleanName + presetExtension);
    file.getParentDirectory().createDirectory();

    if (! xml.writeTo (file))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not write the preset: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    return {};
}

core::Result<void> PluginService::loadUserPreset (const juce::String& insertId, const juce::String& name)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return unlocked;

    const auto file = presetFolderFor (*plugin).getChildFile (juce::File::createLegalFileName (name) + presetExtension);
    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("FKPRESET"))
        return core::fail (core::ErrorCode::ioError, plugins::i18n::format (TRANS ("Could not read the preset: {path}"), { { "path", file.getFullPathName() } }).toStdString());

    if (xml->getStringAttribute ("plugin") != plugin->desc.createIdentifierString())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The preset belongs to a different plugin.").toStdString());

    juce::MemoryBlock state;

    if (! state.fromBase64Encoding (xml->getAllSubText().trim()))
        return core::fail (core::ErrorCode::ioError, TRANS ("The preset is damaged.").toStdString());

    auto* instance = plugin->getAudioPluginInstance();

    if (instance == nullptr)
        return core::fail (core::ErrorCode::unsupported, TRANS ("The plugin is not loaded.").toStdString());

    projectService.beginUndoStep (TRANS ("Load preset"));
    instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    plugin->flushPluginStateToValueTree(); // ins Projekt, damit Speichern und Undo den Zustand kennen
    return {};
}

core::Result<void> PluginService::deleteUserPreset (const juce::String& insertId, const juce::String& name)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    const auto file = presetFolderFor (*plugin).getChildFile (juce::File::createLegalFileName (name) + presetExtension);

    if (! file.existsAsFile() || ! file.deleteFile())
        return core::fail (core::ErrorCode::notFound, TRANS ("Preset not found.").toStdString());

    return {};
}

std::vector<juce::String> PluginService::getFactoryPrograms (const juce::String& insertId) const
{
    std::vector<juce::String> programs;

    if (auto* plugin = findInsert (insertId))
        for (int i = 0; i < plugin->getNumPrograms(); ++i)
            programs.push_back (plugin->getProgramName (i));

    // Ein einzelnes namenloses Programm ist kein Preset, sondern der Normalzustand.
    if (programs.size() == 1 && programs.front().trim().isEmpty())
        programs.clear();

    return programs;
}

core::Result<void> PluginService::loadFactoryProgram (const juce::String& insertId, int programIndex)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return unlocked;

    if (programIndex < 0 || programIndex >= plugin->getNumPrograms())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("Program does not exist.").toStdString());

    projectService.beginUndoStep (TRANS ("Load program"));
    plugin->setCurrentProgram (programIndex, true);
    plugin->flushPluginStateToValueTree();
    return {};
}

juce::AudioPluginFormatManager& PluginService::getFormatManager()
{
    return engineContext.getEngine().getPluginManager().pluginFormatManager;
}

juce::KnownPluginList& PluginService::getKnownPluginList()
{
    return engineContext.getEngine().getPluginManager().knownPluginList;
}

juce::File PluginService::getDeadMansPedalFile() const
{
    return engineContext.getEngine().getTemporaryFileManager().getTempDirectory().getChildFile (
        "PluginScanDeadMansPedal.txt");
}

std::vector<InsertInfo> PluginService::getInserts (TrackHandle& track) const
{
    std::vector<InsertInfo> inserts;

    for (auto* plugin : getExternalPlugins (track))
        if (! plugin->isSynth()) // das Instrument selbst ist kein Insert
            inserts.push_back (infoFor (*plugin));

    return inserts;
}

std::optional<InsertInfo> PluginService::getInstrument (TrackHandle& track) const
{
    for (auto* plugin : getExternalPlugins (track))
        if (plugin->isSynth())
            return infoFor (*plugin);

    // Spur an einem Rack-Instrument: das Instrument steckt im Rack, nicht auf der Spur.
    for (auto* plugin : track.pluginList.getPlugins())
        if (auto* instance = dynamic_cast<te::RackInstance*> (plugin))
            if (instance->type != nullptr)
                for (auto* rackPlugin : instance->type->getPlugins())
                    if (auto* external = dynamic_cast<te::ExternalPlugin*> (rackPlugin); external != nullptr && external->isSynth())
                        return infoFor (*external);

    return std::nullopt;
}

core::Result<juce::String> PluginService::setInstrument (TrackHandle& track, const juce::String& instrumentPluginId)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    const auto description =
        engineContext.getEngine().getPluginManager().knownPluginList.getTypeForIdentifierString (instrumentPluginId);

    if (description == nullptr || ! description->isInstrument)
        return core::fail (core::ErrorCode::notFound,
                           plugins::i18n::format (TRANS ("Instrument not found: {id}"), { { "id", instrumentPluginId } }).toStdString());

    for (auto* plugin : getExternalPlugins (track))
        if (plugin->isSynth())
            if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
                return std::unexpected (unlocked.error());

    for (auto* plugin : track.pluginList.getPlugins())
        if (dynamic_cast<te::RackInstance*> (plugin) != nullptr)
            return core::fail (core::ErrorCode::unsupported,
                               TRANS ("The track plays a rack instrument; the instrument belongs to the rack, not the track.").toStdString());

    projectService.beginUndoStep (TRANS ("Replace instrument"));

    // Das alte Instrument an seinem Platz entfernen, das neue an dieselbe Stelle (vor den Inserts).
    int index = 0;

    for (auto* plugin : getExternalPlugins (track))
        if (plugin->isSynth())
        {
            index = track.pluginList.getPlugins().indexOf (plugin);
            plugin->deleteFromParent();
            break;
        }

    auto plugin = track.pluginList.insertPlugin (te::ExternalPlugin::create (edit->engine, *description), index);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::unknown,
                           plugins::i18n::format (TRANS ("Could not load the instrument: {name}"), { { "name", description->name } }).toStdString());

    instrumentOutputs::useMainOutputOnly (*plugin);
    return plugin->itemID.toString();
}

core::Result<juce::String> PluginService::insertPlugin (TrackHandle& track, const juce::String& pluginId)
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("No project is open.").toStdString());

    auto& knownPlugins = engineContext.getEngine().getPluginManager().knownPluginList;
    const auto description = knownPlugins.getTypeForIdentifierString (pluginId);

    if (description == nullptr)
        return core::fail (core::ErrorCode::notFound, plugins::i18n::format (TRANS ("Plugin not found: {id}"), { { "id", pluginId } }).toStdString());

    if (bakeLock::findBakedDestination (track) != nullptr)
        return std::unexpected (bakeLock::lockedError (track));

    projectService.beginUndoStep (TRANS ("Insert plugin"));

    // Hinter den letzten Insert bzw. hinter den Aux-Return einer FX-Spur, vor Sends, Lautstärke/Pan, Meter.
    // Ein Freeze-Punkt (Bake) zählt mit: Neue Inserts kommen hinter die gebackenen und bleiben live.
    int insertIndex = 0;
    int position = 0;

    for (auto* existing : track.pluginList.getPlugins())
    {
        if (dynamic_cast<te::ExternalPlugin*> (existing) != nullptr ||
            dynamic_cast<te::AuxReturnPlugin*> (existing) != nullptr ||
            dynamic_cast<te::FreezePointPlugin*> (existing) != nullptr)
            insertIndex = position + 1;

        ++position;
    }

    // MIDI-Effekte wirken nur vor dem Instrument (auch vor der Rack-Instanz einer Rack-MIDI-Spur);
    // hinter weiteren MIDI-Effekten, damit die Reihenfolge der Kette der Reihenfolge des Einsetzens folgt.
    if (description->category.containsIgnoreCase ("MIDI"))
    {
        position = 0;

        for (auto* existing : track.pluginList.getPlugins())
        {
            if (existing->isSynth())
            {
                insertIndex = position;
                break;
            }

            ++position;
        }
    }

    auto plugin = track.pluginList.insertPlugin (te::ExternalPlugin::create (edit->engine, *description), insertIndex);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::unknown,
                           plugins::i18n::format (TRANS ("Could not insert the plugin: {name}"), { { "name", description->name } }).toStdString());

    return plugin->itemID.toString();
}

core::Result<void> PluginService::removeInsert (const juce::String& insertId)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return unlocked;

    projectService.beginUndoStep (TRANS ("Remove plugin"));
    plugin->deleteFromParent();
    return {};
}

core::Result<void> PluginService::setInsertEnabled (const juce::String& insertId, bool shouldBeEnabled)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return unlocked;

    plugin->setEnabled (shouldBeEnabled);
    return {};
}

core::Result<EditorPtr> PluginService::createEditor (const juce::String& insertId)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return std::unexpected (unlocked.error());

    auto* instance = plugin->getAudioPluginInstance();

    if (instance == nullptr)
        return core::fail (core::ErrorCode::deviceError,
                           plugins::i18n::format (TRANS ("Plugin is not loaded: {name}"), { { "name", plugin->getName() } }).toStdString());

    // JUCE gibt den Editor als rohen, besessenen Zeiger heraus; EditorPtr übernimmt ihn sofort.
    EditorPtr editor (instance->hasEditor() ? instance->createEditorAndMakeActive()
                                            : new juce::GenericAudioProcessorEditor (*instance),
                      EditorDeleter { plugin });

    if (editor == nullptr)
        return core::fail (core::ErrorCode::unknown, TRANS ("Could not create the plugin window.").toStdString());

    return editor;
}

ScanResult PluginService::scanAllFormats (const std::function<void (const juce::String&)>& progress)
{
    auto& knownPlugins = getKnownPluginList();
    ScanResult result;
    result.numPluginsBefore = knownPlugins.getNumTypes();

    for (auto* format : getFormatManager().getFormats())
    {
        if (! format->canScanForPlugins())
            continue;

        juce::PluginDirectoryScanner scanner (
            knownPlugins, *format, format->getDefaultLocationsToSearch(), true, getDeadMansPedalFile(), true);
        juce::String currentFile;

        while (scanner.scanNextFile (true, currentFile))
            if (progress)
                progress (format->getName() + ": " + currentFile);

        for (const auto& failed : scanner.getFailedFiles())
            result.failedFiles.push_back (failed);
    }

    // Tracktion speichert die Liste in seinem ChangeListener, der normalerweise asynchron über
    // die Ereignisschleife läuft. Synchron auslösen, damit auch ein Aufrufer ohne laufende
    // Schleife (Kommandozeile, Tests) die Liste gespeichert bekommt.
    knownPlugins.sendSynchronousChangeMessage();

    result.numPluginsAfter = knownPlugins.getNumTypes();
    return result;
}

int PluginService::addPluginFiles (const std::vector<juce::File>& files)
{
    auto& knownPlugins = getKnownPluginList();
    const auto numBefore = knownPlugins.getNumTypes();

    for (const auto& file : files)
        for (auto* format : getFormatManager().getFormats())
            if (format->fileMightContainThisPluginType (file.getFullPathName()))
            {
                juce::OwnedArray<juce::PluginDescription> found;
                knownPlugins.scanAndAddFile (file.getFullPathName(), true, found, *format);
                break;
            }

    knownPlugins.sendSynchronousChangeMessage(); // wie scanAllFormats: Liste sofort speichern
    return knownPlugins.getNumTypes() - numBefore;
}

std::optional<float> PluginService::getInsertGainReductionDb (const juce::String& insertId) const
{
    if (auto* plugin = findInsert (insertId))
        return gainReductionOf (*plugin);

    return std::nullopt;
}

std::vector<std::optional<float>> PluginService::getInsertGainReductionsDb (TrackHandle& track) const
{
    std::vector<std::optional<float>> reductions;

    for (auto* plugin : getExternalPlugins (track))
        if (! plugin->isSynth())
            reductions.push_back (gainReductionOf (*plugin));

    return reductions;
}

InsertInfo PluginService::infoFor (te::ExternalPlugin& plugin)
{
    InsertInfo info {
        plugin.itemID.toString(), plugin.getName(), plugin.isEnabled(), bakeLock::isPluginLocked (plugin)
    };
    info.canSidechain = plugin.canSidechain();

    if (const auto sourceId = plugin.getSidechainSourceID(); sourceId.isValid())
        if (auto* source = te::findTrackForID (plugin.edit, sourceId))
        {
            info.sidechainSourceId = source->itemID.toString();
            info.sidechainSourceName = source->getName();
        }

    return info;
}

std::vector<SidechainSource> PluginService::getSidechainSources (const juce::String& insertId) const
{
    std::vector<SidechainSource> sources;
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr || ! plugin->canSidechain())
        return sources;

    for (auto* track : te::getAudioTracks (plugin->edit))
        if (track != plugin->getOwnerTrack())
            sources.push_back ({ track->itemID.toString(), track->getName() });

    return sources;
}

core::Result<void> PluginService::setSidechainSource (const juce::String& insertId, const juce::String& sourceTrackId)
{
    auto* plugin = findInsert (insertId);

    if (plugin == nullptr)
        return core::fail (core::ErrorCode::notFound, TRANS ("Insert not found.").toStdString());

    if (! plugin->canSidechain())
        return core::fail (core::ErrorCode::unsupported, TRANS ("This plugin has no sidechain input.").toStdString());

    if (auto unlocked = checkUnlocked (*plugin); ! unlocked)
        return unlocked;

    te::Track* source = nullptr;

    if (sourceTrackId.isNotEmpty())
    {
        source = te::findTrackForID (plugin->edit, te::EditItemID::fromString (sourceTrackId));

        if (source == nullptr || source == plugin->getOwnerTrack())
            return core::fail (core::ErrorCode::invalidArgument, TRANS ("This track cannot be a sidechain source.").toStdString());
    }

    projectService.beginUndoStep (TRANS ("Sidechain source"));

    if (source != nullptr)
    {
        plugin->setSidechainSourceID (source->itemID);

        // Kanäle der Quelle auf die Sidechain-Eingänge legen (Tracktions Standardverdrahtung).
        if (plugin->getNumWires() == 0)
            plugin->guessSidechainRouting();
    }
    else
    {
        plugin->setSidechainSourceID ({});
    }

    // Unsere Plugins haben einen Schalter "Ext. Sidechain": mitschalten, damit sie auch messen, was ankommt.
    if (auto* instance = plugin->getAudioPluginInstance())
        for (auto* parameter : instance->getParameters())
            if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter);
                withId != nullptr && withId->getParameterID() == "sidechain")
                parameter->setValueNotifyingHost (source != nullptr ? 1.0f : 0.0f);

    return {};
}

core::Result<void> PluginService::checkUnlocked (te::ExternalPlugin& plugin)
{
    if (! bakeLock::isPluginLocked (plugin))
        return {};

    if (auto* track = dynamic_cast<te::AudioTrack*> (plugin.getOwnerTrack()))
        return std::unexpected (bakeLock::lockedError (*track));

    return core::fail (core::ErrorCode::invalidArgument, TRANS ("The plugin is baked.").toStdString());
}

std::optional<float> PluginService::gainReductionOf (te::ExternalPlugin& plugin)
{
    auto* instance = plugin.getAudioPluginInstance();

    if (auto* compressor = dynamic_cast<plugins::CompressorProcessor*> (instance))
        return compressor->getGainReductionDb();

    if (auto* limiter = dynamic_cast<plugins::LimiterProcessor*> (instance))
        return limiter->getGainReductionDb();

    return std::nullopt;
}

void PluginService::addListener (Listener& listener)
{
    listeners.add (&listener);
}

void PluginService::removeListener (Listener& listener)
{
    listeners.remove (&listener);
}

void PluginService::projectChanged()
{
    observedEditState.removeListener (this);
    observedEditState = {};

    if (auto* edit = projectService.getEdit())
    {
        observedEditState = edit->state;
        observedEditState.addListener (this);
    }

    cancelPendingUpdate();
    handleAsyncUpdate();
}

void PluginService::handleAsyncUpdate()
{
    listeners.call ([] (Listener& listener) { listener.insertsChanged(); });
}

void PluginService::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (te::IDs::PLUGIN))
        triggerAsyncUpdate();
}

void PluginService::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int)
{
    if (child.hasType (te::IDs::PLUGIN))
        triggerAsyncUpdate();
}

void PluginService::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (te::IDs::PLUGIN) && property == te::IDs::enabled)
        triggerAsyncUpdate();
}

void PluginService::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (te::IDs::TRACK))
        triggerAsyncUpdate();
}

te::ExternalPlugin* PluginService::findInsert (const juce::String& insertId) const
{
    auto* edit = projectService.getEdit();

    if (edit == nullptr)
        return nullptr;

    for (auto* track : te::getAudioTracks (*edit))
        for (auto* plugin : getExternalPlugins (*track))
            if (plugin->itemID.toString() == insertId)
                return plugin;

    // Instrumente in Rack-Instrumenten (RackService).
    for (auto* rack : edit->getRackList().getTypes())
        for (auto* plugin : rack->getPlugins())
            if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin); external != nullptr && external->itemID.toString() == insertId)
                return external;

    return nullptr;
}

std::vector<te::ExternalPlugin*> PluginService::getExternalPlugins (TrackHandle& track)
{
    std::vector<te::ExternalPlugin*> result;

    // Tracktion liefert alle Plugins der Spur, auch seine festen (Lautstärke, Meter).
    // dynamic_cast filtert die Inserts heraus; legitim, weil die Liste eine Basisklasse liefert.
    for (auto plugin : track.pluginList.getPlugins())
        if (auto* external = dynamic_cast<te::ExternalPlugin*> (plugin);
            external != nullptr && ! channelTrim::isChannelTrim (*external))
            result.push_back (external);

    return result;
}

} // namespace fk::services
