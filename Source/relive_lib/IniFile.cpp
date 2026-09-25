#include "IniFile.hpp"
#include "data_conversion/file_system.hpp"
#include <sstream>

static std::string Trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

IniFile IniFile::Parse(const std::string& text)
{
    IniFile ini;
    Section* pCurrent = nullptr;

    std::istringstream stream(text);
    std::string rawLine;
    while (std::getline(stream, rawLine))
    {
        const std::string line = Trim(rawLine);
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            ini.mSections.push_back(Section{line.substr(1, line.size() - 2), {}});
            pCurrent = &ini.mSections.back();
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        if (!pCurrent)
        {
            ini.mSections.push_back(Section{});
            pCurrent = &ini.mSections.back();
        }
        pCurrent->mEntries.push_back(Entry{Trim(line.substr(0, equals)), Trim(line.substr(equals + 1))});
    }
    return ini;
}

std::string IniFile::ToString() const
{
    std::string text;
    for (const Section& section : mSections)
    {
        if (!text.empty())
        {
            text += "\n";
        }
        if (!section.mName.empty())
        {
            text += "[" + section.mName + "]\n";
        }
        for (const Entry& entry : section.mEntries)
        {
            text += entry.mKey + " = " + entry.mValue + "\n";
        }
    }
    return text;
}

IniFile IniFile::Load(FileSystem& fs, const std::string& path)
{
    return Parse(fs.LoadToString(path.c_str()));
}

bool IniFile::Save(FileSystem& fs, const std::string& path) const
{
    const std::string text = ToString();
    return fs.Save(path.c_str(), std::vector<u8>(text.begin(), text.end()));
}

const IniFile::Section* IniFile::FindSection(const std::string& name) const
{
    for (const Section& section : mSections)
    {
        if (section.mName == name)
        {
            return &section;
        }
    }
    return nullptr;
}

IniFile::Section& IniFile::GetOrAddSection(const std::string& name)
{
    for (Section& section : mSections)
    {
        if (section.mName == name)
        {
            return section;
        }
    }
    mSections.push_back(Section{name, {}});
    return mSections.back();
}

void IniFile::ReplaceSection(Section section)
{
    for (Section& existing : mSections)
    {
        if (existing.mName == section.mName)
        {
            existing = std::move(section);
            return;
        }
    }
    mSections.push_back(std::move(section));
}

std::optional<std::string> IniFile::GetValue(const std::string& section, const std::string& key) const
{
    if (const Section* pSection = FindSection(section))
    {
        for (const Entry& entry : pSection->mEntries)
        {
            if (entry.mKey == key)
            {
                return entry.mValue;
            }
        }
    }
    return std::nullopt;
}

std::optional<bool> IniFile::GetBool(const std::string& section, const std::string& key) const
{
    const auto value = GetValue(section, key);
    if (value == "true")
    {
        return true;
    }
    if (value == "false")
    {
        return false;
    }
    return std::nullopt;
}

void IniFile::SetValue(const std::string& section, const std::string& key, const std::string& value)
{
    Section& target = GetOrAddSection(section);
    for (Entry& entry : target.mEntries)
    {
        if (entry.mKey == key)
        {
            entry.mValue = value;
            return;
        }
    }
    target.mEntries.push_back(Entry{key, value});
}

void IniFile::SetBool(const std::string& section, const std::string& key, bool value)
{
    SetValue(section, key, value ? "true" : "false");
}
