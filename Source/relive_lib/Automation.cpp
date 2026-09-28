#include "Automation.hpp"
#include "BaseMap.hpp"
#include "Engine.hpp"
#include "GameType.hpp"
#include "Movie.hpp"
#include "ResourceManagerWrapper.hpp"
#include "logger.hpp"
#include "Renderer/IRenderer.hpp"
#include "GameObjects/BaseAliveGameObject.hpp"
#include "data_conversion/AnimConversionInfo.hpp"
#include "data_conversion/EnumSerialization.hpp"
#include "data_conversion/PNGFile.hpp"
#include "data_conversion/file_system.hpp"
#include "../AliveLibAE/Abe.hpp"
#include "../AliveLibAE/MainMenu.hpp"
#include "../AliveLibAE/PauseMenu.hpp"
#include "../AliveLibAO/Abe.hpp"
#include "../AliveLibAO/MainMenu.hpp"
#include "../AliveLibAO/PauseMenu.hpp"
#include <SDL3/SDL_timer.h>
#include <sstream>

// No default, so a type added to ReliveTypes without a name here is a warning
static const char_type* TypeName(ReliveTypes type)
{
    switch (type)
    {
        case ReliveTypes::eNone: return "eNone";
        case ReliveTypes::eCrawlingSligButton: return "eCrawlingSligButton";
        case ReliveTypes::eWheelSyncer: return "eWheelSyncer";
        case ReliveTypes::eDemoSpawnPoint: return "eDemoSpawnPoint";
        case ReliveTypes::eMultiSwitchController: return "eMultiSwitchController";
        case ReliveTypes::eStatusLight: return "eStatusLight";
        case ReliveTypes::eSlapLock: return "eSlapLock";
        case ReliveTypes::eParamiteWebLine: return "eParamiteWebLine";
        case ReliveTypes::eGlukkonSwitch: return "eGlukkonSwitch";
        case ReliveTypes::eDoorBlocker: return "eDoorBlocker";
        case ReliveTypes::eTrainDoor: return "eTrainDoor";
        case ReliveTypes::eLevelLoader: return "eLevelLoader";
        case ReliveTypes::eSligGetWings: return "eSligGetWings";
        case ReliveTypes::eSligGetPants: return "eSligGetPants";
        case ReliveTypes::eTeleporter: return "eTeleporter";
        case ReliveTypes::eWater: return "eWater";
        case ReliveTypes::eWorkWheel: return "eWorkWheel";
        case ReliveTypes::eLCDScreen: return "eLCDScreen";
        case ReliveTypes::eInvisibleSwitch: return "eInvisibleSwitch";
        case ReliveTypes::eDoorFlame: return "eDoorFlame";
        case ReliveTypes::eMovingBomb: return "eMovingBomb";
        case ReliveTypes::eMainMenuController: return "eMainMenuController";
        case ReliveTypes::eHintFly: return "eHintFly";
        case ReliveTypes::eSecurityDoor: return "eSecurityDoor";
        case ReliveTypes::eCreditsController: return "eCreditsController";
        case ReliveTypes::eLCDStatusBoard: return "eLCDStatusBoard";
        case ReliveTypes::eSwitchStateBooleanLogic: return "eSwitchStateBooleanLogic";
        case ReliveTypes::eLightEffect: return "eLightEffect";
        case ReliveTypes::eSlogSpawner: return "eSlogSpawner";
        case ReliveTypes::eGasEmitter: return "eGasEmitter";
        case ReliveTypes::eRingCancel: return "eRingCancel";
        case ReliveTypes::eElumWall: return "eElumWall";
        case ReliveTypes::eAbeStart: return "eAbeStart";
        case ReliveTypes::eBeeSwarmHole: return "eBeeSwarmHole";
        case ReliveTypes::eFallingItem: return "eFallingItem";
        case ReliveTypes::eShadowZone: return "eShadowZone";
        case ReliveTypes::eStartController: return "eStartController";
        case ReliveTypes::eBirdPortalExit: return "eBirdPortalExit";
        case ReliveTypes::eHoneyDripTarget: return "eHoneyDripTarget";
        case ReliveTypes::ePathTransition: return "ePathTransition";
        case ReliveTypes::eZSligCover: return "eZSligCover";
        case ReliveTypes::eResetPath: return "eResetPath";
        case ReliveTypes::eElumPathTrans: return "eElumPathTrans";
        case ReliveTypes::eScrabBoundLeft: return "eScrabBoundLeft";
        case ReliveTypes::eScrabBoundRight: return "eScrabBoundRight";
        case ReliveTypes::eScrabNoFall: return "eScrabNoFall";
        case ReliveTypes::eMovingBombStopper: return "eMovingBombStopper";
        case ReliveTypes::eElumStart: return "eElumStart";
        case ReliveTypes::eEdge: return "eEdge";
        case ReliveTypes::eSoftLanding: return "eSoftLanding";
        case ReliveTypes::eMovieHandStone: return "eMovieHandStone";
        case ReliveTypes::eBellSongStone: return "eBellSongStone";
        case ReliveTypes::eDemoPlaybackStone: return "eDemoPlaybackStone";
        case ReliveTypes::eHandStone: return "eHandStone";
        case ReliveTypes::eHoist: return "eHoist";
        case ReliveTypes::eContinuePoint: return "eContinuePoint";
        case ReliveTypes::eWellLocal: return "eWellLocal";
        case ReliveTypes::eWellExpress: return "eWellExpress";
        case ReliveTypes::eMudokonPathTrans: return "eMudokonPathTrans";
        case ReliveTypes::eRingMudokon: return "eRingMudokon";
        case ReliveTypes::eLiftMudokon: return "eLiftMudokon";
        case ReliveTypes::eInvisibleZone: return "eInvisibleZone";
        case ReliveTypes::eEnemyStopper: return "eEnemyStopper";
        case ReliveTypes::eSligBoundLeft: return "eSligBoundLeft";
        case ReliveTypes::eSligBoundRight: return "eSligBoundRight";
        case ReliveTypes::eSligPersist: return "eSligPersist";
        case ReliveTypes::eZzzSpawner: return "eZzzSpawner";
        case ReliveTypes::eKillUnsavedMuds: return "eKillUnsavedMuds";
        case ReliveTypes::eDeathDrop: return "eDeathDrop";
        case ReliveTypes::eAlarm: return "eAlarm";
        case ReliveTypes::eScreenManager: return "eScreenManager";
        case ReliveTypes::eBackgroundAnimation: return "eBackgroundAnimation";
        case ReliveTypes::eBat: return "eBat";
        case ReliveTypes::eLiftMover: return "eLiftMover";
        case ReliveTypes::eTimedMine: return "eTimedMine";
        case ReliveTypes::eBullet: return "eBullet";
        case ReliveTypes::eDDCheat: return "eDDCheat";
        case ReliveTypes::eBells: return "eBells";
        case ReliveTypes::eChimeLock: return "eChimeLock";
        case ReliveTypes::eGasCountDown: return "eGasCountDown";
        case ReliveTypes::eParticleBurst: return "eParticleBurst";
        case ReliveTypes::eDoor: return "eDoor";
        case ReliveTypes::eGameSpeak: return "eGameSpeak";
        case ReliveTypes::eElectricWall: return "eElectricWall";
        case ReliveTypes::eElum: return "eElum";
        case ReliveTypes::eBellHammer: return "eBellHammer";
        case ReliveTypes::ePalOverwriter: return "ePalOverwriter";
        case ReliveTypes::eGroundExplosion: return "eGroundExplosion";
        case ReliveTypes::eSecurityClaw: return "eSecurityClaw";
        case ReliveTypes::eRockSpawner: return "eRockSpawner";
        case ReliveTypes::eFlintLockFire: return "eFlintLockFire";
        case ReliveTypes::eThrowableTotalIndicator: return "eThrowableTotalIndicator";
        case ReliveTypes::eFootSwitch: return "eFootSwitch";
        case ReliveTypes::eGameEnderController: return "eGameEnderController";
        case ReliveTypes::eDeathBird: return "eDeathBird";
        case ReliveTypes::eLoadingFile: return "eLoadingFile";
        case ReliveTypes::eGrenade: return "eGrenade";
        case ReliveTypes::eBoomMachine: return "eBoomMachine";
        case ReliveTypes::eBackgroundGlukkon: return "eBackgroundGlukkon";
        case ReliveTypes::eAbe: return "eAbe";
        case ReliveTypes::MainMenuFade: return "MainMenuFade";
        case ReliveTypes::eHoneySack: return "eHoneySack";
        case ReliveTypes::eHoney: return "eHoney";
        case ReliveTypes::eClawOrBirdPortalTerminator: return "eClawOrBirdPortalTerminator";
        case ReliveTypes::eMudokon: return "eMudokon";
        case ReliveTypes::eLiftPoint: return "eLiftPoint";
        case ReliveTypes::eMeat: return "eMeat";
        case ReliveTypes::eMeatSack: return "eMeatSack";
        case ReliveTypes::eMeatSaw: return "eMeatSaw";
        case ReliveTypes::eMine: return "eMine";
        case ReliveTypes::eRollingBallStopperShaker: return "eRollingBallStopperShaker";
        case ReliveTypes::eMotionDetector: return "eMotionDetector";
        case ReliveTypes::eRollingBallStopper: return "eRollingBallStopper";
        case ReliveTypes::ePauseMenu: return "ePauseMenu";
        case ReliveTypes::eParamite: return "eParamite";
        case ReliveTypes::eDemoPlayback: return "eDemoPlayback";
        case ReliveTypes::eBirdPortal: return "eBirdPortal";
        case ReliveTypes::eBirdPortalTerminator: return "eBirdPortalTerminator";
        case ReliveTypes::eFG1: return "eFG1";
        case ReliveTypes::eAbilityRing: return "eAbilityRing";
        case ReliveTypes::eRock: return "eRock";
        case ReliveTypes::eRockSack: return "eRockSack";
        case ReliveTypes::eRollingBall: return "eRollingBall";
        case ReliveTypes::eRope: return "eRope";
        case ReliveTypes::eAirExplosion: return "eAirExplosion";
        case ReliveTypes::eRedLaser: return "eRedLaser";
        case ReliveTypes::eScrab: return "eScrab";
        case ReliveTypes::eScreenClipper: return "eScreenClipper";
        case ReliveTypes::eEffectBase: return "eEffectBase";
        case ReliveTypes::eFade: return "eFade";
        case ReliveTypes::eFlash: return "eFlash";
        case ReliveTypes::eScreenWave: return "eScreenWave";
        case ReliveTypes::eUnknown: return "eUnknown";
        case ReliveTypes::eShrykull: return "eShrykull";
        case ReliveTypes::eSlig: return "eSlig";
        case ReliveTypes::eSlog: return "eSlog";
        case ReliveTypes::SlingMud: return "SlingMud";
        case ReliveTypes::eSligSpawner: return "eSligSpawner";
        case ReliveTypes::eZBall: return "eZBall";
        case ReliveTypes::eParticle: return "eParticle";
        case ReliveTypes::eZapLine: return "eZapLine";
        case ReliveTypes::eBeeSwarm: return "eBeeSwarm";
        case ReliveTypes::eBeeNest: return "eBeeNest";
        case ReliveTypes::eLever: return "eLever";
        case ReliveTypes::eTrapDoor: return "eTrapDoor";
        case ReliveTypes::eUXB: return "eUXB";
        case ReliveTypes::eMovie: return "eMovie";
        case ReliveTypes::eCameraSwapper: return "eCameraSwapper";
        case ReliveTypes::eElectrocute: return "eElectrocute";
        case ReliveTypes::eTimedMine_or_MovingBomb: return "eTimedMine_or_MovingBomb";
        case ReliveTypes::eBone: return "eBone";
        case ReliveTypes::eBoneBag: return "eBoneBag";
        case ReliveTypes::eBrewMachine: return "eBrewMachine";
        case ReliveTypes::eSligButton: return "eSligButton";
        case ReliveTypes::eExplosionSet: return "eExplosionSet";
        case ReliveTypes::eZapSpark: return "eZapSpark";
        case ReliveTypes::eMetal: return "eMetal";
        case ReliveTypes::eMinesAlarm: return "eMinesAlarm";
        case ReliveTypes::eCrawlingSlig: return "eCrawlingSlig";
        case ReliveTypes::eDrill: return "eDrill";
        case ReliveTypes::eLaughingGas: return "eLaughingGas";
        case ReliveTypes::eDoorLock: return "eDoorLock";
        case ReliveTypes::eDove: return "eDove";
        case ReliveTypes::eEvilFart: return "eEvilFart";
        case ReliveTypes::eFleech: return "eFleech";
        case ReliveTypes::ePossessionFlicker: return "ePossessionFlicker";
        case ReliveTypes::eFlyingSlig: return "eFlyingSlig";
        case ReliveTypes::eFlyingSligSpawner: return "eFlyingSligSpawner";
        case ReliveTypes::eColourfulMeter: return "eColourfulMeter";
        case ReliveTypes::eSlapLock_OrbWhirlWind: return "eSlapLock_OrbWhirlWind";
        case ReliveTypes::eGreeter: return "eGreeter";
        case ReliveTypes::eGlukkon: return "eGlukkon";
        case ReliveTypes::eHelpPhone: return "eHelpPhone";
        case ReliveTypes::eEyeOrbPart: return "eEyeOrbPart";
        case ReliveTypes::eInvisibleEffect: return "eInvisibleEffect";
        case ReliveTypes::ePulley: return "ePulley";
        case ReliveTypes::eResourceManager: return "eResourceManager";
        case ReliveTypes::eSligGetPantsOrWings: return "eSligGetPantsOrWings";
        case ReliveTypes::eRingOrLiftMud: return "eRingOrLiftMud";
        case ReliveTypes::eSecurityOrb: return "eSecurityOrb";
        case ReliveTypes::eText: return "eText";
        case ReliveTypes::eMineCar: return "eMineCar";
        case ReliveTypes::eGreeterBody: return "eGreeterBody";
        case ReliveTypes::eMusicController: return "eMusicController";
        case ReliveTypes::eMusicTrigger: return "eMusicTrigger";
        case ReliveTypes::ePullRingRope: return "ePullRingRope";
        case ReliveTypes::eScrabSpawner: return "eScrabSpawner";
        case ReliveTypes::eMainMenuTransistion: return "eMainMenuTransistion";
        case ReliveTypes::eScreenShake: return "eScreenShake";
        case ReliveTypes::eSlamDoor: return "eSlamDoor";
        case ReliveTypes::eSnoozeParticle: return "eSnoozeParticle";
        case ReliveTypes::eSlurgSpawner: return "eSlurgSpawner";
        case ReliveTypes::eSlurg: return "eSlurg";
        case ReliveTypes::eTimerTrigger: return "eTimerTrigger";
        case ReliveTypes::eTorturedMud: return "eTorturedMud";
        case ReliveTypes::eWebLine: return "eWebLine";
        case ReliveTypes::eWell: return "eWell";
        case ReliveTypes::eThrowableArray: return "eThrowableArray";
        case ReliveTypes::eDoorLight: return "eDoorLight";
        case ReliveTypes::eHoistParticle: return "eHoistParticle";
    }
    return "unknown";
}

static std::string LevelName(EReliveLevelIds level)
{
    return nlohmann::json(level).get<std::string>();
}

// eNone if name isn't a level: the JSON conversion gives unknown names the first entry's value
static EReliveLevelIds LevelFromName(const std::string& name)
{
    return nlohmann::json(name).get<EReliveLevelIds>();
}

static bool ParseInt(const std::string& text, s32& value)
{
    try
    {
        std::size_t used = 0;
        value = std::stoi(text, &used);
        return used == text.size();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

// The arguments naming a camera, from first on: <level> <path> <camera>
static bool CheckCameraArgs(const Automation::Command& command, std::size_t first, std::string& error)
{
    if (LevelFromName(command.mArgs[first]) == EReliveLevelIds::eNone)
    {
        error = "unknown level '" + command.mArgs[first] + "'";
        return false;
    }

    for (std::size_t i = first + 1; i < first + 3; i++)
    {
        s32 value = 0;
        if (!ParseInt(command.mArgs[i], value) || value < 0)
        {
            error = "'" + command.mArgs[i] + "' isn't a path or camera number";
            return false;
        }
    }
    return true;
}

// Checks a command's arguments, setting error if they're wrong
static bool CheckCommand(const Automation::Command& command, std::string& error)
{
    const std::string& name = command.mName;
    const std::size_t argCount = command.mArgs.size();

    auto expectArgs = [&](std::size_t count, const char_type* usage)
    {
        if (argCount != count)
        {
            error = std::string("expected: ") + usage;
            return false;
        }
        return true;
    };

    auto expectNumber = [&](const std::string& arg, s32 minimum)
    {
        s32 value = 0;
        if (!ParseInt(arg, value) || value < minimum)
        {
            error = "'" + arg + "' isn't a number of at least " + std::to_string(minimum);
            return false;
        }
        return true;
    };

    if (name == "start")
    {
        if (argCount != 3 && argCount != 5)
        {
            error = "expected: start <level> <path> <camera> [<x> <y>]";
            return false;
        }
        if (!CheckCameraArgs(command, 0, error))
        {
            return false;
        }
        return argCount == 3 || (expectNumber(command.mArgs[3], 0) && expectNumber(command.mArgs[4], 0));
    }

    if (name == "wait_frames")
    {
        return expectArgs(1, "wait_frames <n>") && expectNumber(command.mArgs[0], 1);
    }

    if (name == "wait_until")
    {
        if (argCount == 1 && command.mArgs[0] == "in_game")
        {
            return true;
        }
        if (argCount == 4 && command.mArgs[0] == "camera")
        {
            return CheckCameraArgs(command, 1, error);
        }
        error = "expected: wait_until in_game, or wait_until camera <level> <path> <camera>";
        return false;
    }

    if (name == "screenshot")
    {
        return expectArgs(1, "screenshot <file.png>");
    }

    if (name == "dump_state")
    {
        return expectArgs(1, "dump_state <file.json>");
    }

    if (name == "dump_objects")
    {
        return expectArgs(1, "dump_objects <file.json>");
    }

    if (name == "timeout")
    {
        return expectArgs(1, "timeout <seconds>") && expectNumber(command.mArgs[0], 1);
    }

    if (name == "quit")
    {
        return expectArgs(0, "quit");
    }

    error = "unknown command '" + name + "'";
    return false;
}

bool Automation::Parse(const std::string& script, std::vector<Command>& commands, std::string& error)
{
    commands.clear();

    std::istringstream lines(script);
    std::string line;
    s32 lineNumber = 0;
    while (std::getline(lines, line))
    {
        lineNumber++;

        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }

        std::istringstream words(line);
        Command command;
        command.mLine = lineNumber;
        if (!(words >> command.mName))
        {
            continue;
        }

        std::string arg;
        while (words >> arg)
        {
            command.mArgs.push_back(arg);
        }

        std::string why;
        if (!CheckCommand(command, why))
        {
            error = "line " + std::to_string(lineNumber) + ": " + why;
            return false;
        }
        commands.push_back(std::move(command));
    }
    return true;
}

Automation::Automation(FileSystem& fs, ResourceManagerWrapper& resMan, std::vector<Command> commands)
    : mFs(fs)
    , mResMan(resMan)
    , mCommands(std::move(commands))
    , mStartTicks(static_cast<u32>(SDL_GetTicks()))
{
}

Automation::Result Automation::Update(BaseMap& map, bool betweenFrames)
{
    if (betweenFrames)
    {
        mFrames++;
    }

    // Commands that finish straight away all run now, e.g. a dump then a screenshot
    while (mNextCommand < mCommands.size())
    {
        const Command& command = mCommands[mNextCommand];

        if (static_cast<u32>(SDL_GetTicks()) - mStartTicks > mTimeoutMs)
        {
            Fail(command, "timed out after " + std::to_string(mTimeoutMs / 1000) + " seconds");
        }
        else if (command.mName == "quit")
        {
            LOG_INFO("[automation] line %d: quit", command.mLine);
            return Result::eQuit;
        }
        else if (RunCommand(command, map, betweenFrames))
        {
            LOG_INFO("[automation] line %d: %s done", command.mLine, command.mName.c_str());
            mNextCommand++;
            mStep = 0;
            continue;
        }

        if (mFailed)
        {
            return Result::eQuit;
        }
        return Result::eContinue;
    }

    LOG_INFO("[automation] script finished");
    return Result::eQuit;
}

bool Automation::RunCommand(const Command& command, BaseMap& map, bool betweenFrames)
{
    const std::string& name = command.mName;
    const std::vector<std::string>& args = command.mArgs;

    if (name == "start")
    {
        return Start(command, map, betweenFrames);
    }

    if (name == "wait_frames")
    {
        if (mStep == 0)
        {
            mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(args[0]));
            mStep = 1;
        }
        return betweenFrames && mFrames >= mWaitUntilFrame;
    }

    if (name == "wait_until")
    {
        return WaitUntil(command, map, betweenFrames);
    }

    if (name == "screenshot")
    {
        if (!Screenshot(args[0]))
        {
            return false;
        }
        return !mFailed;
    }

    if (name == "dump_state")
    {
        return SaveText(args[0], StateJson(map));
    }

    if (name == "dump_objects")
    {
        return SaveText(args[0], ObjectsJson());
    }

    if (name == "timeout")
    {
        mTimeoutMs = static_cast<u32>(std::stoi(args[0])) * 1000;
        return true;
    }

    Fail(command, "unknown command");
    return false;
}

bool Automation::Start(const Command& command, BaseMap& map, bool betweenFrames)
{
    if (!betweenFrames)
    {
        return false;
    }

    const EReliveLevelIds level = LevelFromName(command.mArgs[0]);
    const s16 path = static_cast<s16>(std::stoi(command.mArgs[1]));
    const s16 camera = static_cast<s16>(std::stoi(command.mArgs[2]));
    const bool hasPosition = command.mArgs.size() == 5;
    const bool isAe = GetGameType() == GameType::eAe;

    if (mStep == 0)
    {
        // Like the menus when a game's begun: pend what Abe needs, and make him once it's loaded
        if (isAe ? !gAbe : !AO::gAbe)
        {
            if (isAe)
            {
                ::Abe::PendResources(mResMan);
            }
            else
            {
                AO::Abe::PendResources(mResMan);
            }
            mResMan.RequestLoadingWait();
        }
        mStep = 1;
        return false;
    }

    if (mStep == 1)
    {
        // The menu is finished with, as it is when a game's begun from it. AO's ignores camera
        // changes, so it would carry on otherwise.
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
            if (dynamic_cast<AO::Menu*>(pObj) || dynamic_cast<MainMenuController*>(pObj))
            {
                pObj->SetDead(true);
            }
        }

        BaseAliveGameObject* pAbe = nullptr;
        if (isAe)
        {
            if (!gPauseMenu)
            {
                gPauseMenu = relive_new PauseMenu(mResMan, map);
            }
            if (!gAbe)
            {
                gAbe = relive_new ::Abe(mResMan, map);
            }
            pAbe = gAbe;
        }
        else
        {
            if (!AO::gPauseMenu)
            {
                AO::gPauseMenu = relive_new AO::PauseMenu(mResMan, map);
            }
            if (!AO::gAbe)
            {
                AO::gAbe = relive_new AO::Abe(mResMan, map);
            }
            pAbe = AO::gAbe;
        }

        pAbe->SetUpdateDelay(1);
        pAbe->mCurrentLevel = level;
        pAbe->mCurrentPath = path;
        if (hasPosition)
        {
            pAbe->mXPos = FP_FromInteger(std::stoi(command.mArgs[3]));
            pAbe->mYPos = FP_FromInteger(std::stoi(command.mArgs[4]));
        }

        map.SetActiveCam(level, path, camera, CameraSwapEffects::eInstantChange_0, {}, 0);
        mStep = 2;
        return false;
    }

    if (map.mCurrentLevel != level || map.mCurrentPath != path || map.mCurrentCamera != camera)
    {
        return false;
    }

    if (!hasPosition && sControlledCharacter)
    {
        // Where DDCheat's teleport puts him
        PSX_Point camPos = {};
        map.GetCurrentCamCoords(&camPos);
        sControlledCharacter->mXPos = FP_FromInteger(camPos.x + (isAe ? 184 : 448));
        sControlledCharacter->mYPos = FP_FromInteger(camPos.y + (isAe ? 60 : 180));
    }
    return true;
}

bool Automation::WaitUntil(const Command& command, BaseMap& map, bool betweenFrames) const
{
    if (!betweenFrames)
    {
        return false;
    }

    if (command.mArgs[0] == "in_game")
    {
        return sControlledCharacter && map.mCurrentLevel != EReliveLevelIds::eMenu;
    }

    return map.mCurrentLevel == LevelFromName(command.mArgs[1])
        && map.mCurrentPath == std::stoi(command.mArgs[2])
        && map.mCurrentCamera == std::stoi(command.mArgs[3]);
}

bool Automation::Screenshot(const std::string& path)
{
    IRenderer* pRenderer = IRenderer::GetRenderer();
    if (mStep == 0)
    {
        pRenderer->RequestCapture();
        mStep = 1;
        return false;
    }

    std::vector<u8> pixels;
    s32 width = 0;
    s32 height = 0;
    if (!pRenderer->TakeCapture(pixels, width, height))
    {
        return false;
    }

    PNGFile png;
    const std::vector<u8> encoded = png.Encode(reinterpret_cast<const u32*>(pixels.data()), width, height);
    if (!mFs.Save(path.c_str(), encoded))
    {
        Fail(mCommands[mNextCommand], "couldn't save " + path);
    }
    return true;
}

bool Automation::SaveText(const std::string& path, const std::string& text)
{
    if (!mFs.Save(path.c_str(), std::vector<u8>(text.begin(), text.end())))
    {
        Fail(mCommands[mNextCommand], "couldn't save " + path);
        return false;
    }
    return true;
}

static f64 ToDouble(FP value)
{
    return FP_GetDouble(value);
}

std::string Automation::StateJson(BaseMap& map) const
{
    nlohmann::json state;
    state["game"] = GetGameType() == GameType::eAe ? "AE" : "AO";
    state["level"] = LevelName(map.mCurrentLevel);
    state["path"] = map.mCurrentPath;
    state["camera"] = map.mCurrentCamera;
    state["in_menu"] = map.mCurrentLevel == EReliveLevelIds::eMenu;
    state["camera_changing"] = map.DirectCameraChangePending() || map.LevelChanged() || map.PathChanged() || map.CameraChanged();
    state["game_frame"] = sGnFrame;
    state["script_frame"] = mFrames;
    state["movie_playing"] = Movie::gMovieRefCount > 0;
    state["loading"] = mResMan.IsLoading();

    BaseGameObject* pModal = map.GetActiveModal();
    state["modal"] = pModal ? nlohmann::json(TypeName(pModal->Type())) : nlohmann::json();

    if (sControlledCharacter)
    {
        state["controlled_character"] = {
            {"type", TypeName(sControlledCharacter->Type())},
            {"x", ToDouble(sControlledCharacter->mXPos)},
            {"y", ToDouble(sControlledCharacter->mYPos)}};
    }
    else
    {
        state["controlled_character"] = nullptr;
    }

    PSX_Point camPos = {};
    if (map.mCurrentLevel != EReliveLevelIds::eNone)
    {
        map.GetCurrentCamCoords(&camPos);
    }
    state["camera_world_pos"] = {camPos.x, camPos.y};

    return state.dump(2);
}

std::string Automation::ObjectsJson() const
{
    nlohmann::json objects = nlohmann::json::array();
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        nlohmann::json object;
        object["index"] = i;
        object["type"] = TypeName(pObj->Type());
        object["dead"] = pObj->GetDead();
        object["updatable"] = pObj->GetUpdatable();
        object["drawable"] = pObj->GetDrawable();

        if (pObj->GetIsBaseAnimatedWithPhysicsObj())
        {
            auto pAnimated = static_cast<BaseAnimatedWithPhysicsGameObject*>(pObj);
            object["x"] = ToDouble(pAnimated->mXPos);
            object["y"] = ToDouble(pAnimated->mYPos);
            object["sprite_scale"] = ToDouble(pAnimated->GetSpriteScale());
            object["rgb"] = {pAnimated->mRGB.r, pAnimated->mRGB.g, pAnimated->mRGB.b};

            Animation& anim = pAnimated->GetAnimation();
            if (anim.mAnimRes.mId != AnimId::None)
            {
                const RGB16& animRgb = anim.GetRgb();
                object["anim"] = {
                    {"id", AnimRecName(anim.mAnimRes.mId)},
                    {"frame", anim.GetCurrentFrame()},
                    {"render", anim.GetRender()},
                    {"layer", static_cast<s32>(anim.GetRenderLayer())},
                    {"semi_trans", anim.GetSemiTrans()},
                    // Blending off means the rgb tints it
                    {"blending", anim.GetBlending()},
                    {"blend_mode", static_cast<s32>(anim.GetBlendMode())},
                    {"flip_x", anim.GetFlipX()},
                    {"rgb", {animRgb.r, animRgb.g, animRgb.b}}};
            }
        }

        objects.push_back(std::move(object));
    }
    return objects.dump(2);
}

void Automation::Fail(const Command& command, const std::string& why)
{
    LOG_ERROR("[automation] line %d (%s): %s", command.mLine, command.mName.c_str(), why.c_str());
    mFailed = true;
}
