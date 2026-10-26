#pragma once

#include "fk/core/Result.h"
#include "fk/services/ProjectService.h"

#include <vector>

namespace fk::services
{

/// Projektvorlagen: ein Ordner mit .tracktionedit-Dateien, eine je Vorlage, Dateiname = Name.
/// Was in einer Vorlage steckt, entscheidet ProjectService::writeTemplate.
class TemplateService final
{
public:
    /// templateFolder: z. B. ~/.config/FKAudioSuite/Vorlagen; Tests geben einen Temp-Ordner.
    TemplateService (ProjectService& projectService, juce::File templateFolder);

    /// Alle Vorlagen, nach Name sortiert.
    [[nodiscard]] std::vector<juce::File> getTemplates() const;

    /// Speichert das offene Projekt als Vorlage; eine gleichnamige wird ersetzt.
    [[nodiscard]] core::Result<juce::File> saveAsTemplate (const juce::String& name);

    [[nodiscard]] core::Result<void> deleteTemplate (const juce::File& templateFile);

    [[nodiscard]] juce::File getTemplateFolder() const;

private:
    ProjectService& projectService;
    juce::File templateFolder;
};

} // namespace fk::services
