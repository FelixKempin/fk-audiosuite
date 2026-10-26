#include "TranslationAudit.h"

#include <map>
#include <regex>
#include <string>

namespace fk::tests::i18n
{

namespace
{
/// Wie juce::LocalisedStrings: \" \' \t \r \n.
juce::String unescapeTable (const juce::String& text)
{
    return text.replace ("\\\"", "\"").replace ("\\\'", "\'").replace ("\\t", "\t").replace ("\\r", "\r").replace ("\\n", "\n");
}

/// C++-Literal ohne Präfix: \" \' \\ \n \t.
juce::String unescapeCpp (const std::string& text)
{
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '\\' || i + 1 >= text.size())
        {
            result += text[i];
            continue;
        }

        const char next = text[++i];
        result += next == 'n' ? '\n' : next == 't' ? '\t' : next == 'r' ? '\r' : next;
    }
    return juce::String::fromUTF8 (result.c_str(), static_cast<int> (result.size()));
}

/// Position des schließenden Anführungszeichens wie in JUCE (ein \" schließt nicht).
int findCloseQuote (const juce::String& line, int start)
{
    juce::juce_wchar last = 0;
    for (int i = start; i < line.length(); ++i)
    {
        const auto c = line[i];
        if (c == '"' && last != '\\')
            return i;
        last = c;
    }
    return -1;
}

void collectJson (const juce::var& value, const juce::StringArray& fields, juce::StringArray& keys)
{
    if (auto* object = value.getDynamicObject())
    {
        for (const auto& property : object->getProperties())
        {
            if (property.value.isString() && fields.contains (property.name.toString()))
                keys.addIfNotAlreadyThere (property.value.toString());
            else
                collectJson (property.value, fields, keys);
        }
    }
    else if (auto* array = value.getArray())
    {
        for (const auto& item : *array)
            collectJson (item, fields, keys);
    }
}
} // namespace

bool AuditReport::isClean() const
{
    return missing.isEmpty() && unused.isEmpty() && problems.isEmpty();
}

juce::String AuditReport::describe() const
{
    juce::String text;
    for (const auto& key : missing)
        text << "fehlt in der Tabelle: \"" << key << "\"\n";
    for (const auto& key : unused)
        text << "verwaist (nicht im Code): \"" << key << "\"\n";
    for (const auto& problem : problems)
        text << problem << "\n";
    return text;
}

ParsedTable TranslationAudit::parseTable (const juce::String& text, const juce::String& name)
{
    ParsedTable table;
    table.name = name;

    juce::StringArray lines;
    lines.addLines (text);
    std::map<juce::String, int> seen;

    for (int index = 0; index < lines.size(); ++index)
    {
        const auto line = lines[index].trim();
        const int lineNumber = index + 1;

        if (! line.startsWithChar ('"'))
            continue;

        const int keyEnd = findCloseQuote (line, 1);
        const int valueStart = keyEnd < 0 ? -1 : findCloseQuote (line, keyEnd + 1);
        const int valueEnd = valueStart < 0 ? -1 : findCloseQuote (line, valueStart + 1);

        if (valueEnd < 0 || line.substring (keyEnd + 1, valueStart).trim() != "=")
        {
            table.problems.add (name + ":" + juce::String (lineNumber) + ": unlesbare Zeile");
            continue;
        }

        TableEntry entry { unescapeTable (line.substring (1, keyEnd)),
                           unescapeTable (line.substring (valueStart + 1, valueEnd)),
                           lineNumber };

        if (entry.value.isEmpty())
            table.problems.add (name + ":" + juce::String (lineNumber) + ": leere Übersetzung für \"" + entry.key + "\"");

        if (const auto found = seen.find (entry.key); found != seen.end())
            table.problems.add (name + ":" + juce::String (lineNumber) + ": doppelt (zuerst Zeile "
                                + juce::String (found->second) + "): \"" + entry.key + "\"");
        else
            seen.emplace (entry.key, lineNumber);

        table.entries.push_back (std::move (entry));
    }

    return table;
}

ParsedTable TranslationAudit::loadTable (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        ParsedTable table;
        table.name = file.getFullPathName();
        table.problems.add ("Tabelle fehlt: " + file.getFullPathName());
        return table;
    }

    return parseTable (file.loadFileAsString(), file.getFullPathName());
}

juce::String TranslationAudit::stripComments (const juce::String& sourceText)
{
    const auto source = sourceText.toStdString();
    std::string result;
    result.reserve (source.size());

    for (std::size_t i = 0; i < source.size(); ++i)
    {
        const char c = source[i];

        if (c == '"' || c == '\'')
        {
            // Literal unverändert übernehmen, damit "https://..." kein Kommentar wird.
            result += c;
            for (++i; i < source.size(); ++i)
            {
                result += source[i];
                if (source[i] == '\\' && i + 1 < source.size())
                    result += source[++i];
                else if (source[i] == c)
                    break;
            }
            continue;
        }

        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
        {
            while (i < source.size() && source[i] != '\n')
                ++i;
            result += '\n';
            continue;
        }

        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
        {
            const auto end = source.find ("*/", i + 2);
            i = end == std::string::npos ? source.size() : end + 1;
            result += ' ';
            continue;
        }

        result += c;
    }

    return juce::String::fromUTF8 (result.c_str(), static_cast<int> (result.size()));
}

juce::StringArray TranslationAudit::extractKeys (const juce::String& sourceText)
{
    static const std::string literal = R"rx("((?:[^"\\]|\\.)*)")rx";
    static const std::regex single (R"rx(\b(?:TRANS|NEEDS_TRANS)\s*\(\s*(?:juce::String::fromUTF8\s*\(\s*)?)rx" + literal);
    static const std::regex pluralForms (R"rx(\bplural\s*\([^,;()]+(?:\([^()]*\))?[^,;()]*,\s*)rx" + literal + R"rx(\s*,\s*)rx"
                                         + literal);

    const auto source = stripComments (sourceText).toStdString();
    juce::StringArray keys;

    for (std::sregex_iterator it (source.begin(), source.end(), single), end; it != end; ++it)
        keys.addIfNotAlreadyThere (unescapeCpp ((*it)[1].str()));

    for (std::sregex_iterator it (source.begin(), source.end(), pluralForms), end; it != end; ++it)
    {
        keys.addIfNotAlreadyThere (unescapeCpp ((*it)[1].str()));
        keys.addIfNotAlreadyThere (unescapeCpp ((*it)[2].str()));
    }

    return keys;
}

juce::StringArray TranslationAudit::collectSourceKeys (const juce::Array<juce::File>& directories)
{
    juce::StringArray keys;

    for (const auto& directory : directories)
        for (const auto& file : directory.findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
            for (const auto& key : extractKeys (file.loadFileAsString()))
                keys.addIfNotAlreadyThere (key);

    return keys;
}

juce::StringArray TranslationAudit::collectJsonKeys (const juce::File& directory, const juce::StringArray& fields)
{
    juce::StringArray keys;

    for (const auto& file : directory.findChildFiles (juce::File::findFiles, true, "*.json"))
        collectJson (juce::JSON::parse (file), fields, keys);

    return keys;
}

juce::StringArray TranslationAudit::placeholdersOf (const juce::String& text)
{
    static const std::regex placeholder (R"rx(\{[A-Za-z][A-Za-z0-9]*\})rx");
    const auto source = text.toStdString();

    juce::StringArray names;
    for (std::sregex_iterator it (source.begin(), source.end(), placeholder), end; it != end; ++it)
        names.add (juce::String (it->str()));

    names.sort (false);
    return names;
}

AuditReport TranslationAudit::audit (const juce::StringArray& usedKeys,
                                     const ParsedTable& own,
                                     const std::vector<ParsedTable>& inherited)
{
    AuditReport report;
    report.problems.addArray (own.problems);

    juce::StringArray ownKeys;
    for (const auto& entry : own.entries)
    {
        ownKeys.add (entry.key);

        if (placeholdersOf (entry.key) != placeholdersOf (entry.value))
            report.problems.add (own.name + ":" + juce::String (entry.line) + ": Platzhalter weichen ab: \"" + entry.key
                                 + "\"");
    }

    juce::StringArray inheritedKeys;
    for (const auto& table : inherited)
        for (const auto& entry : table.entries)
            inheritedKeys.add (entry.key);

    for (const auto& entry : own.entries)
        if (inheritedKeys.contains (entry.key))
            report.problems.add (own.name + ":" + juce::String (entry.line)
                                 + ": steht schon in einer vorher gesetzten Tabelle: \"" + entry.key + "\"");

    for (const auto& key : usedKeys)
        if (! ownKeys.contains (key) && ! inheritedKeys.contains (key))
            report.missing.add (key);

    for (const auto& key : ownKeys)
        if (! usedKeys.contains (key))
            report.unused.addIfNotAlreadyThere (key);

    report.missing.sort (false);
    report.unused.sort (false);
    return report;
}

} // namespace fk::tests::i18n
