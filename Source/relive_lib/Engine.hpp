#pragma once

#include "GameType.hpp"
#include "Ipc/Ipc.hpp"
#include "ResourceManagerWrapper.hpp"
#include "Factory.hpp"

class FileSystem;
class CommandLineParser;
class BaseMap;
enum class EReliveLevelIds : s16;

extern u32 sGnFrame;
extern bool gDDCheatOn;
extern u16 gAttract;
extern bool gSkipGameObjectUpdates;
extern s16 gNumCamSwappers;
extern bool gBreakGameLoop;

void DestroyObjects(ResourceManagerWrapper& resMan);

class Engine final
{
public:
    Engine(GameType gameType, FileSystem& fs, CommandLineParser& clp);
    ~Engine();
    void Run();
    static void Init_GameStates();

    // Called once Run() has created the map, e.g. so the auto splitter can point at its fields.
    using TMapCreatedFn = void (*)(BaseMap& map);
    void SetMapCreatedCallback(TMapCreatedFn fn)
    {
        mMapCreatedFn = fn;
    }
private:
    void CmdLineRenderInit(const std::string& activeModName);

    void Game_Run(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);
    void Game_Main(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);

    void Init_Sound_DynamicArrays_And_Others();

    GameType mGameType = GameType::eAe;
    FileSystem& mFs;
    CommandLineParser& mClp;
    std::unique_ptr<relive::IIpcInterface> mIpcInterface;
    std::unique_ptr<ResourceManagerWrapper> mResMan;
    std::unique_ptr<BaseMap> mMap;
    relive::Factory mFactory;
    TMapCreatedFn mMapCreatedFn = nullptr;
};
