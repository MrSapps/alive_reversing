#pragma once

#include <optional>
#include <string>
#include <vector>

class FileSystem;

// A minimal ini file: "[Section]" headers followed by "key = value" lines. Sections and
// entries keep their file order (the input bindings are applied in that order), so parsing
// then saving round trips. Lines starting with ';' or '#' are comments and aren't kept. Keys
// before the first header go in a section with an empty name.
class IniFile final
{
public:
    struct Entry final
    {
        std::string mKey;
        std::string mValue;
    };

    struct Section final
    {
        std::string mName;
        std::vector<Entry> mEntries;
    };

    static IniFile Parse(const std::string& text);
    std::string ToString() const;

    // A missing file loads as an empty IniFile.
    static IniFile Load(FileSystem& fs, const std::string& path);
    bool Save(FileSystem& fs, const std::string& path) const;

    const std::vector<Section>& Sections() const
    {
        return mSections;
    }

    const Section* FindSection(const std::string& name) const;

    // The section called name, added at the end if there isn't one.
    Section& GetOrAddSection(const std::string& name);

    std::optional<std::string> GetValue(const std::string& section, const std::string& key) const;

    // true/false for "true"/"false", nullopt for a missing key or any other value.
    std::optional<bool> GetBool(const std::string& section, const std::string& key) const;

    // Replaces the section with the same name (keeping its position), or adds it at the end.
    void ReplaceSection(Section section);

    // Replaces the key's value, or adds the key at the end of the section.
    void SetValue(const std::string& section, const std::string& key, const std::string& value);
    void SetBool(const std::string& section, const std::string& key, bool value);

private:
    std::vector<Section> mSections;
};
