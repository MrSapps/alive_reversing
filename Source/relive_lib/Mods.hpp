#pragma once

#include <string>

class FileSystem;

namespace relive
{
class Mod final
{
public:
    // The directory name under the mods dir where this mod lives
    std::string mDirectory;

    // Info from the mods modinfo.json file

    // Name of the mod - shown in the window title
    std::string mName;

    // Name of the person/team that made the mod
    std::string mAuthor;

    // Is this mod for AO or AE?
    std::string mTargetGame;
};

class Mods final
{
public:
    explicit Mods(FileSystem& fs)
     : mFs(fs)
    {

    }    
    void EnumerateMods(const std::string& baseDataDirPath);
    const Mod* FindByDirOrModName(const std::string& name) const;
private:
    std::vector<Mod> mEnumeratedMods;
    FileSystem& mFs;
};
}
