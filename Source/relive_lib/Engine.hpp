#pragma once

#include "GameType.hpp"
#include "Ipc/Ipc.hpp"
#include "ResourceManagerWrapper.hpp"
#include "Factory.hpp"

class FileSystem;
struct CommandLineOptions;
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
    Engine(GameType gameType, FileSystem& fs, const CommandLineOptions& options);
    ~Engine();
    // Loads the active mod and creates the resource manager and map. Call before Run().
    void Init();
    void Run();
    static void Init_GameStates();

    BaseMap& GetMap()
    {
        return *mMap;
    }
private:
    void CmdLineRenderInit(const std::string& activeModName);

    void Game_Run(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);
    void Game_Main(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);

    void Init_Sound_DynamicArrays_And_Others();

    GameType mGameType = GameType::eAe;
    FileSystem& mFs;
    const CommandLineOptions& mOptions;
    std::unique_ptr<relive::IIpcInterface> mIpcInterface;
    std::unique_ptr<ResourceManagerWrapper> mResMan;
    std::unique_ptr<BaseMap> mMap;
    relive::Factory mFactory;
    std::string mActiveModDisplayName;
};
