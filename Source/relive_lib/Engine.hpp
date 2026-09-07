#pragma once

#include "GameType.hpp"
#include "Ipc/Ipc.hpp"
#include "ResourceManagerWrapper.hpp"

class FileSystem;
class CommandLineParser;
enum class EReliveLevelIds : s16;


extern u32 sGnFrame;
extern bool gDDCheatOn;
extern u16 gAttract;
extern bool gSkipGameObjectUpdates;
extern s16 gNumCamSwappers;
extern bool gBreakGameLoop;

void DestroyObjects();

class Engine final
{
public:
    Engine(GameType gameType, FileSystem& fs, CommandLineParser& clp);
    ~Engine();
    void Run();
    static void Init_GameStates();
private:
    void CmdLineRenderInit();

    void Game_Run(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);
    void Game_Main(EReliveLevelIds startLevel, s32 startPath, s32 startCamera);

    void Init_Sound_DynamicArrays_And_Others();

    GameType mGameType = GameType::eAe;
    FileSystem& mFs;
    CommandLineParser& mClp;
    std::unique_ptr<relive::IIpcInterface> mIpcInterface;
    ResourceManagerWrapper mResMan;
};
