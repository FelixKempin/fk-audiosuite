#pragma once

#include "fk/core/Result.h"

#include <juce_core/juce_core.h>

namespace fk::plugins::i18n
{

/// Einstellungen, die für alle FK-Programme eines Nutzers gelten (DAW, Standalone, jedes VST3):
/// `~/.config/FKAudioSuite/common.json` bzw. `%APPDATA%\FKAudioSuite\common.json`. Ein Cubase-Nutzer
/// stellt die Sprache in FK Rig ein, ohne die DAW zu kennen; die DAW sieht dieselbe Wahl.
///
/// Liest bei jedem Zugriff neu (andere Prozesse schreiben mit), schreibt atomar und lässt
/// unbekannte Felder stehen. Datei-I/O: nur Message-Thread.
class SharedSettings final
{
public:
    explicit SharedSettings (juce::File settingsFile);

    [[nodiscard]] static juce::File defaultFile();

    /// Leer, wenn Datei oder Feld fehlt.
    [[nodiscard]] juce::String getValue (const juce::String& key) const;

    core::Result<void> setValue (const juce::String& key, const juce::String& value);

    [[nodiscard]] const juce::File& getFile() const noexcept;

private:
    [[nodiscard]] juce::var load() const;

    juce::File file;
};

} // namespace fk::plugins::i18n
