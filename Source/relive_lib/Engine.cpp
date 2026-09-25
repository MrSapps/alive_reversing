#include "Engine.hpp"
#include "GameType.hpp"
#include "data_conversion/data_conversion_ui.hpp"
#include "PsxDisplay.hpp"
#include "BaseGameAutoPlayer.hpp"
#include "Sys.hpp"
#include "Window.hpp"
#include "GameObjects/Particle.hpp"
#include "../AliveLibAE/PsxRender.hpp"

#include "CommandLineOptions.hpp"
#include "Renderer/IRenderer.hpp"
#include "DisplaySettings.hpp"
#include "IniFile.hpp"
#include "Function.hpp"

#include "../relive_lib/Sys.hpp"
#include "../AliveLibAE/Input.hpp"
#include "../relive_lib/Psx.hpp"
#include "../relive_lib/DynamicArray.hpp"
#include "../relive_lib/Sound/Sound.hpp" // for shut down func
#include "AmbientSound.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "../AliveLibAE/Map.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"
#include "../AliveLibAE/PauseMenu.hpp"
#include "../AliveLibAE/GameSpeak.hpp"
#include "../AliveLibAE/DDCheat.hpp"
#include "../relive_lib/Sound/Midi.hpp"
#include <fstream>
#include "../relive_lib/Events.hpp"
#include "../AliveLibAE/Abe.hpp"
#include "../AliveLibAE/MusicController.hpp"
#include "../relive_lib/GameObjects/CheatController.hpp"
#include "../AliveLibAE/Slurg.hpp"
#include "../AliveLibAE/PathDataExtensions.hpp"
#include "../AliveLibAE/GameAutoPlayer.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/GameObjects/ShadowZone.hpp"
#include "../relive_lib/ResourceManagerWrapper.hpp"
#include "../AliveLibAE/GameEnderController.hpp"
#include "../AliveLibAE/ColourfulMeter.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../relive_lib/Collisions.hpp"
#include "../relive_lib/GameObjects/PlatformBase.hpp"


#include "../relive_lib/Function.hpp"
#include "../AliveLibAE/Input.hpp"
#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../AliveLibAO/DDCheat.hpp"
#include "../relive_lib/Psx.hpp"
#include "../relive_lib/Sys.hpp"
#include "../relive_lib/DynamicArray.hpp"
#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "../AliveLibAO/Map.hpp"
#include "../AliveLibAO/GameSpeak.hpp"
#include "../relive_lib/GameObjects/CheatController.hpp"
#include "../AliveLibAO/DDCheat.hpp"
#include "../AliveLibAO/MusicController.hpp"
#include "../AliveLibAO/Input.hpp"
#include "../AliveLibAO/Midi.hpp"
#include "../AliveLibAO/PauseMenu.hpp"
#include "../AliveLibAO/Abe.hpp"
#include "../relive_lib/GameObjects/ShadowZone.hpp"
#include "../AliveLibAO/CameraSwapper.hpp"
#include "AmbientSound.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"
#include "../relive_lib/Events.hpp"
#include "../AliveLibAO/Sound.hpp"
#include "../relive_lib/Engine.hpp"
#include "../AliveLibAO/PathDataExtensions.hpp"
#include "../AliveLibAO/GameAutoPlayer.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "../relive_lib/GameObjects/PlatformBase.hpp"
#include "../AliveLibAO/GameEnderController.hpp"
#include "../relive_lib/Mods.hpp"
#include "data_conversion/file_system.hpp"
#include <FatalError.hpp>

u32 sGnFrame = 0;
s16 gNumCamSwappers = 0;
bool gSkipGameObjectUpdates = false;
bool gDDCheatOn = false;
u16 gAttract = 0;


static bool sCommandLine_ShowFps;

Engine::Engine(GameType gameType, FileSystem& fs, const CommandLineOptions& options)
    : mGameType(gameType)
    , mFs(fs)
    , mOptions(options)
{
    mPathReloadEventType = SDL_RegisterEvents(1);
    LOG_INFO("Event base %d", mPathReloadEventType);

    mIpcInterface = relive::MakeIpcInterface();
    mIpcInterface->Listen([this](relive::PacketTypes packetType, const std::vector<unsigned char>& buffer)
    {
        // Process IPC packets (usually comes from level editor) on this worker thread, send to main
        // thread via an SDL message which will end up in Sys::PumpEvents
        LOG_INFO("On ipc packet type %d len %d", static_cast<u8>(packetType), buffer.size());
        if (packetType == relive::PacketTypes::LevelPathJsonChanged)
        {

            SDL_Event e;
            SDL_zero(e);
            e.type = mPathReloadEventType;
            u8* tmp = new u8[buffer.size()];
            memcpy(tmp, buffer.data(), buffer.size());
            e.user.data1 = tmp;
            e.user.data2 = reinterpret_cast<void*>(buffer.size());
            SDL_PushEvent(&e);
        }
    });
}

Engine::~Engine()
{
    TRACE_ENTRYEXIT;
    mIpcInterface.reset();
    mSys.reset();
    mWindow.reset();
}


static f64 sFps_55EFDC = 0.0;
static s32 sFrameDiff_5CA4DC = 0;
static s32 sFrameCount_5CA300 = 0;

static f64 Calculate_FPS_495250(s32 frameCount)
{
    static u32 sLastTime_5CA338 = SYS_GetTicks() - 500;
    const u32 curTime = SYS_GetTicks();
    const s32 timeDiff = curTime - sLastTime_5CA338;

    if (static_cast<s32>((curTime - sLastTime_5CA338)) < 500)
    {
        return sFps_55EFDC;
    }

    const s32 diffFrames = frameCount - sFrameDiff_5CA4DC;
    sFps_55EFDC = static_cast<f64>(diffFrames) * 1000.0 / static_cast<f64>(timeDiff);

    sLastTime_5CA338 = curTime;
    sFrameDiff_5CA4DC = frameCount;
    return sFps_55EFDC;
}

static void DrawFps_4952F0(f32 fps)
{
    char_type strBuffer[125] = {};
    snprintf(strBuffer, sizeof(strBuffer), "%02.1f fps ", static_cast<f64>(fps));
    gPsxDisplay.mDebugFont.DebugFont_Printf(0, strBuffer);
}


// Called each time a frame is presented
static s32 Game_End_Frame(u32 flags)
{
    if (flags & 1)
    {
        gTurnOffRendering = false;
        return 0;
    }

    const f64 fps = Calculate_FPS_495250(sFrameCount_5CA300);
    if (sCommandLine_ShowFps)
    {
        DrawFps_4952F0(static_cast<f32>(fps));
    }

    ++sFrameCount_5CA300;
    return 0;
}


// So the window never shows whatever was in it before the first frame, and the loading icon
// has something to go on top of if loading comes first
static void PresentBlackFrame()
{
    Poly_G4 black;
    black.SetSemiTransparent(false);
    black.SetShadeTex(false);
    black.SetRGB0(0, 0, 0);
    black.SetRGB1(0, 0, 0);
    black.SetRGB2(0, 0, 0);
    black.SetRGB3(0, 0, 0);
    black.SetXY0(0, 0);
    black.SetXY1(640, 0);
    black.SetXY2(0, 240);
    black.SetXY3(640, 240);

    OrderingTable ot;
    ot.Add(Layer::eLayer_0, &black);
    PSX_DrawOTag(ot);
    PSX_PutDispEnv_4F5890();
}

void Engine::CmdLineRenderInit(const std::string& activeModName)
{
#if FORCE_DDCHEAT
    gDDCheatOn = true;
#else
    gDDCheatOn = mOptions.mDdCheat;
#endif

    // Read directly from disk rather than through GameAutoPlayer::RestoreFileBuffer: display
    // settings don't affect gameplay, and an extra buffer would desync recordings.
    DisplaySettings displaySettings = DisplaySettings::FromIni(mResMan->LoadSettingsIni());

    if (displaySettings.ApplyCommandLine(mOptions))
    {
        // Command line settings are remembered, like the hotkeys'
        IniFile ini = mResMan->LoadSettingsIni();
        displaySettings.WriteTo(ini);
        mResMan->SaveSettingsIni(ini);
    }
    LOG_INFO("Renderer is %s", DisplaySettings::RendererToString(displaySettings.mRenderer));

    std::string windowTitle = mGameType == GameType::eAe ? Window::TitleAE(activeModName) : Window::TitleAO(activeModName);
    if (GetGameAutoPlayer().IsRecording())
    {
        windowTitle += " [Recording]";
    }
    else if (GetGameAutoPlayer().IsPlaying())
    {
        windowTitle += " [AutoPlay]";
    }

    mWindow = std::make_unique<Window>();
    if (!mWindow->CreateWithRenderer(displaySettings.mRenderer, windowTitle))
    {
        ALIVE_FATAL("Failed to create a window and renderer, see the log for details");
    }
    mSys = std::make_unique<Sys>(*mWindow, *mResMan, mPathReloadEventType);
    PresentBlackFrame();
    mSys->SetDisplaySettings(displaySettings);

    PSX_EMU_SetCallBack_4F9430(Game_End_Frame);
}


// QuickSave load/Restart path calls this
void DestroyObjects()
{
    for (s32 iterations = 0; iterations < 2; iterations++)
    {
        for (s32 idx = 0;idx < gBaseGameObjects->Size(); idx++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(idx);
            if (!pObj)
            {
                break;
            }

            if (!pObj->GetSurviveDeathReset())
            {
                idx = gBaseGameObjects->RemoveAt(idx);

                delete pObj;
            }
        }
    }
}

u32 SYS_GetTicks()
{
    // Using this instead of SDL_GetTicks resolves a weird x64 issue on windows where
    // the tick returned is a lot faster on some machines.
    return static_cast<u32>(SDL_GetPerformanceCounter() / (SDL_GetPerformanceFrequency() / 1000));
}

void Alive_Show_ErrorMsg(const char_type* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char_type buf[2048] = {};
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, ("R.E.L.I.V.E. " + Window::BuildString()).c_str(), buf, nullptr);
}

void Engine::Init_GameStates()
{
    if (GetGameType() == GameType::eAo)
    {
        gKilledMudokons = AO::GameEnderController::gRestartRuptureFarmsKilledMuds;
        gRescuedMudokons = AO::GameEnderController::gRestartRuptureFarmsSavedMuds;
    }
    else
    {
        gKilledMudokons = gFeeco_Restart_KilledMudCount;
        gRescuedMudokons = gFeecoRestart_SavedMudCount;
    }

    gDeathGasOn = false; // GasCountDown
    gDeathGasTimer = 0;

    gbDrawMeterCountDown = false; // ColourfulMeter
    gTotalMeterBars = 0;

    gAbeInvincible = false; // Abe

    SwitchStates_ClearRange(0, 255);
}

void Engine::Init_Sound_DynamicArrays_And_Others()
{
    gPauseMenu = nullptr; // PauseMenu
    gAbe = nullptr;
    AO::gPauseMenu = nullptr;
    AO::gAbe = nullptr;
    sControlledCharacter = nullptr;
    gNumCamSwappers = 0; // TODO: Move
    sGnFrame = 0;

    PlatformBase::MakeArray();
    ShadowZone::MakeArray();

    gBaseAliveGameObjects = relive_new DynamicArrayT<BaseAliveGameObject>(20);

    if (mGameType == GameType::eAe)
    {
        SND_Init();
        SND_Init_Ambiance();
        MusicController::Create(*mResMan, *mMap);
    }
    else
    {
        AO::SND_Init();
        SND_Init_Ambiance();
        AO::MusicController::Create(*mResMan, *mMap);
    }
    Init_GameStates(); // Init other vars + switch states

}

static void Game_Init_LoadingIcon()
{
    /*
    u8** ppRes = ResourceManager::GetLoadedResource(ResourceManager::Resource_Animation, AEResourceID::kLoadingResID, 1u, 0);
    if (!ppRes)
    {
        ResourceManager::LoadResourceFile_49C170("LOADING.BAN", nullptr);
        ppRes = ResourceManager::GetLoadedResource(ResourceManager::Resource_Animation, AEResourceID::kLoadingResID, 1u, 0);
    }
    ResourceManager::Set_Header_Flags_49C650(ppRes, ResourceManager::ResourceHeaderFlags::eNeverFree);
    */
}

static void Game_Free_LoadingIcon()
{
    //gLoadingResource.Clear();
    /*
    u8** ppRes = ResourceManager::GetLoadedResource(ResourceManager::Resource_Animation, AEResourceID::kLoadingResID, 0, 0);
    if (ppRes)
    {
        ResourceManager::FreeResource_49C330(ppRes);
    }*/
}


void Game_Shutdown()
{
    Input_DisableInputForPauseMenuAndDebug_4EDDC0();
    GetSoundAPI().mSND_SsQuit();
    IRenderer::FreeRenderer();
}

// Draws the loading icon over whatever was shown last and presents it
static void ShowLoadingIcon(ResourceManagerWrapper& resMan, BaseMap& map)
{
    AnimResource res = resMan.LoadAnimation(AnimId::Loading_Icon2);
    auto pParticle = relive_new Particle(FP_FromInteger(0), FP_FromInteger(0), res, resMan, map);
    pParticle->GetAnimation().SetSemiTrans(false);
    pParticle->GetAnimation().SetBlending(true);
    pParticle->GetAnimation().SetRenderLayer(Layer::eLayer_0);

    OrderingTable ot;
    pParticle->GetAnimation().VRender(320, 220, ot, 0, 0);
    PSX_DrawOTag(ot);
    PSX_PutDispEnv_4F5890();

    pParticle->SetDead(true);
}

// One main loop iteration spent waiting for loading to finish
static Sys::PumpResult LoadingTick(Sys& sys, ResourceManagerWrapper& resMan, BaseMap& map, bool showLoadingIcon)
{
    // How many of these there are depends on how long loading takes, so none of it is recorded
    GetGameAutoPlayer().DisableRecorder();

    const Sys::PumpResult result = sys.PumpEvents(nullptr);

    // If not uncapped fps playback then actually wait for 1 frame on each tick
    const bool unCappedFps = GetGameAutoPlayer().IsPlaying() && GetGameAutoPlayer().NoFpsLimitPlayBack();
    PSX_VSync(unCappedFps ? VSyncMode::UncappedFps : VSyncMode::LimitTo30Fps);

    if (showLoadingIcon)
    {
        ShowLoadingIcon(resMan, map);
    }

    GetGameAutoPlayer().EnableRecorder();
    return result;
}

void Engine::Game_Loop(BaseMap& map)
{
    mFrameStage = FrameStage::eBegin;
    mPauseMenuObjectFound = false;
    while (!gBaseGameObjects->IsEmpty())
    {
        // Anything the resource manager loads can load in the background, and this is the only
        // place that waits for it: whatever needs it asks for a wait (RequestLoadingWait) and
        // carries on once it's done, e.g. a screen change returns ScreenChangeResult::eWaiting.
        if (mResMan->LoadingWaitRequested())
        {
            if (!mLoadingWaitStarted)
            {
                mLoadingWaitStarted = true;
                mLoadingWaitStartTicks = SYS_GetTicks();
                mLoadingIconShown = false;
            }

            constexpr u32 kShowLoadingIconAfterMs = 1000;
            const bool showLoadingIcon = !mLoadingIconShown
                && (mResMan->ShowLoadingIconNow() || SYS_GetTicks() > mLoadingWaitStartTicks + kShowLoadingIconAfterMs);

            if (mResMan->IsLoading() || showLoadingIcon)
            {
                if (LoadingTick(*mSys, *mResMan, map, showLoadingIcon) == Sys::PumpResult::eQuit)
                {
                    map.EndAllModals();
                    break;
                }
                mLoadingIconShown = mLoadingIconShown || showLoadingIcon;
                continue;
            }

            mResMan->EndLoadingWait();
            mLoadingWaitStarted = false;
        }

        // The only other place events are pumped while the game runs. Modal objects (pause
        // menu, movies...) run from here too, rather than in nested loops of their own. The
        // nested loops never gave the pump the map, so restarting the music after the quit
        // question still waits until no modal is active.
        if (mSys->PumpEvents(map.GetActiveModal() ? nullptr : &map) == Sys::PumpResult::eQuit)
        {
            // Quit confirmed: abandon whatever was running and shut down normally
            map.EndAllModals();
            break;
        }

        if (BaseGameObject* pModal = map.GetActiveModal())
        {
            const ModalState state = pModal->VModalUpdate();
            if (state == ModalState::eQuitGame)
            {
                GetGameAutoPlayer().SyncPoint(SyncPoints::MainLoopExit);
                map.EndAllModals();
                break;
            }

            if (state == ModalState::eFinished)
            {
                pModal->EndModal();
            }
            continue;
        }

        // Only between frames, so a reload can't pull objects out from under one
        if (mFrameStage == FrameStage::eBegin && !map.DirectCameraChangePending())
        {
            for (const std::string& pathJsonFileName : mSys->TakePathReloadRequests())
            {
                map.ReloadPathJsonRequest(pathJsonFileName);
            }
        }

        RunFrame(map);
    } // Main loop end

    PSX_VSync(VSyncMode::UncappedFps);

    // Destroy all game objects
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObjToKill = gBaseGameObjects->ItemAt(i);
        if (!pObjToKill)
        {
            break;
        }

        if (pObjToKill->GetDead())
        {
            i = gBaseGameObjects->RemoveAt(i);
            relive_delete pObjToKill;
        }
    }
}

void Engine::RunFrame(BaseMap& map)
{
    // Every stage that can start a modal object returns once one has, having already moved
    // mFrameStage/mFrameObjIdx on to where the frame carries on from.
    if (mFrameStage == FrameStage::eBegin && map.DirectCameraChangePending())
    {
        // The first camera, or a path reload's, see BaseMap::ContinueDirectCameraChange
        if (map.ContinueDirectCameraChange() == ScreenChangeResult::eWaiting)
        {
            return;
        }
    }

    if (mFrameStage == FrameStage::eBegin)
    {
        GetGameAutoPlayer().SyncPoint(SyncPoints::MainLoopStart);

        EventsResetActive();
        Slurg::Clear_Slurg_Step_Watch_Points();
        gSkipGameObjectUpdates = false; // Used by quick save

        // Update objects
        GetGameAutoPlayer().SyncPoint(SyncPoints::ObjectsUpdateStart);
        mFrameObjIdx = 0;
        mFrameStage = FrameStage::eUpdateObjects;
    }

    if (mFrameStage == FrameStage::eUpdateObjects)
    {
        for (; mFrameObjIdx < gBaseGameObjects->Size(); mFrameObjIdx++)
        {
            BaseGameObject* pBaseGameObject = gBaseGameObjects->ItemAt(mFrameObjIdx);

            if (!pBaseGameObject || gSkipGameObjectUpdates)
            {
                break;
            }

            if (pBaseGameObject->GetUpdatable()
                && !pBaseGameObject->GetDead()
                && (gNumCamSwappers == 0 || pBaseGameObject->GetUpdateDuringCamSwap()))
            {
                const s32 updateDelay = pBaseGameObject->UpdateDelay();
                if (updateDelay <= 0)
                {
                    if (pBaseGameObject == gPauseMenu)
                    {
                        mPauseMenuObjectFound = true;
                    }
                    else
                    {
                        pBaseGameObject->VUpdate();
                        if (map.GetActiveModal())
                        {
                            mFrameObjIdx++;
                            return;
                        }
                    }
                }
                else
                {
                    pBaseGameObject->SetUpdateDelay(updateDelay - 1);
                }
            }
        }
        GetGameAutoPlayer().SyncPoint(SyncPoints::ObjectsUpdateEnd);
        mFrameStage = FrameStage::eRender;
    }

    if (mFrameStage == FrameStage::eRender)
    {
        // Animate everything
        if (gNumCamSwappers <= 0)
        {
            GetGameAutoPlayer().SyncPoint(SyncPoints::AnimateAll);
            AnimationBase::AnimateAll(AnimationBase::gAnimations);
        }

        // Render objects
        GetGameAutoPlayer().SyncPoint(SyncPoints::DrawAllStart);
        for (s32 i = 0; i < gObjListDrawables->Size(); i++)
        {
            BaseGameObject* pDrawable = gObjListDrawables->ItemAt(i);
            if (!pDrawable)
            {
                break;
            }

            if (pDrawable->GetDead())
            {
                pDrawable->SetCantKill(false);
            }
            else if (pDrawable->GetDrawable())
            {
                pDrawable->SetCantKill(true);
                pDrawable->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);
            }
        }
        GetGameAutoPlayer().SyncPoint(SyncPoints::DrawAllEnd);

        gPsxDisplay.mDebugFont.DebugFont_Flush();
        gScreenManager->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);

        GetGameAutoPlayer().SyncPoint(SyncPoints::RenderOT);
        gPsxDisplay.RenderOrderingTable();

        GetGameAutoPlayer().SyncPoint(SyncPoints::RenderStart);
        mFrameObjIdx = 0;
        mFrameStage = FrameStage::eDestroyObjects;
    }

    if (mFrameStage == FrameStage::eDestroyObjects)
    {
        // Destroy objects with certain flags
        for (; mFrameObjIdx < gBaseGameObjects->Size(); mFrameObjIdx++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(mFrameObjIdx);
            if (!pObj)
            {
                break;
            }

            if (pObj->GetDead() && !pObj->GetCantKill() && pObj->mChaseCounter == 0)
            {
                mFrameObjIdx = gBaseGameObjects->RemoveAt(mFrameObjIdx);
                relive_delete pObj;

                // e.g. a camera swapper finishing with the purple light effect
                if (map.GetActiveModal())
                {
                    mFrameObjIdx++;
                    return;
                }
            }
        }

        GetGameAutoPlayer().SyncPoint(SyncPoints::RenderEnd);
        mFrameStage = FrameStage::ePauseMenu;
    }

    if (mFrameStage == FrameStage::ePauseMenu)
    {
        mFrameStage = FrameStage::eScreenChange;

        const bool bPauseMenuObjectFound = mPauseMenuObjectFound;
        mPauseMenuObjectFound = false;
        if (bPauseMenuObjectFound && gPauseMenu)
        {
            gPauseMenu->VUpdate();
            if (map.GetActiveModal())
            {
                return;
            }
        }
    }

    if (mFrameStage == FrameStage::eScreenChange)
    {
        if (map.ScreenChange() == ScreenChangeResult::eWaiting)
        {
            // Waiting on a modal or loading part way through, called again to carry on once
            // that's done
            return;
        }

        mFrameStage = FrameStage::eEnd;
        if (map.GetActiveModal())
        {
            return;
        }
    }

    mFrameStage = FrameStage::eBegin;

    if (GetGameType() == GameType::eAe)
    {
        Input().Update(GetGameAutoPlayer());
    }
    else
    {
        AO::Input().Update(GetGameAutoPlayer());
    }

    if (gNumCamSwappers == 0)
    {
        GetGameAutoPlayer().SyncPoint(SyncPoints::IncrementFrame);
        sGnFrame++;
    }

    GetGameAutoPlayer().ValidateObjectStates();
}

void Engine::Game_Run(EReliveLevelIds startLevel, s32 startPath, s32 startCamera)
{
    // Begin start up
    if (mSys->PumpEvents(mMap.get()) == Sys::PumpResult::eQuit)
    {
        return;
    }

    // Needed while waiting for everything else to load, starting with the first camera
    mResMan->PendAnimation(AnimId::Loading_Icon2);

    gAttract = 0;
 
    AO::Input().InitPad(1);

    gBaseGameObjects = relive_new DynamicArrayT<BaseGameObject>(90);

    BaseAnimatedWithPhysicsGameObject::MakeArray(); // Makes drawables

    AnimationBase::CreateAnimationArray();

    if (mGameType == GameType::eAe)
    {
        Input_Init(*mResMan);
    }
    else
    {
        AO::Input_Init(*mResMan);
    }

    Init_Sound_DynamicArrays_And_Others();

    if (mGameType == GameType::eAe)
    {
        relive_new DDCheat(*mResMan, *mMap);
        gEventSystem = relive_new GameSpeak(*mResMan, *mMap);
    }
    else
    {
        relive_new AO::DDCheat(*mResMan, *mMap);
        AO::gEventSystem = relive_new AO::GameSpeak(*mResMan, *mMap);
    }
    gCheatController = relive_new CheatController(*mResMan, *mMap);

    Game_Init_LoadingIcon();

    mMap->Init(startLevel, static_cast<s16>(startPath), static_cast<s16>(startCamera), CameraSwapEffects::eInstantChange_0, {}, 0);

    // Main loop start
    Game_Loop(*mMap);

    // Shut down start
    Game_Free_LoadingIcon();

    mMap->Shutdown();
    mMap.reset();

    if (mGameType == GameType::eAe)
    {
        DDCheat::ClearProperties();
    }
    else
    {
        AO::DDCheat::ClearProperties();
    }

    AnimationBase::FreeAnimationArray();
    BaseAnimatedWithPhysicsGameObject::FreeArray();
    relive_delete gBaseGameObjects;
    PlatformBase::FreeArray();
    ShadowZone::FreeArray();
    relive_delete gBaseAliveGameObjects;
    relive_delete gCollisions;

    if (mGameType == GameType::eAe)
    {
        MusicController::Shutdown();
    }
    else
    {
        AO::MusicController::Shutdown();
    }

    SND_Reset_Ambiance();
    SND_Shutdown();
    Input().ShutDown_45F020();
}

void Engine::Game_Main(EReliveLevelIds startLevel, s32 startPath, s32 startCamera)
{
    // Only returns once the engine is shutting down
    Game_Run(startLevel, startPath, startCamera);

    Game_Shutdown();
}

void Engine::Init()
{
    std::string activeModPath;

    const std::string& modName = mOptions.mModName;
    if (!modName.empty())
    {
        LOG_INFO("Set active mod to be %s", modName.c_str());

        FileSystem::Path modsDir;
        modsDir.Append("relive_data").Append("mods");

        relive::Mods mods(mFs);
        mods.EnumerateMods(modsDir.GetPath());

        const relive::Mod* pMod = mods.FindByDirOrModName(modName);
        if (!pMod)
        {
            ALIVE_FATAL("Mod \"%s\" was set as the active mod on the command line but doesn't exist", modName.c_str());
        }

        const char_type* const expectedTargetGame = (mGameType == GameType::eAe) ? "AE" : "AO";
        if (strcmpi(pMod->mTargetGame.c_str(), expectedTargetGame) != 0)
        {
            ALIVE_FATAL("Mod \"%s\" targets \"%s\" but the active game is \"%s\"", modName.c_str(), pMod->mTargetGame.c_str(), expectedTargetGame);
        }

        mActiveModDisplayName = pMod->mName;
        activeModPath = modsDir.Append(pMod->mDirectory).GetPath();
    }

    mResMan = std::make_unique<ResourceManagerWrapper>(mFs, activeModPath);
    mResMan->SetDebugLoadDelay(mOptions.mSlowLoadMs);

    gPsxDisplay.Init(*mResMan);

    if (mGameType == GameType::eAe)
    {
        mMap = std::make_unique<Map>(*mResMan, mFactory);
    }
    else
    {
        mMap = std::make_unique<AO::Map>(*mResMan, mFactory);
    }
}

void Engine::Run()
{
    GetGameAutoPlayer().ProcessCommandLine(mFs, mOptions);

    sCommandLine_ShowFps = mOptions.mShowFps;
    gCommandLine_NoFrameSkip = mOptions.mNoFrameSkip;

    CmdLineRenderInit(mActiveModDisplayName);

    // Another hack till refactor branch replaces master
    GetGameAutoPlayer().Pause(true);
    GetGameAutoPlayer().DisableRecorder();

    // TODO: HACK mini loop till Game.cpp is merged
    DataConversionUI dcu(mGameType, *mResMan, *mMap);
    if (dcu.ConversionRequired())
    {
        do
        {
            dcu.VUpdate();

            dcu.VRender(gPsxDisplay.mDrawEnv.mOrderingTable);

            // dcu (and so any FMV conversion jobs it dispatched onto its own ThreadPool - see
            // fmv_converter.cpp) is only ever reachable from right here, so quitting while it's
            // still running is handled locally: ask it to cancel and give it a bounded window to
            // actually stop (FmvConv::Convert checks ThreadPool::IsCancelRequested() roughly once
            // per encoded frame and cleans up its own temp file) before exiting for real either
            // way, rather than leaving conversion jobs mid-write.
            if (mSys->PumpEvents(mMap.get()) == Sys::PumpResult::eQuit)
            {
                dcu.RequestCancel();

                constexpr u32 kMaxCancelWaitMs = 2000;
                const u32 waitStartTicks = SYS_GetTicks();
                while (dcu.AsyncTasksInProgress() && (SYS_GetTicks() - waitStartTicks) < kMaxCancelWaitMs)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                return;
            }
            gPsxDisplay.RenderOrderingTable();
        }
        while (!dcu.GetDead());
    }
    else
    {
        LOG_INFO("Data is up to date, skip conversion");
    }

    GetGameAutoPlayer().Pause(false);
    GetGameAutoPlayer().EnableRecorder();

    if (mGameType == GameType::eAe)
    {
        LOG_INFO("AE standalone starting...");
        //Game_Main(EReliveLevelIds::eMines, 1, 4);
        Game_Main(EReliveLevelIds::eMenu, 1, 25);
    }
    else
    {
        LOG_INFO("AO standalone starting...");
        Game_Main(EReliveLevelIds::eMenu, 1, 10);
    }
}
