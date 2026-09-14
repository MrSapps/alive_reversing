#include "Mods.hpp"
#include "data_conversion/file_system.hpp"
#include "nlohmann/json.hpp"

namespace relive
{
void Mods::EnumerateMods(const std::string& baseDataDirPath)
{
    mEnumeratedMods.clear();

    mFs.EnumerateSubDirectories(baseDataDirPath.c_str(), [&](const char_type* dir, u32)
    {
        FileSystem::Path modInfoPath;
        modInfoPath.Append(baseDataDirPath).Append(dir).Append("modinfo.json");

        const std::string modInfoJson = mFs.LoadToString(modInfoPath);
        if (modInfoJson.empty())
        {
            // Not a mod directory (no modinfo.json) - skip it rather than failing the whole scan.
            return;
        }

        try
        {
            const auto json = nlohmann::json::parse(modInfoJson);

            Mod mod;
            mod.mDirectory = dir;
            mod.mName = json.at("name").get<std::string>();
            mod.mAuthor = json.at("author").get<std::string>();
            mod.mTargetGame = json.at("target_game").get<std::string>();

            mEnumeratedMods.push_back(std::move(mod));
        }
        catch (const nlohmann::json::exception&)
        {
            // Malformed modinfo.json - skip it, same as a missing one.
        }
    });
}

const Mod* Mods::FindByDirOrModName(const std::string& name) const
{
    for (const auto& mod : mEnumeratedMods)
    {
        if (mod.mDirectory == name || mod.mName == name)
        {
            return &mod;
        }
    }
    return nullptr;
}
}
