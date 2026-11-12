#include "Automation.hpp"
#include <cstdlib>
#include "AutomationSpawn.hpp"
#include "Bot/Bot.hpp"
#include "BaseMap.hpp"
#include "Engine.hpp"
#include "GameType.hpp"
#include "PsxDisplay.hpp"
#include "SoundDebug.hpp"
#include "StateHash.hpp"
#include "SwitchStates.hpp"
#include "Movie.hpp"
#include "ResourceManager.hpp"
#include "logger.hpp"
#include "Renderer/IRenderer.hpp"
#include "GameObjects/BaseAliveGameObject.hpp"
#include "GameObjects/PlatformBase.hpp"
#include "GameObjects/Electrocute.hpp"
#include "ObjectIds.hpp"
#include "../AliveLibAE/InvisibleEffect.hpp"
#include "data_conversion/AnimConversionInfo.hpp"
#include "data_conversion/EnumSerialization.hpp"
#include "data_conversion/PNGFile.hpp"
#include "data_conversion/file_system.hpp"
#include "../AliveLibAE/Abe.hpp"
#include "../AliveLibAE/Input.hpp"
#include "../AliveLibAE/MainMenu.hpp"
#include "../AliveLibAE/PauseMenu.hpp"
#include "../AliveLibAE/QuikSave.hpp"
#include "../AliveLibAO/Abe.hpp"
#include "../AliveLibAO/MainMenu.hpp"
#include "../AliveLibAO/PauseMenu.hpp"
#include "../AliveLibAO/QuikSave.hpp"
#include <SDL3/SDL_timer.h>
#include <sstream>

extern s16 gNumCamSwappers; // Engine.cpp

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
        case ReliveTypes::eAnimationState: return "eAnimationState";
        case ReliveTypes::eSpark: return "eSpark";
        case ReliveTypes::eHoneyDrip: return "eHoneyDrip";
        case ReliveTypes::eBellSong: return "eBellSong";
        case ReliveTypes::eClaw: return "eClaw";
        case ReliveTypes::eBoomMachinePipe: return "eBoomMachinePipe";
        case ReliveTypes::eRollingBallShaker: return "eRollingBallShaker";
        case ReliveTypes::eParamiteWeb: return "eParamiteWeb";
        case ReliveTypes::eCircularFade: return "eCircularFade";
        case ReliveTypes::eOrbWhirlWind: return "eOrbWhirlWind";
        case ReliveTypes::eDeathGas: return "eDeathGas";
        case ReliveTypes::eBulletShell: return "eBulletShell";
        case ReliveTypes::eGibs: return "eGibs";
        case ReliveTypes::eBlood: return "eBlood";
        case ReliveTypes::eGameSpeakListener: return "eGameSpeakListener";
        case ReliveTypes::eAliveObjectState: return "eAliveObjectState";
        case ReliveTypes::eEventsState: return "eEventsState";
        case ReliveTypes::eWorldStatics: return "eWorldStatics";
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
        case ReliveTypes::eLeaf: return "eLeaf";
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

    if (SoundDebug::IsSoundCommand(name))
    {
        return SoundDebug::Check(name, command.mArgs, error);
    }

    if (name == "fmv")
    {
        return expectArgs(1, "fmv <name>");
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

    if (name == "hold")
    {
        u32 bits = 0;
        return expectArgs(2, "hold <buttons> <frames>") && Automation::ParseButtons(command.mArgs[0], bits, error) && expectNumber(command.mArgs[1], 1);
    }

    if (name == "electrocute")
    {
        if (argCount > 1 || (argCount == 1 && command.mArgs[0] != "extra"))
        {
            error = "expected: electrocute [extra]";
            return false;
        }
        return true;
    }

    if (name == "invincible")
    {
        return expectArgs(0, "invincible");
    }

    if (name == "spawn")
    {
        if (argCount != 1 || !AutomationSpawn::IsKnown(command.mArgs[0]))
        {
            error = "expected: spawn <name>, one of:";
            for (const std::string& known : AutomationSpawn::Names())
            {
                error += " " + known;
            }
            return false;
        }
        return true;
    }

    if (name == "invisible")
    {
        if (argCount != 1 || (command.mArgs[0] != "out" && command.mArgs[0] != "in" && command.mArgs[0] != "now"))
        {
            error = "expected: invisible <out|in|now>";
            return false;
        }
        return true;
    }

    if (name == "switch")
    {
        return expectArgs(2, "switch <id> <0|1>") && expectNumber(command.mArgs[0], 0) && expectNumber(command.mArgs[1], 0);
    }

    if (name == "expect_objects")
    {
        return expectArgs(2, "expect_objects <type> <count>") && expectNumber(command.mArgs[1], 0);
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

    if (name == "hash_log")
    {
        if (argCount < 1 || argCount > 2 || (argCount == 2 && command.mArgs[1] != "full"))
        {
            error = "expected: hash_log <file> [full]";
            return false;
        }
        return true;
    }

    if (name == "hash_mark")
    {
        return expectArgs(1, "hash_mark <name>");
    }

    if (name == "dump_raw")
    {
        return expectArgs(1, "dump_raw <file>");
    }

    if (name == "expect_hash")
    {
        if (argCount < 1 || (argCount > 1 && command.mArgs[1] != "ignore") || argCount == 2)
        {
            error = "expected: expect_hash <name> [ignore <class>...]";
            return false;
        }
        return true;
    }

    if (name == "quicksave")
    {
        return expectArgs(0, "quicksave");
    }

    if (name == "quickload")
    {
        return expectArgs(0, "quickload");
    }

    if (name == "bot_trace")
    {
        return expectArgs(1, "bot_trace <file>");
    }

    if (name == "bot_nav_dump")
    {
        return expectArgs(1, "bot_nav_dump <file>");
    }

    if (name == "bot_log")
    {
        return expectArgs(1, "bot_log <file>");
    }

    if (name == "bot")
    {
        Bot::Profile profile;
        return expectArgs(1, "bot <profile>") && Bot::Profile::Parse(command.mArgs[0], profile, error);
    }

    if (name == "bot_wait")
    {
        return expectArgs(1, "bot_wait <max frames>") && expectNumber(command.mArgs[0], 1);
    }

    if (name == "bot_stop")
    {
        return expectArgs(0, "bot_stop");
    }

    if (name == "expect_bot_restores")
    {
        return expectArgs(1, "expect_bot_restores <at least>") && expectNumber(command.mArgs[0], 0);
    }

    if (name == "replay")
    {
        return expectArgs(1, "replay <inputs file>");
    }

    if (name == "bot_report" || name == "bot_inputs")
    {
        return expectArgs(1, (name + " <file>").c_str());
    }

    if (name == "expect_bot")
    {
        if (argCount != 1 || (command.mArgs[0] != "done" && command.mArgs[0] != "stuck" && command.mArgs[0] != "running"))
        {
            error = "expected: expect_bot done|stuck|running";
            return false;
        }
        return true;
    }

    if (name == "bot_goto")
    {
        if ((argCount != 2 && argCount != 3) || !expectNumber(command.mArgs[0], -100000) || !expectNumber(command.mArgs[1], -100000) || (argCount == 3 && command.mArgs[2] != "half"))
        {
            error = "expected: bot_goto <x> <y> [half]";
            return false;
        }
        return true;
    }

    if (name == "bot_do")
    {
        const std::string what = argCount > 0 ? command.mArgs[0] : "";
        u32 bits = 0;
        if (what == "wait" && (argCount == 2 || argCount == 3) && expectNumber(command.mArgs[1], 0))
        {
            return argCount == 2 || Automation::ParseButtons(command.mArgs[2], bits, error);
        }
        if (what == "move" && (argCount == 2 || argCount == 3) && expectNumber(command.mArgs[1], -100000))
        {
            if (argCount == 3 && command.mArgs[2] != "walk" && command.mArgs[2] != "run" && command.mArgs[2] != "sneak" && command.mArgs[2] != "roll")
            {
                error = "expected: bot_do move <x> [walk|run|sneak|roll]";
                return false;
            }
            return true;
        }
        if (what == "face" && argCount == 2 && (command.mArgs[1] == "left" || command.mArgs[1] == "right"))
        {
            return true;
        }
        if (what == "runjump" && argCount == 3 && expectNumber(command.mArgs[1], -100000) && (command.mArgs[2] == "left" || command.mArgs[2] == "right"))
        {
            return true;
        }
        if (what == "walkoff" && argCount == 2 && (command.mArgs[1] == "left" || command.mArgs[1] == "right"))
        {
            return true;
        }
        if (what == "press" && (argCount == 2 || argCount == 3))
        {
            return Automation::ParseButtons(command.mArgs[1], bits, error) && (argCount == 2 || expectNumber(command.mArgs[2], 1));
        }
        error = "expected: bot_do wait|move|face|press ... (see Automation.hpp)";
        return false;
    }

    if (name == "draw")
    {
        return expectArgs(1, "draw <0|1>") && expectNumber(command.mArgs[0], 0);
    }

    if (name == "restore_check")
    {
        if (argCount < 1 || argCount > 2 || !expectNumber(command.mArgs[0], 1))
        {
            error = "expected: restore_check <frames> [<buttons>]";
            return false;
        }
        u32 bits = 0;
        return argCount == 1 || Automation::ParseButtons(command.mArgs[1], bits, error);
    }

    if (name == "expect_restore_checks")
    {
        return expectArgs(0, "expect_restore_checks");
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

Automation::~Automation()
{
    if (mBot && !mBotTracePath.empty())
    {
        SaveText(mBotTracePath, mBot->Trace());
    }
    if (mBot && !mBotLogPath.empty())
    {
        SaveText(mBotLogPath, mBot->LogText());
    }
    mBot.reset();
    FlushHashLog();
    SoundDebug::SetScriptFrame(false, 0);
}

Automation::Automation(FileSystem& fs, ResourceManager& resMan, std::vector<Command> commands)
    : mFs(fs)
    , mResMan(resMan)
    , mCommands(std::move(commands))
    , mStartTicks(static_cast<u32>(SDL_GetTicks()))
{
}

Automation::Result Automation::Update(BaseMap& map, bool betweenFrames)
{
    // A modal (the pause menu, a movie) runs a frame of its own each loop iteration
    const bool frameTick = betweenFrames || map.GetActiveModal() != nullptr;
    if (frameTick)
    {
        mFrames++;
    }
    SoundDebug::SetScriptFrame(true, mFrames);

    if (!mHashLogPath.empty() && betweenFrames && !mRestoring)
    {
        LogHash(map);
    }

    // The bot acts on what the last frame left, before the commands look at it
    if (mBot)
    {
        mBot->Update(map, betweenFrames);
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

Bot::Bot& Automation::TheBot()
{
    if (!mBot)
    {
        mBot = std::make_unique<Bot::Bot>();
    }
    return *mBot;
}

bool Automation::ParseButtons(const std::string& buttons, u32& bits, std::string& error)
{
    struct Button final
    {
        const char* mName;
        u32 mBits;
    };
    // The shared (AE) InputCommands, which AO's input converts from
    static const Button kButtons[] = {
        {"up", InputCommands::eUp},
        {"down", InputCommands::eDown},
        {"left", InputCommands::eLeft},
        {"right", InputCommands::eRight},
        {"run", InputCommands::eRun},
        {"action", InputCommands::eDoAction},
        {"sneak", InputCommands::eSneak},
        {"throw", InputCommands::eThrowItem},
        {"hop", InputCommands::eHop},
        {"roll", InputCommands::eFartOrRoll},
        {"fart", InputCommands::eFartOrRoll},
        {"speak1", InputCommands::eGameSpeak1},
        {"speak2", InputCommands::eGameSpeak2},
        {"speak3", InputCommands::eGameSpeak3},
        {"speak4", InputCommands::eGameSpeak4},
        {"speak5", InputCommands::eGameSpeak5},
        {"speak6", InputCommands::eGameSpeak6},
        {"speak7", InputCommands::eGameSpeak7},
        {"speak8", InputCommands::eGameSpeak8},
        {"chant", InputCommands::eChant},
        {"pause", InputCommands::ePause},
        {"confirm", InputCommands::eUnPause_OrConfirm},
        {"back", InputCommands::eBack},
        {"cheat", InputCommands::eCheatMode},
    };

    bits = 0;
    std::stringstream ss(buttons);
    std::string name;
    while (std::getline(ss, name, '+'))
    {
        bool found = false;
        for (const Button& button : kButtons)
        {
            if (name == button.mName)
            {
                bits |= button.mBits;
                found = true;
                break;
            }
        }
        if (!found)
        {
            error = "unknown button " + name + " (up, down, left, right, run, action, sneak, throw, hop, roll, fart, speak1-8, chant, pause, back, cheat)";
            return false;
        }
    }
    if (bits == 0)
    {
        error = "no buttons";
        return false;
    }
    return true;
}

bool Automation::RunCommand(const Command& command, BaseMap& map, bool betweenFrames)
{
    const std::string& name = command.mName;
    const std::vector<std::string>& args = command.mArgs;

    if (name == "start")
    {
        return Start(command, map, betweenFrames);
    }

    if (SoundDebug::IsSoundCommand(name))
    {
        // Between frames, so it's in the same place in the run each time
        if (!betweenFrames)
        {
            return false;
        }
        std::string error;
        if (!SoundDebug::Run(name, args, error, map, mResMan))
        {
            Fail(command, error);
            return false;
        }
        return true;
    }

    if (name == "fmv")
    {
        // Starts the movie as the game's camera and menu code do: an object that takes over the main
        // loop once it updates. Between frames, so it's always in the same place in the run
        if (!betweenFrames)
        {
            return false;
        }
        relive_new Movie(args[0].c_str(), mResMan, map);
        return true;
    }

    if (name == "wait_frames")
    {
        if (mStep == 0)
        {
            mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(args[0]));
            mStep = 1;
        }
        return (betweenFrames || map.GetActiveModal()) && mFrames >= mWaitUntilFrame;
    }

    if (name == "wait_until")
    {
        return WaitUntil(command, map, betweenFrames);
    }

    if (name == "hold")
    {
        const bool frameTick = betweenFrames || map.GetActiveModal() != nullptr;
        if (mStep == 0)
        {
            if (!frameTick)
            {
                return false;
            }
            u32 bits = 0;
            std::string error;
            ParseButtons(args[0], bits, error);
            Input().mAutomationInput = bits;
            mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(args[1]));
            mStep = 1;
        }
        if (frameTick && mFrames >= mWaitUntilFrame)
        {
            Input().mAutomationInput = 0;
            return true;
        }
        return false;
    }

    if (name == "electrocute")
    {
        if (!betweenFrames)
        {
            return false;
        }
        if (!sControlledCharacter)
        {
            Fail(command, "there is no controlled character");
            return true;
        }
        relive_new Electrocute(sControlledCharacter, !args.empty(), true, mResMan, map);
        return true;
    }

    if (name == "invincible")
    {
        if (GetGameType() == GameType::eAe)
        {
            gAbeInvincible = true;
        }
        else
        {
            AO::gAbeInvulnerableCheat = 1;
        }
        return true;
    }

    if (name == "spawn")
    {
        if (!betweenFrames)
        {
            return false;
        }
        if (!AutomationSpawn::Spawn(args[0], mResMan, map))
        {
            Fail(command, "there is no controlled character");
        }
        return true;
    }

    if (name == "invisible")
    {
        if (!betweenFrames)
        {
            return false;
        }
        if (!sControlledCharacter || GetGameType() != GameType::eAe)
        {
            Fail(command, "invisible needs AE and a controlled character");
            return true;
        }
        auto* pEffect = static_cast<InvisibleEffect*>(sObjectIds.Find_Impl(mInvisibleEffectId));
        if (!pEffect || pEffect->GetDead())
        {
            pEffect = relive_new InvisibleEffect(sControlledCharacter, mResMan, map);
            mInvisibleEffectId = pEffect->mBaseGameObjectId;
        }
        if (args[0] == "out")
        {
            pEffect->BecomeInvisible();
        }
        else if (args[0] == "in")
        {
            pEffect->BecomeVisible();
        }
        else
        {
            pEffect->InstantInvisibility();
        }
        return true;
    }

    if (name == "switch")
    {
        if (!betweenFrames)
        {
            return false;
        }
        SwitchStates_Set(static_cast<u16>(std::stoi(args[0])), static_cast<s8>(std::stoi(args[1])));
        return true;
    }

    if (name == "expect_objects")
    {
        s32 count = 0;
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
            if (!pObj)
            {
                break;
            }
            if (!pObj->GetDead() && args[0] == TypeName(pObj->Type()))
            {
                count++;
            }
        }
        if (count != std::stoi(args[1]))
        {
            Fail(command, "expected " + args[1] + " " + args[0] + ", found " + std::to_string(count));
        }
        return true;
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

    if (name == "dump_raw")
    {
        return betweenFrames && SaveText(args[0], StateHash::RawMemoryDump());
    }

    if (name == "hash_mark")
    {
        if (!betweenFrames)
        {
            return false;
        }
        mHashMarks[args[0]] = StateHash::Compute(map, true);
        return true;
    }

    if (name == "expect_hash")
    {
        if (!betweenFrames)
        {
            return false;
        }
        auto mark = mHashMarks.find(args[0]);
        if (mark == mHashMarks.end())
        {
            Fail(command, "no hash_mark called " + args[0]);
            return false;
        }
        const std::vector<std::string> ignore(args.begin() + (args.size() > 1 ? 2 : 1), args.end());
        const std::string difference = StateHash::Compute(map, true).Differences(mark->second, ignore);
        if (!difference.empty())
        {
            Fail(command, "the world isn't what hash_mark " + args[0] + " saw: " + difference);
            return false;
        }
        return true;
    }

    if (name == "hash_log")
    {
        if (!betweenFrames)
        {
            return false;
        }
        mHashLogPath = args[0];
        mHashLogFull = args.size() == 2;
        mHashLog.clear();
        LogHash(map);
        return true;
    }

    if (name == "quicksave")
    {
        if (!betweenFrames)
        {
            return false;
        }
        if (GetGameType() == GameType::eAe)
        {
            QuikSave::DoQuicksave(map);
        }
        else
        {
            AO::QuikSave::DoQuicksave(map);
        }
        return true;
    }

    if (name == "restore_check")
    {
        return RestoreCheck(command, map, betweenFrames);
    }

    if (name == "expect_restore_checks")
    {
        if (mFailedRestoreChecks > 0)
        {
            Fail(command, std::to_string(mFailedRestoreChecks) + " of " + std::to_string(mRestoreChecks) + " restore checks failed");
            return false;
        }
        return true;
    }

    if (name == "bot")
    {
        Bot::Profile profile;
        std::string error;
        Bot::Profile::Parse(command.mArgs[0], profile, error);
        TheBot().Start(profile);
        return true;
    }

    if (name == "bot_stop")
    {
        TheBot().Stop();
        return true;
    }

    if (name == "expect_bot_restores")
    {
        if (TheBot().Restores() < std::stoi(command.mArgs[0]))
        {
            Fail(command, "the bot went back " + std::to_string(TheBot().Restores()) + " times");
        }
        return true;
    }

    if (name == "replay")
    {
        if (mStep == 0)
        {
            // "start <game frame>" then a line of hex button bits per frame, as bot_inputs writes
            mReplay.clear();
            mReplayAt = 0;
            std::istringstream in(mFs.LoadToString(command.mArgs[0].c_str()));
            std::string word;
            u32 startFrame = 0;
            if (!(in >> word >> startFrame) || word != "start")
            {
                Fail(command, "not an inputs file: " + command.mArgs[0]);
                return true;
            }
            while (in >> word)
            {
                mReplay.push_back(static_cast<u32>(std::stoul(word, nullptr, 16)));
            }
            mReplayStartFrame = startFrame;
            mStep = 1;
            return false;
        }
        // A line with the top bit set is for an iteration a modal (a movie) ran in, the rest for game frames
        const u32 kModalInput = 0x80000000u;
        const bool nextIsModal = mReplayAt < mReplay.size() && (mReplay[mReplayAt] & kModalInput) != 0;
        if (!betweenFrames)
        {
            if (nextIsModal && map.GetActiveModal())
            {
                Input().mAutomationInput = mReplay[mReplayAt++] & ~kModalInput;
            }
            return false;
        }
        if (mReplayAt == 0 && sGnFrame != mReplayStartFrame)
        {
            Fail(command, "the inputs start at game frame " + std::to_string(mReplayStartFrame) + " but this is frame " + std::to_string(sGnFrame));
            return true;
        }
        // The modal is over: what was left for it isn't wanted
        while (mReplayAt < mReplay.size() && (mReplay[mReplayAt] & kModalInput) != 0)
        {
            mReplayAt++;
        }
        if (mReplayAt >= mReplay.size())
        {
            Input().mAutomationInput = 0;
            return true;
        }
        Input().mAutomationInput = mReplay[mReplayAt++];
        return false;
    }

    if (name == "bot_wait")
    {
        Bot::Bot& bot = TheBot();
        if (mStep == 0)
        {
            mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(command.mArgs[0]));
            mStep = 1;
        }
        if (bot.State() == Bot::BotState::eRunning && mFrames < mWaitUntilFrame)
        {
            return false;
        }
        LOG_INFO("[bot_wait] %s", bot.Report().c_str());
        return true;
    }

    if (name == "bot_report")
    {
        return SaveText(command.mArgs[0], TheBot().Report());
    }

    if (name == "bot_inputs")
    {
        return SaveText(command.mArgs[0], TheBot().InputsText());
    }

    if (name == "expect_bot")
    {
        const Bot::BotState state = TheBot().State();
        const char* is = state == Bot::BotState::eDone ? "done" : state == Bot::BotState::eStuck ? "stuck" : state == Bot::BotState::eRunning ? "running" : "idle";
        if (command.mArgs[0] != is)
        {
            Fail(command, "the bot is " + std::string(is) + ": " + TheBot().Why());
        }
        return true;
    }

    if (name == "bot_log")
    {
        mBotLogPath = command.mArgs[0];
        TheBot();
        return true;
    }

    if (name == "bot_goto")
    {
        Bot::Bot& bot = TheBot();
        if (mStep == 0)
        {
            bot.GoTo(std::stoi(command.mArgs[0]), std::stoi(command.mArgs[1]), command.mArgs.size() > 2);
            mStep = 1;
            return false;
        }
        if (bot.State() == Bot::BotState::eRunning)
        {
            return false;
        }
        const Bot::AbeState& abe = bot.Abe();
        LOG_INFO("[bot_goto] %s at %d,%d f=%u deaths=%d restores=%d", bot.State() == Bot::BotState::eDone ? "done" : bot.Why().c_str(), abe.mX, abe.mY, abe.mFrame, bot.Deaths(), bot.Restores());
        if (bot.State() != Bot::BotState::eDone)
        {
            Fail(command, "bot_goto: " + bot.Why());
        }
        return true;
    }

    if (name == "bot_nav_dump")
    {
        Bot::NavGraph graph;
        graph.Build(Bot::GameView::BuildNavInput(map));
        // And the planning problem made from it, as seen from where Abe is
        Bot::World world;
        Bot::GameView::ReadWorld(map, world);
        const Bot::AbeState abe = Bot::GameView::ReadAbe(map);
        const Bot::Domain domain = Bot::Domain::Build(graph, world, graph.NodeAt(abe.mX, abe.mY, abe.mHalfScale, 8), nullptr);
        std::string regions;
        for (std::size_t i = 0; i < graph.Nodes().size(); i++)
        {
            if (i == 0 || domain.mRegions[i] != domain.mRegions[i - 1] || graph.Nodes()[i].mLine != graph.Nodes()[i - 1].mLine)
            {
                regions += "region " + std::to_string(domain.mRegions[i] + 1) + " from " + std::to_string(graph.Nodes()[i].mX) + "," + std::to_string(graph.Nodes()[i].mY) + "\n";
            }
        }
        return SaveText(command.mArgs[0], graph.Text() + regions + domain.Text());
    }

    if (name == "bot_trace")
    {
        mBotTracePath = command.mArgs[0];
        TheBot().StartTrace();
        return true;
    }

    if (name == "bot_do")
    {
        Bot::Bot& bot = TheBot();
        if (mStep == 0)
        {
            const std::string& what = command.mArgs[0];
            std::string error;
            u32 bits = 0;
            if (what == "wait")
            {
                if (command.mArgs.size() > 2)
                {
                    ParseButtons(command.mArgs[2], bits, error);
                }
                bot.Do(Bot::Prim::Wait(std::stoi(command.mArgs[1]), bits));
            }
            else if (what == "move")
            {
                Bot::Gait gait = Bot::Gait::eWalk;
                if (command.mArgs.size() > 2)
                {
                    gait = command.mArgs[2] == "run" ? Bot::Gait::eRun : command.mArgs[2] == "sneak" ? Bot::Gait::eSneak : command.mArgs[2] == "roll" ? Bot::Gait::eRoll : Bot::Gait::eWalk;
                }
                bot.Do(Bot::Prim::MoveTo(std::stoi(command.mArgs[1]), gait));
            }
            else if (what == "face")
            {
                bot.Do(Bot::Prim::Face(command.mArgs[1] == "left"));
            }
            else if (what == "runjump")
            {
                bot.Do(Bot::Prim::RunJump(std::stoi(command.mArgs[1]), command.mArgs[2] == "left"));
            }
            else if (what == "walkoff")
            {
                bot.Do(Bot::Prim::WalkOff(command.mArgs[1] == "left"));
            }
            else
            {
                ParseButtons(command.mArgs[1], bits, error);
                bot.Do(Bot::Prim::Press(bits, command.mArgs.size() > 2 ? std::stoi(command.mArgs[2]) : 12));
            }
            mStep = 1;
            return false;
        }
        if (bot.PilotBusy())
        {
            return false;
        }
        const Bot::AbeState& abe = bot.Abe();
        LOG_INFO("[bot_do] %s: %s at %d,%d %s", command.mArgs[0].c_str(), bot.PilotResult() == Bot::PilotStatus::eDone ? "done" : bot.PilotWhy().c_str(), abe.mX, abe.mY, Bot::MoveName(abe.mMove));
        if (bot.PilotResult() != Bot::PilotStatus::eDone)
        {
            Fail(command, "bot_do failed: " + bot.PilotWhy());
        }
        return true;
    }

    if (name == "draw")
    {
        gPsxDisplay.mSkipDraw = command.mArgs[0] == "0";
        return true;
    }

    if (name == "quickload")
    {
        // The restore loads in the background and changes camera, so wait for the next quiet frame
        if (mStep == 0)
        {
            if (!betweenFrames)
            {
                return false;
            }
            map.mRestoreKeepsInput = true;
            if (GetGameType() == GameType::eAe)
            {
                QuikSave::LoadActive(map);
            }
            else
            {
                AO::QuikSave::LoadActive(map);
            }
            mRestoring = true;
            mStep = 1;
            return false;
        }
        // Done once the restore has made its objects and the camera change is over (see Engine::RunFrame). The
        // resource manager can go on loading in the background after that
        if (betweenFrames && !map.mPendingSaveRestore && map.mCamState == BaseMap::CamChangeStates::eInactive_0 && gNumCamSwappers == 0)
        {
            mRestoring = false;
            // The restored state itself, before any frame has run
            if (!mHashLogPath.empty())
            {
                LogHash(map);
            }
            return true;
        }
        return false;
    }

    if (name == "timeout")
    {
        mTimeoutMs = static_cast<u32>(std::stoi(args[0])) * 1000;
        return true;
    }

    Fail(command, "unknown command");
    return false;
}

// For finding what differs: with RELIVE_RESTORE_DUMP=<prefix> the raw memory of the objects is written as <prefix>.A (when
// saved), .C (the original, later), .B (just restored) and .D (the restored, later), the same format as dump_raw
// The motions of the controlled character that are him dying, or the sequence that follows: the game saves nothing of it
// that a restore could be compared with, and it ends by loading the checkpoint
static bool IsDyingMotion(s32 motion)
{
    if (GetGameType() == GameType::eAe)
    {
        switch (static_cast<eAbeMotions>(motion))
        {
            case eAbeMotions::Motion_56_DeathDropFall_4591F0:
            case eAbeMotions::Motion_57_Dead_4589A0:
            case eAbeMotions::Motion_58_DeadPre_4593E0:
            case eAbeMotions::Motion_84_FallLandDie_45A420:
            case eAbeMotions::Motion_109_ZShotRolling:
            case eAbeMotions::Motion_110_ZShot:
            case eAbeMotions::Motion_129_PoisonGasDeath:
                return true;
            default:
                return false;
        }
    }
    switch (static_cast<AO::eAbeMotions>(motion))
    {
        case AO::eAbeMotions::Motion_59_DeathDropFall:
        case AO::eAbeMotions::Motion_60_Dead:
        case AO::eAbeMotions::Motion_86_FallLandDie:
        case AO::eAbeMotions::Motion_147_ShotRolling:
        case AO::eAbeMotions::Motion_148_Shot:
        case AO::eAbeMotions::Motion_164_PoisonGasDeath:
            return true;
        default:
            return false;
    }
}

static bool IsIdleMotion(s32 motion)
{
    return GetGameType() == GameType::eAe ? motion == static_cast<s32>(eAbeMotions::Motion_0_Idle_44EEB0) : motion == static_cast<s32>(AO::eAbeMotions::Motion_0_Idle);
}

// For finding why the order of a list differs: RELIVE_RESTORE_ORDERS=<part name, e.g. anim_order> logs it at each point of a check
static void LogRestoreCheckOrder(const char* point, const StateHash& hash)
{
    const char* pName = std::getenv("RELIVE_RESTORE_ORDERS");
    if (!pName)
    {
        return;
    }
    for (const StateHash::Part& part : hash.mParts)
    {
        if (part.mName == pName)
        {
            std::string flat = part.mText;
            std::replace(flat.begin(), flat.end(), '\n', ',');
            LOG_INFO("[restore_check] order %s %s: %s", pName, point, flat.c_str());
        }
    }
}

void Automation::DumpRestoreCheckRaw(const char* suffix)
{
    const char* pPrefix = std::getenv("RELIVE_RESTORE_DUMP");
    if (pPrefix)
    {
        SaveText(std::string(pPrefix) + "." + suffix, StateHash::RawMemoryDump());
    }
}

bool Automation::RestoreCheck(const Command& command, BaseMap& map, bool betweenFrames)
{
    const bool frameTick = betweenFrames || map.GetActiveModal() != nullptr;
    const std::vector<std::string>& args = command.mArgs;

    // Skipping a check leaves the game where it is, e.g. in its death sequence: the script goes on once that is over,
    // or the next camera it starts would cut it short and leave the character dying for the rest of the run
    const auto skipAndSettle = [this]()
    {
        Input().mAutomationInput = 0;
        mSettleUntil = mFrames + 1500;
        // Once it is calm the check is made, a check is only skipped for good if that is still not possible
        mSettleRetry = !mCheckRetried;
        mCheckRetried = true;
        mStep = 6;
        return false;
    };

    switch (mStep)
    {
        case 0:
        {
            if (!betweenFrames)
            {
                return false;
            }
            mCheckJson.clear();
            mCheckWhere = LevelName(map.mCurrentLevel) + " " + std::to_string(map.mCurrentPath) + " " + std::to_string(map.mCurrentCamera);
            if (!sControlledCharacter || !(sControlledCharacter->mHealth > FP_FromInteger(0)))
            {
                LOG_INFO("[restore_check] %s: skipped, the controlled character is dead", mCheckWhere.c_str());
                return skipAndSettle();
            }

            // Nor in the death sequence, which gives the character his health back before it is over
            if (IsDyingMotion(sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion)))
            {
                LOG_INFO("[restore_check] %s: skipped, the controlled character is dying", mCheckWhere.c_str());
                return skipAndSettle();
            }

            // The game saves between camera changes: the swapper and the fades that count as one are made again by nothing
            if (gNumCamSwappers != 0 || map.mCamState != BaseMap::CamChangeStates::eInactive_0)
            {
                LOG_INFO("[restore_check] %s: skipped, a camera change is under way (%d camera swappers, state %d)", mCheckWhere.c_str(), static_cast<s32>(gNumCamSwappers), static_cast<s32>(map.mCamState));
                return skipAndSettle();
            }

            // So that the world saved is in the order the one that is restored will be
            BaseGameObject::DestroyDeadObjects();
            mCheckSaved = StateHash::Compute(map, true);
            LogRestoreCheckOrder("A", mCheckSaved);
            DumpRestoreCheckRaw("A");
            if (GetGameType() == GameType::eAe)
            {
                QuikSave::DoQuicksave(map);
                QuikSave::StashActive();
            }
            else
            {
                AO::QuikSave::DoQuicksave(map);
                AO::QuikSave::StashActive();
            }

            std::string error;
            mCheckDied = false;
            mCheckButtons = 0;
            if (args.size() > 1)
            {
                ParseButtons(args[1], mCheckButtons, error);
            }
            Input().mAutomationInput = mCheckButtons;
            mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(args[0]));
            mStep = 1;
            return false;
        }

        case 1:
            if (!mCheckDied && (!sControlledCharacter || !(sControlledCharacter->mHealth > FP_FromInteger(0)) || IsDyingMotion(sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion))))
            {
                mCheckDied = true;
                mCheckDiedMotion = sControlledCharacter ? sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion) : -1;
                // What dying goes back to is what this check saved, where he may be falling to his death: back to where the script started
                if (GetGameType() == GameType::eAe)
                {
                    QuikSave::UnstashStart();
                }
                else
                {
                    AO::QuikSave::UnstashStart();
                }
            }
            if (frameTick && mFrames >= mWaitUntilFrame)
            {
                Input().mAutomationInput = 0;
                mStep = 2;
            }
            return false;

        case 2:
            if (!betweenFrames)
            {
                return false;
            }
            if (mCheckDied || !sControlledCharacter || !(sControlledCharacter->mHealth > FP_FromInteger(0)) || IsDyingMotion(sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion)))
            {
                LOG_INFO("[restore_check] %s: skipped, the controlled character died while it waited (motion %d, died at motion %d)", mCheckWhere.c_str(), sControlledCharacter ? sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion) : -1, mCheckDiedMotion);
                // What dying goes back to is what the check saved, where he may be falling to his death: back to where the script started
                if (GetGameType() == GameType::eAe)
                {
                    QuikSave::UnstashStart();
                }
                else
                {
                    AO::QuikSave::UnstashStart();
                }
                return skipAndSettle();
            }
            mCheckLater = StateHash::Compute(map, true);
            LogRestoreCheckOrder("C", mCheckLater);
            DumpRestoreCheckRaw("C");
            map.mRestoreKeepsInput = true;
            if (GetGameType() == GameType::eAe)
            {
                // What the wait did may have made another save (a continue point does): it is the one from the start that is loaded
                QuikSave::UnstashActive();
                // The save is loaded as it is from a file
                mCheckJson = QuikSave::RoundTripThroughJson();
                QuikSave::LoadActive(map);
            }
            else
            {
                AO::QuikSave::UnstashActive();
                AO::QuikSave::LoadActive(map);
            }
            mRestoring = true;
            mStep = 3;
            return false;

        case 3:
            if (betweenFrames && !map.mPendingSaveRestore && map.mCamState == BaseMap::CamChangeStates::eInactive_0 && gNumCamSwappers == 0)
            {
                mRestoring = false;
                const StateHash restored = StateHash::Compute(map, true);
                LogRestoreCheckOrder("B", restored);
                mCheckAtRestore = mCheckSaved.Differences(restored, {});
                DumpRestoreCheckRaw("B");
                Input().mAutomationInput = mCheckButtons;
                mWaitUntilFrame = mFrames + static_cast<u32>(std::stoi(args[0]));
                mStep = 4;
            }
            return false;

        case 4:
            if (frameTick && mFrames >= mWaitUntilFrame)
            {
                Input().mAutomationInput = 0;
                mStep = 5;
            }
            return false;

        case 6:
        {
            if (!betweenFrames)
            {
                return false;
            }
            // Alive and standing: the death sequence ends by loading the checkpoint, where he is
            const bool calm = sControlledCharacter && sControlledCharacter->mHealth > FP_FromInteger(0)
                && IsIdleMotion(sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion))
                && gNumCamSwappers == 0 && map.mCamState == BaseMap::CamChangeStates::eInactive_0 && !map.mPendingSaveRestore;
            if (calm || mFrames >= mSettleUntil)
            {
                LOG_INFO("[restore_check] %s: settled after %s (swappers %d, cam state %d, frame %d, motion %d, health %d)", mCheckWhere.c_str(), calm ? "it was calm" : "the time ran out", static_cast<s32>(gNumCamSwappers), static_cast<s32>(map.mCamState), static_cast<s32>(sGnFrame), sControlledCharacter ? sControlledCharacter->VGetMotion(eMotionType::eCurrentMotion) : -1, sControlledCharacter ? FP_GetExponent(sControlledCharacter->mHealth) : -1);
                if (!calm)
                {
                    std::string names;
                    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
                    {
                        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
                        if (pObj && !pObj->GetDead())
                        {
                            names += StateHash::ClassName(*pObj) + (pObj->GetUpdatable() ? " " : "(not updatable) ");
                        }
                    }
                    LOG_INFO("[restore_check] %s: objects: %s", mCheckWhere.c_str(), names.c_str());
                }
                mStep = 0;
                if (calm && mSettleRetry)
                {
                    mSettleRetry = false;
                    return false;
                }
                mSettleRetry = false;
                mCheckRetried = false;
                return true;
            }
            return false;
        }

        default:
        {
            if (!betweenFrames)
            {
                return false;
            }
            const StateHash atEndHash = StateHash::Compute(map, true);
            LogRestoreCheckOrder("D", atEndHash);
            const std::string atEnd = mCheckLater.Differences(atEndHash, {});
            DumpRestoreCheckRaw("D");
            mRestoreChecks++;
            mCheckRetried = false;
            if (mCheckAtRestore.empty() && atEnd.empty() && mCheckJson.empty())
            {
                LOG_INFO("[restore_check] %s: OK", mCheckWhere.c_str());
            }
            else
            {
                mFailedRestoreChecks++;
                if (!mCheckJson.empty())
                {
                    LOG_ERROR("[restore_check] %s: %s", mCheckWhere.c_str(), mCheckJson.c_str());
                }
                if (!mCheckAtRestore.empty())
                {
                    LOG_ERROR("[restore_check] %s: restored world differs: %s", mCheckWhere.c_str(), mCheckAtRestore.c_str());
                }
                if (!atEnd.empty())
                {
                    LOG_ERROR("[restore_check] %s: %s frames on it differs: %s", mCheckWhere.c_str(), args[0].c_str(), atEnd.c_str());
                }
            }
            return true;
        }
    }
}

bool Automation::Start(const Command& command, BaseMap& map, bool betweenFrames)
{
    if (!betweenFrames)
    {
        return false;
    }

    SoundDebug::ForgetOwners();
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
        // He isn't on the floor he was on, nor on what he stood on: he falls to the one here
        if (PlatformBase* pPlatform = static_cast<PlatformBase*>(sObjectIds.Find_Impl(sControlledCharacter->BaseAliveGameObject_PlatformId)))
        {
            pPlatform->VRemove(sControlledCharacter);
        }
        sControlledCharacter->BaseAliveGameObject_PlatformId = Guid{};
        sControlledCharacter->BaseAliveGameObjectCollisionLine = nullptr;
        sControlledCharacter->BaseAliveGameObjectLastLineYPos = sControlledCharacter->mYPos;
    }

    // The menus' new game makes a checkpoint, so dying has somewhere to respawn. Without one the
    // respawn goes to level none and crashes
    if (isAe)
    {
        QuikSave::DoQuicksave(map);
        // Like touching a continue point, which also saves what restart path goes back to
        gAbe->RequestQuicksave();
        QuikSave::StashStart();
    }
    else
    {
        AO::QuikSave::SaveCheckpoint(map);
        AO::QuikSave::StashStart();
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

void Automation::LogHash(BaseMap& map)
{
    const StateHash hash = StateHash::Compute(map, mHashLogFull);
    // One line per state: loading and other waits go round the main loop without a frame passing
    if (sGnFrame == mHashLoggedFrame && hash.mTotal == mHashLoggedTotal)
    {
        return;
    }
    mHashLoggedFrame = sGnFrame;
    mHashLoggedTotal = hash.mTotal;
    char line[64] = {};
    snprintf(line, sizeof(line), "%u %016llx\n", sGnFrame, static_cast<unsigned long long>(hash.mTotal));
    mHashLog += line;
    for (const StateHash::Part& part : hash.mParts)
    {
        snprintf(line, sizeof(line), "  %016llx ", static_cast<unsigned long long>(part.mHash));
        mHashLog += line + part.mName + "\n";
        if (!part.mText.empty())
        {
            mHashLog += "    " + part.mText + "\n";
        }
    }
}

void Automation::FlushHashLog()
{
    if (!mHashLogPath.empty() && !mFs.Save(mHashLogPath.c_str(), std::vector<u8>(mHashLog.begin(), mHashLog.end())))
    {
        LOG_ERROR("[automation] couldn't save %s", mHashLogPath.c_str());
    }
    mHashLogPath.clear();
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
        object["class"] = StateHash::ClassName(*pObj);
        object["dead"] = pObj->GetDead();
        object["updatable"] = pObj->GetUpdatable();
        object["drawable"] = pObj->GetDrawable();

        if (pObj->GetIsBaseAnimatedWithPhysicsObj())
        {
            auto pAnimated = static_cast<BaseAnimatedWithPhysicsGameObject*>(pObj);
            object["x"] = ToDouble(pAnimated->mXPos);
            object["y"] = ToDouble(pAnimated->mYPos);
            object["sprite_scale"] = ToDouble(pAnimated->GetSpriteScale());
            object["vel_x"] = ToDouble(pAnimated->mVelX);
            object["vel_y"] = ToDouble(pAnimated->mVelY);
            if (pAnimated->GetAnimation().GetRender())
            {
                const PSX_RECT bound = pAnimated->VGetBoundingRect();
                object["bound"] = {bound.x, bound.y, bound.w, bound.h};
            }
            if (pAnimated->GetIsBaseAliveGameObject())
            {
                auto pAlive = static_cast<BaseAliveGameObject*>(pAnimated);
                object["health"] = ToDouble(pAlive->mHealth);
                object["motion"] = pAlive->VGetMotion(eMotionType::eCurrentMotion);
                object["next_motion"] = pAlive->VGetMotion(eMotionType::eNextMotion);
                object["previous_motion"] = pAlive->VGetMotion(eMotionType::ePreviousMotion);
            }
            object["rgb"] = {pAnimated->mRGB.r, pAnimated->mRGB.g, pAnimated->mRGB.b};

            Animation& anim = pAnimated->GetAnimation();
            if (anim.mAnimRes.mId != AnimId::None)
            {
                const RGB16& animRgb = anim.GetRgb();
                object["anim"] = {
                    {"id", AnimRecName(anim.mAnimRes.mId)},
                    {"frame", anim.GetCurrentFrame()},
                    {"frame_change_counter", anim.GetFrameChangeCounter()},
                    {"frame_delay", anim.GetFrameDelay()},
                    {"animate", anim.GetAnimate()},
                    {"loop", anim.GetLoop()},
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
