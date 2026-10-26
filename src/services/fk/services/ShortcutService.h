#pragma once

#include "fk/engine/EngineContext.h"

#include <juce_core/juce_core.h>

#include <map>

namespace te = tracktion;

namespace fk::services
{

/// Speichert die eigene Tastenbelegung in den Programmeinstellungen (gilt für alle Projekte). Kennt
/// keine Befehle, nur stabile Schlüssel und Tastenbeschreibungen ("ctrl + Z"); welche Befehle es gibt
/// und was ihr Standard ist, weiß die Oberfläche. Gespeichert werden nur Abweichungen vom Standard.
class ShortcutService final
{
public:
    explicit ShortcutService (engine::EngineContext& engineContext);

    /// Befehlsschlüssel → zugewiesene Tasten. Ein leeres Feld heißt "bewusst keine Taste".
    [[nodiscard]] std::map<juce::String, juce::StringArray> getOverrides() const;

    void setOverride (const juce::String& commandKey, const juce::StringArray& keyDescriptions);
    void clearOverride (const juce::String& commandKey);
    void clearAll();

private:
    void store (const std::map<juce::String, juce::StringArray>& overrides);

    engine::EngineContext& engineContext;
};

} // namespace fk::services
