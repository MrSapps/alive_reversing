#include "stdafx.h"
#include "QuikSave.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "ResourceManager.hpp"
#include "PathData.hpp"
#include "Map.hpp"
#include "Abe.hpp"
#include "Glukkon.hpp"
#include "UXB.hpp"
#include "HoistRocksEffect.hpp"
#include "Water.hpp"
#include "LaughingGas.hpp"
#include "../relive_lib/GameObjects/BoomMachinePipe.hpp"
#include "../relive_lib/GameObjects/RollingBallShaker.hpp"
#include "../relive_lib/GameObjects/ParamiteWeb.hpp"
#include "InvisibleEffect.hpp"
#include "../relive_lib/GameObjects/CircularFade.hpp"
#include "../relive_lib/GameObjects/ZapLine.hpp"
#include "Shrykull.hpp"
#include "../relive_lib/GameObjects/ScreenClipper.hpp"
#include "../relive_lib/GameObjects/BirdPortalTerminator.hpp"
#include "../relive_lib/GameObjects/OrbWhirlWind.hpp"
#include "../relive_lib/GameObjects/ThrowableTotalIndicator.hpp"
#include "../relive_lib/GameObjects/DeathGas.hpp"
#include "../relive_lib/GameObjects/PossessionFlicker.hpp"
#include "ExplosionSet.hpp"
#include "../relive_lib/GameObjects/GroundExplosion.hpp"
#include "../relive_lib/GameObjects/Electrocute.hpp"
#include "../relive_lib/GameObjects/PaletteOverwriter.hpp"
#include "Bullet.hpp"
#include "../relive_lib/GameObjects/BulletShell.hpp"
#include "../relive_lib/GameObjects/Door.hpp"
#include "../relive_lib/GameObjects/ElectricWall.hpp"
#include "ParamiteWebLine.hpp"
#include "PullRingRope.hpp"
#include "SligGetPantsAndWings.hpp"
#include "CrawlingSligButton.hpp"
#include "BrewMachine.hpp"
#include "LevelLoader.hpp"
#include "../relive_lib/GameObjects/BackgroundAnimation.hpp"
#include "GlukkonSwitch.hpp"
#include "TorturedMudokon.hpp"
#include "DoorBlocker.hpp"
#include "DoorFlame.hpp"
#include "StatusLight.hpp"
#include "../relive_lib/GameObjects/TimedMine.hpp"
#include "MovingBomb.hpp"
#include "../relive_lib/GameObjects/Mine.hpp"
#include "BoneBag.hpp"
#include "RockSack.hpp"
#include "MeatSack.hpp"
#include "../relive_lib/GameObjects/BoomMachine.hpp"
#include "../relive_lib/ListOrderSaveState.hpp"
#include "../relive_lib/GameObjects/ZzzSpawner.hpp"
#include "SlurgSpawner.hpp"
#include "SlogSpawner.hpp"
#include "../relive_lib/GameObjects/RollingBallStopper.hpp"
#include "RollingBall.hpp"
#include "FootSwitch.hpp"
#include "Lever.hpp"
#include "MotionDetector.hpp"
#include "SecurityDoor.hpp"
#include "MultiSwitchController.hpp"
#include "Teleporter.hpp"
#include "FallingItem.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/WorldStatic.hpp"
#include "../relive_lib/GameObjects/AirExplosion.hpp"
#include "../relive_lib/GameObjects/Alarm.hpp"
#include "../relive_lib/GameObjects/Blood.hpp"
#include "../relive_lib/GameObjects/DeathBirdParticle.hpp"
#include "../relive_lib/GameObjects/Fade.hpp"
#include "../relive_lib/GameObjects/Flash.hpp"
#include "../relive_lib/GameObjects/Gibs.hpp"
#include "../relive_lib/GameObjects/Particle.hpp"
#include "../relive_lib/GameObjects/ParticleBurst.hpp"
#include "../relive_lib/GameObjects/ScreenShake.hpp"
#include "../relive_lib/GameObjects/Spark.hpp"
#include "../relive_lib/GameObjects/ZapSpark.hpp"
#include "GameSpeak.hpp"
#include "../relive_lib/GameObjects/ThrowableArray.hpp"
#include "LCDStatusBoard.hpp"
#include "LCDScreen.hpp"
#include "DDCheat.hpp"
#include "../relive_lib/Events.hpp"
#include "WorkWheel.hpp"
#include "Drill.hpp"
#include "../relive_lib/data_conversion/file_system.hpp"
#include "LiftPoint.hpp"
#include "LiftMover.hpp"
#include "../relive_lib/GameObjects/TrapDoor.hpp"
#include "../relive_lib/GameObjects/TimerTrigger.hpp"
#include "SlamDoor.hpp"
#include "../relive_lib/GameObjects/Leaf.hpp"
#include "../relive_lib/GameObjects/SnoozeParticle.hpp"
#include "../relive_lib/GameObjects/InvisibleSwitch.hpp"
#include "MusicTrigger.hpp"
#include "SecurityOrb.hpp"
#include "../relive_lib/GameObjects/GasEmitter.hpp"
#include "FlyingSlig.hpp"
#include "SlapLock.hpp"
#include "SlapLockWhirlWind.hpp"
#include "../relive_lib/GameObjects/AbilityRing.hpp"
#include "../relive_lib/Engine.hpp"
#include "Slurg.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "Rock.hpp"
#include "Meat.hpp"
#include "Bone.hpp"
#include "../relive_lib/GameObjects/Dove.hpp"
#include "MineCar.hpp"
#include "Slig.hpp"
#include "SligSpawner.hpp"
#include "ScrabSpawner.hpp"
#include "GameEnderController.hpp"
#include "Paramite.hpp"
#include "BirdPortal.hpp"
#include "ColourfulMeter.hpp"
#include "MinesAlarm.hpp"
#include "EvilFart.hpp"
#include "CrawlingSlig.hpp"
#include "Fleech.hpp"
#include "Greeter.hpp"
#include "Slog.hpp"
#include "../relive_lib/GameObjects/Grenade.hpp"
#include "Mudokon.hpp"
#include "../relive_lib/FatalError.hpp"
#include "../relive_lib/BinaryPath.hpp"
#include "FlyingSligSpawner.hpp"
#include "Scrab.hpp"
#include "Engine.hpp"
#include "MainMenu.hpp" // only for global gSavedKilledMudsPerZulag

#include "nlohmann/json.hpp" // TODO: temp
#include "../relive_lib/data_conversion/AESaveSerialization.hpp"

extern u8 sRandomSeed; // Math.cpp

Quicksave QuikSave::gActiveQuicksaveData;
SaveFileRec QuikSave::gSaveFileRecords[128];
s32 QuikSave::gSavedGameToLoadIdx;
s32 QuikSave::gTotalSaveFilesCount;

static void ConvertObjectSaveStateDataToJson(nlohmann::json& j, ReliveTypes type, const SerializedObjectData& pData)
{
    switch (type)
    {
        case ::ReliveTypes::eSligSpawner:
            j.push_back(*pData.ReadTmpPtr<::SligSpawnerSaveState>());
            break;
        case ::ReliveTypes::eLiftMover:
            j.push_back(*pData.ReadTmpPtr<::LiftMoverSaveState>());
            break;

        case ::ReliveTypes::eBone:
            j.push_back(*pData.ReadTmpPtr<::BoneSaveState>());
            break;

        case ::ReliveTypes::eDove:
            j.push_back(*pData.ReadTmpPtr<::DoveSaveState>());
            break;

        case ::ReliveTypes::eLCDScreen:
            j.push_back(*pData.ReadTmpPtr<::LCDScreenSaveState>());
            break;

        case ::ReliveTypes::eMinesAlarm:
            j.push_back(*pData.ReadTmpPtr<::MinesAlarmSaveState>());
            break;

        case ::ReliveTypes::eCrawlingSlig:
            j.push_back(*pData.ReadTmpPtr<::CrawlingSligSaveState>());
            break;

        case ::ReliveTypes::eDrill:
            j.push_back(*pData.ReadTmpPtr<::DrillSaveState>());
            break;

        case ::ReliveTypes::eEvilFart:
            j.push_back(*pData.ReadTmpPtr<::EvilFartSaveState>());
            break;

        case ::ReliveTypes::eFleech:
            j.push_back(*pData.ReadTmpPtr<::FleechSaveState>());
            break;

        case ::ReliveTypes::eFlyingSlig:
            j.push_back(*pData.ReadTmpPtr<::FlyingSligSaveState>());
            break;

        case ::ReliveTypes::eFlyingSligSpawner:
            j.push_back(*pData.ReadTmpPtr<::FlyingSligSpawnerSaveState>());
            break;

        case ::ReliveTypes::eGameEnderController:
            j.push_back(*pData.ReadTmpPtr<::GameEnderControllerSaveState>());
            break;

        case ::ReliveTypes::eSlapLock_OrbWhirlWind:
            j.push_back(*pData.ReadTmpPtr<::SlapLockWhirlWindSaveState>());
            break;

        case ::ReliveTypes::eSlapLock:
            j.push_back(*pData.ReadTmpPtr<::SlapLockSaveState>());
            break;

        case ::ReliveTypes::eGreeter:
            j.push_back(*pData.ReadTmpPtr<::GreeterSaveState>());
            break;

        case ::ReliveTypes::eGrenade:
            j.push_back(*pData.ReadTmpPtr<::GrenadeSaveState>());
            break;

        case ::ReliveTypes::eGlukkon:
            j.push_back(*pData.ReadTmpPtr<::GlukkonSaveState>());
            break;

        case ::ReliveTypes::eAbe:
            j.push_back(*pData.ReadTmpPtr<::AbeSaveState>());
            break;

        case ::ReliveTypes::eLiftPoint:
            j.push_back(*pData.ReadTmpPtr<::LiftPointSaveState>());
            break;

        case ::ReliveTypes::eMudokon:
        case ::ReliveTypes::eRingOrLiftMud:
            j.push_back(*pData.ReadTmpPtr<::MudokonSaveState>());
            break;

        case ::ReliveTypes::eMeat:
            j.push_back(*pData.ReadTmpPtr<::MeatSaveState>());
            break;

        case ::ReliveTypes::eMineCar:
            j.push_back(*pData.ReadTmpPtr<::MineCarSaveState>());
            break;

        case ::ReliveTypes::eParamite:
            j.push_back(*pData.ReadTmpPtr<::ParamiteSaveState>());
            break;

        case ::ReliveTypes::eBirdPortal:
            j.push_back(*pData.ReadTmpPtr<::BirdPortalSaveState>());
            break;

        case ::ReliveTypes::eThrowableArray:
            j.push_back(*pData.ReadTmpPtr<::ThrowableArraySaveState>());
            break;

        case ::ReliveTypes::eAbilityRing:
            j.push_back(*pData.ReadTmpPtr<::AbilityRingSaveState>());
            break;

        case ::ReliveTypes::eWater:
            j.push_back(*pData.ReadTmpPtr<::WaterSaveState>());
            break;

        case ::ReliveTypes::eHoist:
            j.push_back(*pData.ReadTmpPtr<::HoistRocksEffectSaveState>());
            break;

        case ::ReliveTypes::eAnimationState:
            j.push_back(*pData.ReadTmpPtr<::AnimationStateSaveState>());
            break;

        case ::ReliveTypes::eAliveObjectState:
            j.push_back(*pData.ReadTmpPtr<::AliveObjectStateSaveState>());
            break;

        case ::ReliveTypes::eFallingItem:
            j.push_back(*pData.ReadTmpPtr<::FallingItemSaveState>());
            break;

        case ::ReliveTypes::eTeleporter:
            j.push_back(*pData.ReadTmpPtr<::TeleporterSaveState>());
            break;

        case ::ReliveTypes::eMultiSwitchController:
            j.push_back(*pData.ReadTmpPtr<::MultiSwitchControllerSaveState>());
            break;

        case ::ReliveTypes::eSecurityDoor:
            j.push_back(*pData.ReadTmpPtr<::SecurityDoorSaveState>());
            break;

        case ::ReliveTypes::eMotionDetector:
            j.push_back(*pData.ReadTmpPtr<::MotionDetectorSaveState>());
            break;

        case ::ReliveTypes::eLever:
            j.push_back(*pData.ReadTmpPtr<::LeverSaveState>());
            break;

        case ::ReliveTypes::eFootSwitch:
            j.push_back(*pData.ReadTmpPtr<::FootSwitchSaveState>());
            break;

        case ::ReliveTypes::eRollingBall:
            j.push_back(*pData.ReadTmpPtr<::RollingBallSaveState>());
            break;

        case ::ReliveTypes::eRollingBallStopper:
            j.push_back(*pData.ReadTmpPtr<::RollingBallStopperSaveState>());
            break;

        case ::ReliveTypes::eSlogSpawner:
            j.push_back(*pData.ReadTmpPtr<::SlogSpawnerSaveState>());
            break;

        case ::ReliveTypes::eSlurgSpawner:
            j.push_back(*pData.ReadTmpPtr<::SlurgSpawnerSaveState>());
            break;

        case ::ReliveTypes::eZzzSpawner:
            j.push_back(*pData.ReadTmpPtr<::ZzzSpawnerSaveState>());
            break;

        case ::ReliveTypes::eBoomMachine:
            j.push_back(*pData.ReadTmpPtr<::BoomMachineSaveState>());
            break;

        case ::ReliveTypes::eMeatSack:
            j.push_back(*pData.ReadTmpPtr<::MeatSackSaveState>());
            break;

        case ::ReliveTypes::eRockSack:
            j.push_back(*pData.ReadTmpPtr<::RockSackSaveState>());
            break;

        case ::ReliveTypes::eBoneBag:
            j.push_back(*pData.ReadTmpPtr<::BoneBagSaveState>());
            break;

        case ::ReliveTypes::eMine:
            j.push_back(*pData.ReadTmpPtr<::MineSaveState>());
            break;

        case ::ReliveTypes::eMovingBomb:
            j.push_back(*pData.ReadTmpPtr<::MovingBombSaveState>());
            break;

        case ::ReliveTypes::eTimedMine:
            j.push_back(*pData.ReadTmpPtr<::TimedMineSaveState>());
            break;

        case ::ReliveTypes::eDoorFlame:
            j.push_back(*pData.ReadTmpPtr<::DoorFlameSaveState>());
            break;

        case ::ReliveTypes::eStatusLight:
            j.push_back(*pData.ReadTmpPtr<::StatusLightSaveState>());
            break;

        case ::ReliveTypes::eDoorBlocker:
            j.push_back(*pData.ReadTmpPtr<::DoorBlockerSaveState>());
            break;

        case ::ReliveTypes::eTorturedMud:
            j.push_back(*pData.ReadTmpPtr<::TorturedMudokonSaveState>());
            break;

        case ::ReliveTypes::eGlukkonSwitch:
            j.push_back(*pData.ReadTmpPtr<::GlukkonSwitchSaveState>());
            break;

        case ::ReliveTypes::eBackgroundAnimation:
            j.push_back(*pData.ReadTmpPtr<::BackgroundAnimationSaveState>());
            break;

        case ::ReliveTypes::eLevelLoader:
            j.push_back(*pData.ReadTmpPtr<::LevelLoaderSaveState>());
            break;

        case ::ReliveTypes::eBrewMachine:
            j.push_back(*pData.ReadTmpPtr<::BrewMachineSaveState>());
            break;

        case ::ReliveTypes::eCrawlingSligButton:
            j.push_back(*pData.ReadTmpPtr<::CrawlingSligButtonSaveState>());
            break;

        case ::ReliveTypes::eGasCountDown:
            j.push_back(*pData.ReadTmpPtr<::GasCountDownSaveState>());
            break;

        case ::ReliveTypes::eSligGetPantsOrWings:
            j.push_back(*pData.ReadTmpPtr<::SligGetPantsAndWingsSaveState>());
            break;

        case ::ReliveTypes::ePullRingRope:
            j.push_back(*pData.ReadTmpPtr<::PullRingRopeSaveState>());
            break;

        case ::ReliveTypes::eParamiteWebLine:
            j.push_back(*pData.ReadTmpPtr<::ParamiteWebLineSaveState>());
            break;

        case ::ReliveTypes::eElectricWall:
            j.push_back(*pData.ReadTmpPtr<::ElectricWallSaveState>());
            break;

        case ::ReliveTypes::eDoor:
            j.push_back(*pData.ReadTmpPtr<::DoorSaveState>());
            break;

        case ::ReliveTypes::eBulletShell:
            j.push_back(*pData.ReadTmpPtr<::BulletShellSaveState>());
            break;

        case ::ReliveTypes::eBullet:
            j.push_back(*pData.ReadTmpPtr<::BulletSaveState>());
            break;

        case ::ReliveTypes::eColourfulMeter:
            j.push_back(*pData.ReadTmpPtr<::ColourfulMeterSaveState>());
            break;

        case ::ReliveTypes::ePalOverwriter:
            j.push_back(*pData.ReadTmpPtr<::PalleteOverwriterSaveState>());
            break;

        case ::ReliveTypes::eElectrocute:
            j.push_back(*pData.ReadTmpPtr<::ElectrocuteSaveState>());
            break;

        case ::ReliveTypes::eGroundExplosion:
            j.push_back(*pData.ReadTmpPtr<::GroundExplosionSaveState>());
            break;

        case ::ReliveTypes::eExplosionSet:
            j.push_back(*pData.ReadTmpPtr<::ExplosionSetSaveState>());
            break;

        case ::ReliveTypes::ePossessionFlicker:
            j.push_back(*pData.ReadTmpPtr<::PossessionFlickerSaveState>());
            break;

        case ::ReliveTypes::eDeathGas:
            j.push_back(*pData.ReadTmpPtr<::DeathGasSaveState>());
            break;

        case ::ReliveTypes::eThrowableTotalIndicator:
            j.push_back(*pData.ReadTmpPtr<::ThrowableTotalIndicatorSaveState>());
            break;

        case ::ReliveTypes::eOrbWhirlWind:
            j.push_back(*pData.ReadTmpPtr<::OrbWhirlWindSaveState>());
            break;

        case ::ReliveTypes::eBirdPortalTerminator:
            j.push_back(*pData.ReadTmpPtr<::BirdPortalTerminatorSaveState>());
            break;

        case ::ReliveTypes::eScreenClipper:
            j.push_back(*pData.ReadTmpPtr<::ScreenClipperSaveState>());
            break;

        case ::ReliveTypes::eShrykull:
            j.push_back(*pData.ReadTmpPtr<::ShrykullSaveState>());
            break;

        case ::ReliveTypes::eZapLine:
            j.push_back(*pData.ReadTmpPtr<::ZapLineSaveState>());
            break;

        case ::ReliveTypes::eCircularFade:
            j.push_back(*pData.ReadTmpPtr<::CircularFadeSaveState>());
            break;

        case ::ReliveTypes::eInvisibleEffect:
            j.push_back(*pData.ReadTmpPtr<::InvisibleEffectSaveState>());
            break;

        case ::ReliveTypes::eParamiteWeb:
            j.push_back(*pData.ReadTmpPtr<::ParamiteWebSaveState>());
            break;

        case ::ReliveTypes::eRollingBallShaker:
            j.push_back(*pData.ReadTmpPtr<::RollingBallShakerSaveState>());
            break;

        case ::ReliveTypes::eBoomMachinePipe:
            j.push_back(*pData.ReadTmpPtr<::BoomMachinePipeSaveState>());
            break;

        case ::ReliveTypes::eEventsState:
            j.push_back(*pData.ReadTmpPtr<::EventsSaveState>());
            break;

        case ::ReliveTypes::eListOrder:
            j.push_back(*pData.ReadTmpPtr<::ListOrderSaveState>());
            break;

        case ::ReliveTypes::eGameSpeak:
            j.push_back(*pData.ReadTmpPtr<::GameSpeakSaveState>());
            break;

        case ::ReliveTypes::eGameSpeakListener:
            j.push_back(*pData.ReadTmpPtr<::GameSpeakListenerSaveState>());
            break;



        case ::ReliveTypes::eRock:
            j.push_back(*pData.ReadTmpPtr<::RockSaveState>());
            break;

        case ::ReliveTypes::eScrab:
            j.push_back(*pData.ReadTmpPtr<::ScrabSaveState>());
            break;

        case ::ReliveTypes::eScrabSpawner:
            j.push_back(*pData.ReadTmpPtr<::ScrabSpawnerSaveState>());
            break;

        case ::ReliveTypes::eInvisibleSwitch:
            j.push_back(*pData.ReadTmpPtr<::InvisibleSwitchSaveState>());
            break;

        case ::ReliveTypes::eMusicTrigger:
            j.push_back(*pData.ReadTmpPtr<::MusicTriggerSaveState>());
            break;

        case ::ReliveTypes::eSecurityOrb:
            j.push_back(*pData.ReadTmpPtr<::SecurityOrbSaveState>());
            break;

        case ::ReliveTypes::eGasEmitter:
            j.push_back(*pData.ReadTmpPtr<::GasEmitterSaveState>());
            break;

        case ::ReliveTypes::eLeaf:
            j.push_back(*pData.ReadTmpPtr<::LeafSaveState>());
            break;

        case ::ReliveTypes::eSnoozeParticle:
            j.push_back(*pData.ReadTmpPtr<::SnoozeParticleSaveState>());
            break;

        case ::ReliveTypes::eSlamDoor:
            j.push_back(*pData.ReadTmpPtr<::SlamDoorSaveState>());
            break;

        case ::ReliveTypes::eSlig:
            j.push_back(*pData.ReadTmpPtr<::SligSaveState>());
            break;

        case ::ReliveTypes::eSlog:
            j.push_back(*pData.ReadTmpPtr<::SlogSaveState>());
            break;

        case ::ReliveTypes::eSlurg:
            j.push_back(*pData.ReadTmpPtr<::SlurgSaveState>());
            break;

        case ::ReliveTypes::eTimerTrigger:
            j.push_back(*pData.ReadTmpPtr<::TimerTriggerSaveState>());
            break;

        case ::ReliveTypes::eTrapDoor:
            j.push_back(*pData.ReadTmpPtr<::TrapDoorSaveState>());
            break;

        case ::ReliveTypes::eUXB:
            j.push_back(*pData.ReadTmpPtr<::UXBSaveState>());
            break;

        case ::ReliveTypes::eWorkWheel:
            j.push_back(*pData.ReadTmpPtr<::WorkWheelSaveState>());
            break;

        case ::ReliveTypes::eSpark:
            j.push_back(*pData.ReadTmpPtr<::SparkSaveState>());
            break;

        case ::ReliveTypes::eParticle:
            j.push_back(*pData.ReadTmpPtr<::ParticleSaveState>());
            break;

        case ::ReliveTypes::eGibs:
            j.push_back(*pData.ReadTmpPtr<::GibsSaveState>());
            break;

        case ::ReliveTypes::eParticleBurst:
            j.push_back(*pData.ReadTmpPtr<::ParticleBurstSaveState>());
            break;

        case ::ReliveTypes::eBlood:
            j.push_back(*pData.ReadTmpPtr<::BloodSaveState>());
            break;

        case ::ReliveTypes::eDeathBird:
            j.push_back(*pData.ReadTmpPtr<::DeathBirdParticleSaveState>());
            break;

        case ::ReliveTypes::eZapSpark:
            j.push_back(*pData.ReadTmpPtr<::ZapSparkSaveState>());
            break;

        case ::ReliveTypes::eFlash:
            j.push_back(*pData.ReadTmpPtr<::FlashSaveState>());
            break;

        case ::ReliveTypes::eFade:
        case ::ReliveTypes::eMainMenuTransistion:
            j.push_back(*pData.ReadTmpPtr<::FadeSaveState>());
            break;

        case ::ReliveTypes::eAlarm:
            j.push_back(*pData.ReadTmpPtr<::AlarmSaveState>());
            break;

        case ::ReliveTypes::eScreenShake:
            j.push_back(*pData.ReadTmpPtr<::ScreenShakeSaveState>());
            break;

        case ::ReliveTypes::eAirExplosion:
            j.push_back(*pData.ReadTmpPtr<::AirExplosionSaveState>());
            break;

        case ::ReliveTypes::eLaughingGas:
            j.push_back(*pData.ReadTmpPtr<::LaughingGasSaveState>());
            break;

        case ::ReliveTypes::eWorldStatics:
            j.push_back(*pData.ReadTmpPtr<::WorldStaticsSaveState>());
            break;

        default:
            ALIVE_FATAL("No create json save state for type %d", static_cast<s32>(type));
    }
}


static void RestoreObjectState(ReliveTypes type, SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map, std::vector<std::function<void()>>& afterCamera)
{
    switch (type)
    {
        case ::ReliveTypes::eGameSpeak:
            return GameSpeak::RestoreFromSaveState(*pData.ReadTmpPtr<GameSpeakSaveState>());

        case ::ReliveTypes::eLaughingGas:
        {
            // The gas is made by the camera (from its TLV), so it is only put back once that has
            const LaughingGasSaveState state = *pData.ReadTmpPtr<LaughingGasSaveState>();
            afterCamera.push_back([state]() { LaughingGas::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eAlarm:
        {
            const AlarmSaveState state = *pData.ReadTmpPtr<AlarmSaveState>();
            if (state.mIsTlvAlarm)
            {
                // The camera makes it (from its TLV)
                afterCamera.push_back([state]() { Alarm::RestoreFromSaveState(state); });
            }
            else
            {
                Alarm::CreateFromSaveState(state, resMan, map);
            }
            return;
        }

        case ::ReliveTypes::eFallingItem:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const FallingItemSaveState state = *pData.ReadTmpPtr<FallingItemSaveState>();
            afterCamera.push_back([state]() { FallingItem::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eTeleporter:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const TeleporterSaveState state = *pData.ReadTmpPtr<TeleporterSaveState>();
            afterCamera.push_back([state]() { Teleporter::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eMultiSwitchController:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MultiSwitchControllerSaveState state = *pData.ReadTmpPtr<MultiSwitchControllerSaveState>();
            afterCamera.push_back([state]() { MultiSwitchController::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eSecurityDoor:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SecurityDoorSaveState state = *pData.ReadTmpPtr<SecurityDoorSaveState>();
            afterCamera.push_back([state]() { SecurityDoor::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eMotionDetector:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MotionDetectorSaveState state = *pData.ReadTmpPtr<MotionDetectorSaveState>();
            afterCamera.push_back([state]() { MotionDetector::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eLever:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const LeverSaveState state = *pData.ReadTmpPtr<LeverSaveState>();
            afterCamera.push_back([state]() { Lever::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eFootSwitch:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const FootSwitchSaveState state = *pData.ReadTmpPtr<FootSwitchSaveState>();
            afterCamera.push_back([state]() { FootSwitch::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eRollingBall:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const RollingBallSaveState state = *pData.ReadTmpPtr<RollingBallSaveState>();
            afterCamera.push_back([state]() { RollingBall::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eRollingBallStopper:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const RollingBallStopperSaveState state = *pData.ReadTmpPtr<RollingBallStopperSaveState>();
            afterCamera.push_back([state]() { RollingBallStopper::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eSlogSpawner:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SlogSpawnerSaveState state = *pData.ReadTmpPtr<SlogSpawnerSaveState>();
            afterCamera.push_back([state]() { SlogSpawner::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eSlurgSpawner:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SlurgSpawnerSaveState state = *pData.ReadTmpPtr<SlurgSpawnerSaveState>();
            afterCamera.push_back([state]() { SlurgSpawner::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eZzzSpawner:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ZzzSpawnerSaveState state = *pData.ReadTmpPtr<ZzzSpawnerSaveState>();
            afterCamera.push_back([state]() { ZzzSpawner::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eBoomMachine:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BoomMachineSaveState state = *pData.ReadTmpPtr<BoomMachineSaveState>();
            afterCamera.push_back([state]() { BoomMachine::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eMeatSack:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MeatSackSaveState state = *pData.ReadTmpPtr<MeatSackSaveState>();
            afterCamera.push_back([state]() { MeatSack::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eRockSack:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const RockSackSaveState state = *pData.ReadTmpPtr<RockSackSaveState>();
            afterCamera.push_back([state]() { RockSack::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eBoneBag:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BoneBagSaveState state = *pData.ReadTmpPtr<BoneBagSaveState>();
            afterCamera.push_back([state]() { BoneBag::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eMine:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MineSaveState state = *pData.ReadTmpPtr<MineSaveState>();
            afterCamera.push_back([state]() { Mine::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eMovingBomb:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MovingBombSaveState state = *pData.ReadTmpPtr<MovingBombSaveState>();
            afterCamera.push_back([state]() { MovingBomb::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eTimedMine:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const TimedMineSaveState state = *pData.ReadTmpPtr<TimedMineSaveState>();
            afterCamera.push_back([state]() { TimedMine::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eStatusLight:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const StatusLightSaveState state = *pData.ReadTmpPtr<StatusLightSaveState>();
            afterCamera.push_back([state]() { StatusLight::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eDoorFlame:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const DoorFlameSaveState state = *pData.ReadTmpPtr<DoorFlameSaveState>();
            afterCamera.push_back([state]() { DoorFlame::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eDoorBlocker:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const DoorBlockerSaveState state = *pData.ReadTmpPtr<DoorBlockerSaveState>();
            afterCamera.push_back([state]() { DoorBlocker::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eTorturedMud:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const TorturedMudokonSaveState state = *pData.ReadTmpPtr<TorturedMudokonSaveState>();
            afterCamera.push_back([state]() { TorturedMudokon::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eGlukkonSwitch:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const GlukkonSwitchSaveState state = *pData.ReadTmpPtr<GlukkonSwitchSaveState>();
            afterCamera.push_back([state]() { GlukkonSwitch::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eBackgroundAnimation:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BackgroundAnimationSaveState state = *pData.ReadTmpPtr<BackgroundAnimationSaveState>();
            afterCamera.push_back([state]() { BackgroundAnimation::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eLevelLoader:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const LevelLoaderSaveState state = *pData.ReadTmpPtr<LevelLoaderSaveState>();
            afterCamera.push_back([state]() { LevelLoader::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eBrewMachine:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BrewMachineSaveState state = *pData.ReadTmpPtr<BrewMachineSaveState>();
            afterCamera.push_back([state]() { BrewMachine::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eCrawlingSligButton:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const CrawlingSligButtonSaveState state = *pData.ReadTmpPtr<CrawlingSligButtonSaveState>();
            afterCamera.push_back([state]() { CrawlingSligButton::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eGasCountDown:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const GasCountDownSaveState state = *pData.ReadTmpPtr<GasCountDownSaveState>();
            afterCamera.push_back([state]() { GasCountDown::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eSligGetPantsOrWings:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SligGetPantsAndWingsSaveState state = *pData.ReadTmpPtr<SligGetPantsAndWingsSaveState>();
            afterCamera.push_back([state]() { SligGetPantsAndWings::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::ePullRingRope:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const PullRingRopeSaveState state = *pData.ReadTmpPtr<PullRingRopeSaveState>();
            afterCamera.push_back([state]() { PullRingRope::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eParamiteWebLine:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ParamiteWebLineSaveState state = *pData.ReadTmpPtr<ParamiteWebLineSaveState>();
            afterCamera.push_back([state]() { ParamiteWebLine::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eElectricWall:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ElectricWallSaveState state = *pData.ReadTmpPtr<ElectricWallSaveState>();
            afterCamera.push_back([state]() { ElectricWall::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eDoor:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const DoorSaveState state = *pData.ReadTmpPtr<DoorSaveState>();
            afterCamera.push_back([state]() { Door::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eColourfulMeter:
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ColourfulMeterSaveState state = *pData.ReadTmpPtr<ColourfulMeterSaveState>();
            afterCamera.push_back([state]() { ColourfulMeter::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eExplosionSet:
        {
            // The one explosion set lives on from the camera it was made in, which the camera now loaded makes
            // use of (it only sets it up), so it is made here under its own id and put back once the camera has
            const ExplosionSetSaveState state = *pData.ReadTmpPtr<ExplosionSetSaveState>();
            relive_new ExplosionSet(resMan, map);
            afterCamera.push_back([state]() { ExplosionSet::RestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eEventsState:
        {
            // Names objects the camera may make
            const EventsSaveState state = *pData.ReadTmpPtr<EventsSaveState>();
            afterCamera.push_back([state]() { EventsRestoreFromSaveState(state); });
            return;
        }

        case ::ReliveTypes::eWorldStatics:
            // Put back when the save is loaded, before any object is made, see RestoreWorldStatics
            pData.SkipRead(sizeof(WorldStaticsSaveState));
            return;

        case ::ReliveTypes::eBulletShell:
            return ::BulletShell::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBullet:
            return ::Bullet::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::ePalOverwriter:
            return ::PalleteOverwriter::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eElectrocute:
            return ::Electrocute::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGroundExplosion:
            return ::GroundExplosion::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::ePossessionFlicker:
            return ::PossessionFlicker::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eDeathGas:
            return ::DeathGas::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eThrowableTotalIndicator:
            return ::ThrowableTotalIndicator::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eOrbWhirlWind:
            return ::OrbWhirlWind::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBirdPortalTerminator:
            return ::BirdPortalTerminator::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eScreenClipper:
            return ::ScreenClipper::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eShrykull:
            return ::Shrykull::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eZapLine:
            return ::ZapLine::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eCircularFade:
            return ::CircularFade::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eInvisibleEffect:
            return ::InvisibleEffect::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eParamiteWeb:
            return ::ParamiteWeb::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eRollingBallShaker:
            return ::RollingBallShaker::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBoomMachinePipe:
            return ::BoomMachinePipe::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSpark:
            return Spark::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eParticle:
            return Particle::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGibs:
            return Gibs::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eParticleBurst:
            return ParticleBurst::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBlood:
            return Blood::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eDeathBird:
            return DeathBirdParticle::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eZapSpark:
            return ZapSpark::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eFlash:
            return Flash::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eFade:
        case ::ReliveTypes::eMainMenuTransistion:
            return Fade::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eScreenShake:
            return ScreenShake::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eAirExplosion:
            return AirExplosion::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSligSpawner:
            return SligSpawner::CreateFromSaveState(pData, resMan, map);
            
        case ::ReliveTypes::eLiftMover:
            return LiftMover::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBone:
            return Bone::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eDove:
            return Dove::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eMinesAlarm:
            return MinesAlarm::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eCrawlingSlig:
            return CrawlingSlig::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eDrill:
            return Drill::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eEvilFart:
            return EvilFart::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eFleech:
            return Fleech::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eFlyingSlig:
            return FlyingSlig::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eFlyingSligSpawner:
            return FlyingSligSpawner::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGameEnderController:
            return GameEnderController::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlapLock_OrbWhirlWind:
            return SlapLockWhirlWind::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlapLock:
            return SlapLock::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGreeter:
            return Greeter::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGrenade:
            return Grenade::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGlukkon:
            return Glukkon::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eAbe:
            return Abe::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eLiftPoint:
            return LiftPoint::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eMudokon:
        case ::ReliveTypes::eRingOrLiftMud:
            return Mudokon::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eMeat:
            return Meat::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eMineCar:
            return MineCar::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eParamite:
            return Paramite::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eBirdPortal:
        {
            const BirdPortalSaveState state = *pData.PeekTmpPtr<BirdPortalSaveState>();
            if (state.mBase.mFullState)
            {
                // Made by the camera (from its TLV), so it is put back once that has
                pData.SkipRead(sizeof(BirdPortalSaveState));
                afterCamera.push_back([state]() { BirdPortal::RestoreFromSaveState(state); });
                return;
            }
            return BirdPortal::CreateFromSaveState(pData, resMan, map);
        }

        case ::ReliveTypes::eThrowableArray:
            return ThrowableArray::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eAbilityRing:
            return AbilityRing::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eRock:
            return Rock::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eScrab:
            return Scrab::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eScrabSpawner:
            return ScrabSpawner::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eInvisibleSwitch:
            return InvisibleSwitch::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eMusicTrigger:
            return MusicTrigger::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSecurityOrb:
            return SecurityOrb::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eGasEmitter:
            return GasEmitter::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eLeaf:
            return Leaf::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSnoozeParticle:
            return SnoozeParticle::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlamDoor:
            return SlamDoor::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlig:
            return Slig::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlog:
            return Slog::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eSlurg:
            return Slurg::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eTimerTrigger:
            return TimerTrigger::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eTrapDoor:
            return TrapDoor::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eUXB:
            return UXB::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eWorkWheel:
            return WorkWheel::CreateFromSaveState(pData, resMan, map);

        case ::ReliveTypes::eLCDScreen:
            return LCDScreen::CreateFromSaveState(pData, resMan, map);

        default:
            ALIVE_FATAL("No create save state for type %d", static_cast<s32>(type));
    }
}

void ConvertObjectsStatesToJson(nlohmann::json& j, const SerializedObjectData& pData)
{
    pData.ReadRewind();
    while (pData.CanRead())
    {
        const SaveStateBase* pSaveStateBase = pData.PeekTmpPtr<SaveStateBase>();
        LOG_INFO("Converting type %d with size %u", static_cast<s32>(pSaveStateBase->mType), pSaveStateBase->mSize);
        const Guid objectId = pSaveStateBase->mObjectId;
        const Guid pathTlvId = pSaveStateBase->mPathTlvId;
        const FP lastLineYPos = pSaveStateBase->mLastLineYPos;
        ConvertObjectSaveStateDataToJson(j, pSaveStateBase->mType, pData);
        j.back()["base_object_id"] = objectId;
        j.back()["base_path_tlv_id"] = pathTlvId;
        if (pSaveStateBase->mObjectId.IsValid())
        {
            j.back()["base_last_line_y_pos"] = lastLineYPos;
        }
    }
}

void QuikSave::RestoreBlyData(PendingObjectRestoreData& pSaveData, ResourceManager& resMan, BaseMap& map)
{
    pSaveData.mObjectsStateData.ReadRewind();
    // The object an animation is for may be made by a later save (a lift makes its rope), so these go on once all are made
    std::vector<AnimationStateSaveState> animStates;
    std::vector<HoistRocksEffectSaveState> hoistStates;
    std::vector<WaterSaveState> waterStates;
    std::vector<AliveObjectStateSaveState> aliveStates;
    std::vector<GameSpeakListenerSaveState> listenerStates;
    std::vector<std::function<void()>> blobStates;
    // Every object that was saved has an animation record, in the order the objects were in
    std::vector<Guid> objectOrder;
    std::vector<Guid> drawablesOrder;
    std::vector<Guid> aliveOrder;
    std::vector<std::pair<Guid, s32>> animationsOrder;
    std::vector<Guid> platformsOrder;
    while (pSaveData.mObjectsStateData.CanRead())
    {
        const SaveStateBase* pSaveStateBase = pSaveData.mObjectsStateData.PeekTmpPtr<SaveStateBase>();
        if (pSaveStateBase->mType == ReliveTypes::eAnimationState)
        {
            objectOrder.push_back(pSaveStateBase->mObjectId);
        }
        if (pSaveStateBase->mType == ReliveTypes::eListOrder)
        {
            const ListOrderSaveState& order = *pSaveData.mObjectsStateData.ReadTmpPtr<ListOrderSaveState>();
            if (order.mList == ListOrderSaveState::List::eAnimations)
            {
                for (s32 i = 0; i < order.mCount; i++)
                {
                    animationsOrder.push_back({order.mIds[i], order.mOffsets[i]});
                }
            }
            else
            {
                std::vector<Guid>& ids = order.mList == ListOrderSaveState::List::eDrawables ? drawablesOrder
                    : order.mList == ListOrderSaveState::List::eAliveObjects                  ? aliveOrder
                                                                                              : platformsOrder;
                ids.assign(order.mIds, order.mIds + order.mCount);
            }
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eWater)
        {
            waterStates.push_back(*pSaveData.mObjectsStateData.ReadTmpPtr<WaterSaveState>());
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eHoist)
        {
            hoistStates.push_back(*pSaveData.mObjectsStateData.ReadTmpPtr<HoistRocksEffectSaveState>());
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eAnimationState)
        {
            animStates.push_back(*pSaveData.mObjectsStateData.ReadTmpPtr<AnimationStateSaveState>());
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eAliveObjectState)
        {
            aliveStates.push_back(*pSaveData.mObjectsStateData.ReadTmpPtr<AliveObjectStateSaveState>());
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eGameSpeakListener)
        {
            // The character may be made by the camera, so this is put on it once all are made
            listenerStates.push_back(*pSaveData.mObjectsStateData.ReadTmpPtr<GameSpeakListenerSaveState>());
            continue;
        }
        // The object made takes the id it was saved with, and what its constructor makes the ids that come from it.
        // Not Abe, who is restored in place and keeps his id
        if (pSaveStateBase->mObjectId.IsValid() && pSaveStateBase->mType != ReliveTypes::eAbe)
        {
            Guid::CreationScope idScope(pSaveStateBase->mObjectId);
            RestoreObjectState(pSaveStateBase->mType, pSaveData.mObjectsStateData, resMan, map, blobStates);
        }
        else
        {
            RestoreObjectState(pSaveStateBase->mType, pSaveData.mObjectsStateData, resMan, map, blobStates);
        }
        BaseGameObject::ApplySavedHeader(*pSaveStateBase);
    }

    // The camera's own objects (a lever, the rocks at a hoist) are made after these, so they get theirs then
    map.mAfterCameraObjects.clear();
    for (const HoistRocksEffectSaveState& hoistState : hoistStates)
    {
        map.mAfterCameraObjects.push_back([hoistState]() { HoistRocksEffect::RestoreSaveState(hoistState); });
    }
    for (const std::function<void()>& blobState : blobStates)
    {
        map.mAfterCameraObjects.push_back(blobState);
    }
    for (const WaterSaveState& waterState : waterStates)
    {
        map.mAfterCameraObjects.push_back([waterState]() { Water::RestoreSaveState(waterState); });
    }
    for (const AnimationStateSaveState& animState : animStates)
    {
        map.mAfterCameraObjects.push_back([animState]() { BaseGameObject::RestoreAnimationState(animState); });
    }
    for (const AliveObjectStateSaveState& aliveState : aliveStates)
    {
        map.mAfterCameraObjects.push_back([aliveState]() { BaseGameObject::RestoreAliveObjectState(aliveState); });
    }
    for (const GameSpeakListenerSaveState& listenerState : listenerStates)
    {
        map.mAfterCameraObjects.push_back([listenerState]() { BaseGameObject::RestoreListener(listenerState); });
    }
    map.mRestoreObjectOrder = objectOrder;
    map.mRestoreDrawablesOrder = drawablesOrder;
    map.mRestoreAliveOrder = aliveOrder;
    map.mRestoreAnimationsOrder = animationsOrder;
    map.mRestorePlatformsOrder = platformsOrder;
    // First: what the others put back may be on what this makes
    map.mAfterCameraObjects.insert(map.mAfterCameraObjects.begin(), [&map, objectOrder]() { map.MakeMissingTlvObjects(objectOrder); });
    map.mAfterCameraObjects.push_back([objectOrder]() { BaseGameObject::ReorderObjects(objectOrder); });
    map.mAfterCameraObjects.push_back([]() { BaseGameObject::PostRestoreAllFromTlv(); });
    map.mAfterCameraObjects.push_back([]() { BaseGameObject::MarkDrawnObjects(); });
    // What the objects the camera made did to the switches in their constructors is not part of the save
    map.mAfterCameraObjects.push_back([switchStates = QuikSave::gActiveQuicksaveData.mSwitchStates]()
                                      {
                                          for (u32 i = 0; i < ALIVE_COUNTOF(gSwitchStates.mData); i++)
                                          {
                                              if (gSwitchStates.mData[i] != switchStates.mData[i])
                                              {
                                                  LOG_WARNING("restore: a restore's objects changed switch %u to %d, put back to %d", i, static_cast<s32>(gSwitchStates.mData[i]), static_cast<s32>(switchStates.mData[i]));
                                              }
                                          }
                                          gSwitchStates = switchStates;
                                      });
    // The same for the statics: a constructor (the gas, a bee) changes them, as it did in the world that was saved
    map.mAfterCameraObjects.push_back([]()
                                      {
                                          const std::string before = WorldStatics::Describe();
                                          QuikSave::RestoreWorldStatics(QuikSave::gActiveQuicksaveData.mObjectsStateData);
                                          WorldStatics::LogChangedSince(before, "restore");
                                      });

    map.RestoreQuicksaveBlyData(pSaveData.mObjectBlyData);
    BaseGameObject::PostRestoreAll();
}

// The variables outside any object are put back before the objects are made, since making them changes some
void QuikSave::RestoreWorldStatics(const SerializedObjectData& pData)
{
    pData.ReadRewind();
    while (pData.CanRead())
    {
        const SaveStateBase* pBase = pData.PeekTmpPtr<SaveStateBase>();
        if (pBase->mType == ReliveTypes::eWorldStatics)
        {
            WorldStatics::RestoreFromSaveState(*pData.ReadTmpPtr<WorldStaticsSaveState>());
        }
        else
        {
            pData.SkipRead(pBase->mSize);
        }
    }
    pData.ReadRewind();
}

void Quicksave_LoadFromMemory_4C95A0(BaseMap& map)
{
    DestroyObjects();
    // The objects of the old world reset their TLVs as they went, which is not the live game's doing
    for (std::unique_ptr<BinaryPath>& pBinPath : map.GetLoadedPaths())
    {
        if (pBinPath)
        {
            pBinPath->ClearResetThisVisitFlags();
        }
    }
    EventsReset();
    gSkipGameObjectUpdates = true;
    // A door or teleporter the old world was part way through must not make the screen change that follows play its
    // transition: it takes updates the restore doesn't give
    map.mPendingTransition = BaseMap::PendingTransition::eNone_0;
    QuikSave::RestoreWorldInfo(QuikSave::gActiveQuicksaveData.mWorldInfo);
    QuikSave::RestoreWorldStatics(QuikSave::gActiveQuicksaveData.mObjectsStateData);
    gSwitchStates = QuikSave::gActiveQuicksaveData.mSwitchStates;
    map.mPendingSaveRestore = &QuikSave::gActiveQuicksaveData;
    map.SetActiveCam(
        QuikSave::gActiveQuicksaveData.mWorldInfo.mLevel,
        QuikSave::gActiveQuicksaveData.mWorldInfo.mPath,
        QuikSave::gActiveQuicksaveData.mWorldInfo.mCam,
        CameraSwapEffects::eInstantChange_0,{},
        1);
    map.mForceLoad = 1;
}

void QuikSave::LoadActive(BaseMap& map)
{
    map.GetResourceManager().RequestLoadingWait(LoadingIcon::eNow);
    Quicksave_LoadFromMemory_4C95A0(map);
}

void QuikSave::SaveToMemory_4C91A0(Quicksave& pSave, BaseMap& map)
{
    if (gAbe->mHealth > FP_FromInteger(0))
    {
        QuikSave::SaveWorldInfo(&pSave.mWorldInfo, map);
        pSave.mSwitchStates = gSwitchStates;

        pSave.mObjectsStateData.WriteRewind();
        for (s32 idx = 0; idx < gBaseGameObjects->Size(); idx++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(idx);
            if (!pObj)
            {
                break;
            }

            if (!pObj->GetDead())
            {
                pObj->SaveState(pSave.mObjectsStateData);
            }
        }

        pSave.mObjectsStateData.Write(WorldStatics::GetSaveState());
        BaseGameObject::SaveListOrders(pSave.mObjectsStateData);
        pSave.mObjectsStateData.Write(EventsGetSaveState());

        map.SaveQuicksaveBlyData(pSave.mObjectBlyData);
    }
}

static Quicksave sStashedQuicksave;

void QuikSave::StashActive()
{
    sStashedQuicksave = gActiveQuicksaveData;
}

void QuikSave::UnstashActive()
{
    gActiveQuicksaveData = sStashedQuicksave;
}

static Quicksave sStartQuicksave;

void QuikSave::StashStart()
{
    sStartQuicksave = gActiveQuicksaveData;
}

void QuikSave::UnstashStart()
{
    gActiveQuicksaveData = sStartQuicksave;
}

std::string QuikSave::RoundTripThroughJson()
{
    nlohmann::json j;
    to_json(j, gActiveQuicksaveData);
    Quicksave copy;
    from_json(j, copy);

    const SerializedObjectData& before = gActiveQuicksaveData.mObjectsStateData;
    const SerializedObjectData& after = copy.mObjectsStateData;
    std::string difference;
    std::size_t posBefore = 0;
    std::size_t posAfter = 0;
    while (posBefore < before.Size() && posAfter < after.Size())
    {
        const SaveStateBase& recordBefore = *reinterpret_cast<const SaveStateBase*>(before.Data() + posBefore);
        const SaveStateBase& recordAfter = *reinterpret_cast<const SaveStateBase*>(after.Data() + posAfter);
        if (recordBefore.mSize != recordAfter.mSize || recordBefore.mType != recordAfter.mType)
        {
            difference = "json: a record of type " + std::to_string(static_cast<s32>(recordBefore.mType)) + " came back as type " + std::to_string(static_cast<s32>(recordAfter.mType)) + " of another size";
            break;
        }
        if (std::memcmp(before.Data() + posBefore, after.Data() + posAfter, recordBefore.mSize) != 0)
        {
            std::size_t offset = 0;
            while (before.Data()[posBefore + offset] == after.Data()[posAfter + offset])
            {
                offset++;
            }
            difference = "json: a record of type " + std::to_string(static_cast<s32>(recordBefore.mType)) + " differs at byte " + std::to_string(offset) + " of " + std::to_string(recordBefore.mSize);
            for (std::size_t k = 0; k < 16 && offset + k < recordBefore.mSize; k++)
            {
                char hex[16];
                snprintf(hex, sizeof(hex), " %02x/%02x", before.Data()[posBefore + offset + k], after.Data()[posAfter + offset + k]);
                difference += hex;
            }
            break;
        }
        posBefore += recordBefore.mSize;
        posAfter += recordAfter.mSize;
    }
    if (difference.empty() && (posBefore != before.Size() || posAfter != after.Size()))
    {
        difference = "json: the saved objects came back as " + std::to_string(after.Size()) + " bytes from " + std::to_string(before.Size());
    }
    if (difference.empty() && (gActiveQuicksaveData.mObjectBlyData.Size() != copy.mObjectBlyData.Size() || std::memcmp(gActiveQuicksaveData.mObjectBlyData.Data(), copy.mObjectBlyData.Data(), copy.mObjectBlyData.Size()) != 0))
    {
        difference = "json: the flags of the TLVs differ";
    }
    // What is loaded next is what came back
    gActiveQuicksaveData = copy;
    return difference;
}

void QuikSave::DoQuicksave(BaseMap& map)
{
    map.GetResourceManager().RequestLoadingWait(LoadingIcon::eNow);
    QuikSave::SaveToMemory_4C91A0(gActiveQuicksaveData, map);
}

void QuikSave::RestoreWorldInfo(const Quicksave_WorldInfo& rInfo)
{
    // Read all fields bar the last
    for (s32 i = 0; i < ALIVE_COUNTOF(rInfo.field_18_saved_killed_muds_per_zulag); i++)
    {
        gSavedKilledMudsPerZulag.mData[i] = rInfo.field_18_saved_killed_muds_per_zulag[i];
    }

    // Last is read from another field
    gSavedKilledMudsPerZulag.mData[ALIVE_COUNTOF(gSavedKilledMudsPerZulag.mData) - 1] = rInfo.field_17_last_saved_killed_muds_per_path;

    gZulagNumber = rInfo.field_2C_current_zulag_number;
    gKilledMudokons = rInfo.mKilledMudokons;
    gRescuedMudokons = rInfo.mRescuedMudokons;
    gMudokonsInArea = rInfo.field_16_muds_in_area; // TODO: Check types
    gTotalMeterBars = rInfo.mTotalMeterBars;
    gbDrawMeterCountDown = rInfo.field_30_bDrawMeterCountDown;
    gDeathGasTimer = rInfo.mGasTimer;
    gAbeInvincible = rInfo.mAbeInvincible;
    gVisitedBonewerkz = rInfo.mVisitedBonewerkz;
    gVisitedBarracks = rInfo.mVisitedBarracks;
    gVisitedFeecoEnder = rInfo.mVisitedFeecoEnder;
    sGnFrame = rInfo.mGnFrame;
    sRandomSeed = rInfo.mRandomSeed;
    Leaf::SetRandIdx(rInfo.mLeafRandIdx);
    BaseGameObject::RestoreGuidCounter(rInfo.mGuidCounter);
}

void QuikSave::SaveWorldInfo(Quicksave_WorldInfo* pInfo, BaseMap& map)
{
    const PSX_RECT rect = sControlledCharacter->VGetBoundingRect();

    pInfo->mGnFrame = sGnFrame;
    pInfo->mLevel = map.mCurrentLevel;
    pInfo->mPath = map.mCurrentPath;
    pInfo->mCam = map.mCurrentCamera;

    for (s32 i = 0; i < ALIVE_COUNTOF(pInfo->field_18_saved_killed_muds_per_zulag); i++)
    {
        pInfo->field_18_saved_killed_muds_per_zulag[i] = gSavedKilledMudsPerZulag.mData[i];
    }

    pInfo->field_17_last_saved_killed_muds_per_path = gSavedKilledMudsPerZulag.mData[ALIVE_COUNTOF(gSavedKilledMudsPerZulag.mData) - 1];

    pInfo->field_2C_current_zulag_number = gZulagNumber;
    pInfo->mRescuedMudokons = gRescuedMudokons;
    pInfo->mKilledMudokons = gKilledMudokons;
    pInfo->field_16_muds_in_area = static_cast<s8>(gMudokonsInArea); // TODO: Check types
    pInfo->mTotalMeterBars = gTotalMeterBars;
    pInfo->field_30_bDrawMeterCountDown = gbDrawMeterCountDown;
    pInfo->mAbeInvincible = gAbeInvincible;
    pInfo->mVisitedBonewerkz = gVisitedBonewerkz;
    pInfo->mVisitedBarracks = gVisitedBarracks;
    pInfo->mVisitedFeecoEnder = gVisitedFeecoEnder;
    pInfo->mGasTimer = gDeathGasTimer;
    pInfo->mRandomSeed = sRandomSeed;
    pInfo->mLeafRandIdx = Leaf::GetRandIdx();
    pInfo->mGuidCounter = Guid::GetNextGuidCounter();
    pInfo->mControlledCharX = FP_GetExponent(sControlledCharacter->mXPos);
    pInfo->mControlledCharY = rect.h;
    pInfo->mControlledCharAtFullScale = sControlledCharacter->GetSpriteScale() == FP_FromDouble(1.0);
}

static s32 Sort_comparitor_4D42C0(const void* pSaveRecLeft, const void* pSaveRecRight)
{
    const s32 leftTime = reinterpret_cast<const SaveFileRec*>(pSaveRecLeft)->mLastWriteTimeStamp;
    const s32 rightTime = reinterpret_cast<const SaveFileRec*>(pSaveRecRight)->mLastWriteTimeStamp;

    if (leftTime <= rightTime)
    {
        return leftTime < rightTime;
    }
    else
    {
        return -1;
    }
}

void QuikSave::FindSaves(FileSystem& fs)
{
    gTotalSaveFilesCount = 0;

    fs.EnumerateDirectory("*.json", [](const char_type* fileName, u32 lastWriteTime)
                          {
                              if (gTotalSaveFilesCount < 128)
                              {
                                  size_t fileNameLen = strlen(fileName) - 5; // remove .json
                                  if (fileNameLen > 0)
                                  {
                                      // Limit length to prevent buffer overflow
                                      if (fileNameLen > 20)
                                      {
                                          fileNameLen = 20;
                                      }

                                      SaveFileRec* pRec = &gSaveFileRecords[gTotalSaveFilesCount];
                                      memcpy(pRec->mFileName, fileName, fileNameLen);
                                      pRec->mFileName[fileNameLen] = 0;

                                      pRec->mLastWriteTimeStamp = lastWriteTime;
                                      gTotalSaveFilesCount++;
                                  }
                              }
                          });

    // Sort all we've found by time stamp, users probably want to load their last save first
    qsort(gSaveFileRecords, gTotalSaveFilesCount, sizeof(SaveFileRec), Sort_comparitor_4D42C0);

    // Underflow
    if (gSavedGameToLoadIdx < 0)
    {
        gSavedGameToLoadIdx = 0;
    }

    // Overflow
    if (gSavedGameToLoadIdx >= gTotalSaveFilesCount)
    {
        gSavedGameToLoadIdx = gTotalSaveFilesCount - 1;
    }
}
