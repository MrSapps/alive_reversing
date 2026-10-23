#include "Mods.hpp"
#include "data_conversion/file_system.hpp"
#include "nlohmann/json.hpp"
#include "FatalError.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cctype>

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

std::string Mods::ActiveModPath(FileSystem& fs, const std::string& modName, GameType gameType, std::string& displayName)
{
    LOG_INFO("Set active mod to be %s", modName.c_str());

    FileSystem::Path modsDir;
    modsDir.Append("relive_data").Append("mods");

    Mods mods(fs);
    mods.EnumerateMods(modsDir.GetPath());

    const Mod* pMod = mods.FindByDirOrModName(modName);
    if (!pMod)
    {
        ALIVE_FATAL("Mod \"%s\" was set as the active mod on the command line but doesn't exist", modName.c_str());
    }

    const std::string expectedTargetGame = (gameType == GameType::eAe) ? "AE" : "AO";
    std::string target = pMod->mTargetGame;
    std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c)
        { return static_cast<char>(std::toupper(c)); });
    if (target != expectedTargetGame)
    {
        ALIVE_FATAL("Mod \"%s\" targets \"%s\" but the active game is \"%s\"", modName.c_str(), pMod->mTargetGame.c_str(), expectedTargetGame.c_str());
    }

    displayName = pMod->mName;
    return modsDir.Append(pMod->mDirectory).GetPath();
}
}
