#include "fk/services/TemplateService.h"
#include "fk/plugins/i18n/Text.h"

#include <algorithm>

namespace fk::services
{

namespace
{
constexpr auto templateExtension = ".tracktionedit";
}

TemplateService::TemplateService (ProjectService& service, juce::File folder)
    : projectService (service)
    , templateFolder (std::move (folder))
{
}

std::vector<juce::File> TemplateService::getTemplates() const
{
    std::vector<juce::File> templates;

    for (const auto& file :
         templateFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + templateExtension))
        templates.push_back (file);

    std::ranges::sort (templates,
                       [] (const juce::File& a, const juce::File& b)
                       { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return templates;
}

core::Result<juce::File> TemplateService::saveAsTemplate (const juce::String& name)
{
    const auto cleanName = juce::File::createLegalFileName (name.trim());

    if (cleanName.isEmpty())
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("The template needs a name.").toStdString());

    const auto file = templateFolder.getChildFile (cleanName + templateExtension);

    if (const auto result = projectService.writeTemplate (file); ! result)
        return std::unexpected (result.error());

    return file;
}

core::Result<void> TemplateService::deleteTemplate (const juce::File& templateFile)
{
    if (! templateFile.isAChildOf (templateFolder))
        return core::fail (core::ErrorCode::invalidArgument, TRANS ("This is not a template.").toStdString());

    if (! templateFile.deleteFile())
        return core::fail (core::ErrorCode::ioError,
                           plugins::i18n::format (TRANS ("Could not delete the template: {path}"), { { "path", templateFile.getFullPathName() } }).toStdString());

    return {};
}

juce::File TemplateService::getTemplateFolder() const
{
    return templateFolder;
}

} // namespace fk::services
