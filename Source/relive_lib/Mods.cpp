#include "Mods.hpp"
#include "data_conversion/file_system.hpp"

namespace relive
{
void Mods::EnumerateMods(const std::string& baseDataDirPath)
{
    // TODO
    /*
    auto dirs = mFs.EnumerateDirectory(baseDataDirPath);
    for (auto& dir : dirs)
    {
        auto modInfoJson = mFs.LoadToString(FileSystem::Path(baseDataDirPath) / dir / "modinfo.json");
        if (!modInfoJson.empty())
        {
            Mod mod;
            mod.mDirectory = dir;

            auto json = nlohmann::json::parse(modInfoJson);
            mod.mName = json["name"].get<std::string>();
            mod.mAuthor = json["author"].get<std::string>();
            mod.mTargetGame = json["target_game"].get<std::string>();

            mEnumeratedMods.push_back(mod);
        }
    }
    */
}

const Mod* Mods::FindByDirOrModName(const std::string& name) const
{
    // TODO
    return nullptr;
}
}