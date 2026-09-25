#pragma once

#include "GameType.hpp"
#include "Ipc/Ipc.hpp"
#include "ResourceManagerWrapper.hpp"
#include "Factory.hpp"

class FileSystem;
struct CommandLineOptions;
class BaseMap;
class Sys;
class Window;
enum class EReliveLevelIds : s16;

extern u32 sGnFrame;
extern bool gDDCheatOn;
extern u16 gAttract;
extern bool gSkipGameObjectUpdates;
extern s16 gNumCamSwappers;

void DestroyObjects();

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
    void Game_Loop(BaseMap& map);

    // A game frame is split into stages so that a modal object (see BaseMap::GetActiveModal)
    // started part way through can suspend it: the frame carries on from mFrameStage once the
    // modal has finished, keeping the order the original game's nested loops ran things in.
    enum class FrameStage
    {
        eBegin,
        eUpdateObjects,
        eRender,
        eDestroyObjects,
        ePauseMenu,
        eScreenChange,
        eEnd,
    };

    // Runs the frame from mFrameStage on, returning early if a modal suspends it
    void RunFrame(BaseMap& map);

    void Init_Sound_DynamicArrays_And_Others();

    GameType mGameType = GameType::eAe;
    FileSystem& mFs;
    const CommandLineOptions& mOptions;
    std::unique_ptr<relive::IIpcInterface> mIpcInterface;
    // SDL event type the IPC listener sends the editor's path reload requests as
    u32 mPathReloadEventType = 0;
    std::unique_ptr<Window> mWindow;
    std::unique_ptr<Sys> mSys;
    FrameStage mFrameStage = FrameStage::eBegin;
    s32 mFrameObjIdx = 0;
    bool mPauseMenuObjectFound = false;
    // The current loading wait, see Game_Loop
    bool mLoadingWaitStarted = false;
    u32 mLoadingWaitStartTicks = 0;
    bool mLoadingIconShown = false;
    std::unique_ptr<ResourceManagerWrapper> mResMan;
    std::unique_ptr<BaseMap> mMap;
    relive::Factory mFactory;
    std::string mActiveModDisplayName;
};
