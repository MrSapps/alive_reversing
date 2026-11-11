#include "stdafx_ao.h"
#include "QuikSave.hpp"
#include <optional>
#include "Abe.hpp"
#include "DoorFlame.hpp"
#include "HintFly.hpp"
#include "Bullet.hpp"
#include "BellSong.hpp"
#include "Bells.hpp"
#include "FlintLockFire.hpp"
#include "LightEffect.hpp"
#include "HoneySack.hpp"
#include "HoneyDrip.hpp"
#include "Honey.hpp"
#include "Meat.hpp"
#include "Rock.hpp"
#include "MusicTrigger.hpp"
#include "PullRingRope.hpp"
#include "SlogSpawner.hpp"
#include "MotionDetector.hpp"
#include "SecurityDoor.hpp"
#include "FootSwitch.hpp"
#include "Lever.hpp"
#include "GameSpeak.hpp"
#include "Slig.hpp"
#include "SligSpawner.hpp"
#include "Scrab.hpp"
#include "Paramite.hpp"
#include "Slog.hpp"
#include "Mudokon.hpp"
#include "SlingMudokon.hpp"
#include "Elum.hpp"
#include "UXB.hpp"
#include "LCDScreen.hpp"
#include "DoorLight.hpp"
#include "MeatSaw.hpp"
#include "Bat.hpp"
#include "BeeNest.hpp"
#include "BeeSwarmHole.hpp"
#include "BeeSwarm.hpp"
#include "BellHammer.hpp"
#include "ZBall.hpp"
#include "HoistRocksEffect.hpp"
#include "LiftPoint.hpp"
#include "LiftMover.hpp"
#include "BirdPortal.hpp"
#include "RollingBall.hpp"
#include "MovingBomb.hpp"
#include "FallingItem.hpp"
#include "MeatSack.hpp"
#include "RockSack.hpp"
#include "../relive_lib/GameObjects/InvisibleSwitch.hpp"
#include "SecurityClaw.hpp"
#include "SecurityOrb.hpp"
#include "ChimeLock.hpp"
#include "BackgroundGlukkon.hpp"
#include "Shrykull.hpp"
#include "DDCheat.hpp"
#include "../relive_lib/BaseMap.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "../relive_lib/Engine.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/GameObjects/Grenade.hpp"
#include "../relive_lib/GameObjects/TrapDoor.hpp"
#include "../relive_lib/GameObjects/TimerTrigger.hpp"
#include "../relive_lib/GameObjects/AbilityRing.hpp"
#include "../relive_lib/GameObjects/ThrowableArray.hpp"
#include "../relive_lib/WorldStatic.hpp"
#include "../relive_lib/GameObjects/BoomMachinePipe.hpp"
#include "../relive_lib/GameObjects/RollingBallShaker.hpp"
#include "../relive_lib/GameObjects/ParamiteWeb.hpp"
#include "../relive_lib/GameObjects/CircularFade.hpp"
#include "../relive_lib/GameObjects/ZapLine.hpp"
#include "../relive_lib/GameObjects/ScreenClipper.hpp"
#include "../relive_lib/GameObjects/BirdPortalTerminator.hpp"
#include "../relive_lib/GameObjects/Mine.hpp"
#include "../relive_lib/GameObjects/TimedMine.hpp"
#include "../relive_lib/GameObjects/ElectricWall.hpp"
#include "../relive_lib/GameObjects/BoomMachine.hpp"
#include "../relive_lib/ListOrderSaveState.hpp"
#include "../relive_lib/GameObjects/Alarm.hpp"
#include "../relive_lib/GameObjects/ZzzSpawner.hpp"
#include "../relive_lib/GameObjects/RollingBallStopper.hpp"
#include "../relive_lib/GameObjects/BackgroundAnimation.hpp"
#include "../relive_lib/GameObjects/Door.hpp"
#include "../relive_lib/GameObjects/Spark.hpp"
#include "../relive_lib/GameObjects/Particle.hpp"
#include "../relive_lib/GameObjects/Gibs.hpp"
#include "../relive_lib/GameObjects/ParticleBurst.hpp"
#include "../relive_lib/GameObjects/Blood.hpp"
#include "../relive_lib/GameObjects/DeathBirdParticle.hpp"
#include "../relive_lib/GameObjects/ZapSpark.hpp"
#include "../relive_lib/GameObjects/Flash.hpp"
#include "../relive_lib/GameObjects/Fade.hpp"
#include "../relive_lib/GameObjects/ScreenShake.hpp"
#include "../relive_lib/GameObjects/AirExplosion.hpp"
#include "../relive_lib/GameObjects/Dove.hpp"
#include "../relive_lib/GameObjects/Leaf.hpp"
#include "../relive_lib/GameObjects/SnoozeParticle.hpp"
#include "../relive_lib/GameObjects/GasEmitter.hpp"
#include "../relive_lib/GameObjects/OrbWhirlWind.hpp"
#include "../relive_lib/GameObjects/ThrowableTotalIndicator.hpp"
#include "../relive_lib/GameObjects/DeathGas.hpp"
#include "../relive_lib/GameObjects/PossessionFlicker.hpp"
#include "../relive_lib/GameObjects/GroundExplosion.hpp"
#include "../relive_lib/GameObjects/Electrocute.hpp"
#include "../relive_lib/GameObjects/PaletteOverwriter.hpp"
#include "../relive_lib/GameObjects/BulletShell.hpp"
#include "../AliveLibAE/GameSpeak.hpp"
#include <functional>
#include <vector>

extern u8 sRandomSeed; // Math.cpp

namespace AO {

Quicksave QuikSave::gActiveQuicksaveData;

void QuikSave::SaveWorldInfo(QuicksaveWorldInfoBase& pInfo, BaseMap& map)
{
    const PSX_RECT rect = sControlledCharacter->VGetBoundingRect();

    pInfo.mGnFrame = static_cast<s32>(sGnFrame);
    pInfo.mLevel = map.mCurrentLevel;
    pInfo.mPath = map.mCurrentPath;
    pInfo.mCam = map.mCurrentCamera;
    pInfo.mRescuedMudokons = gRescuedMudokons;
    pInfo.mKilledMudokons = gKilledMudokons;
    pInfo.mGasTimer = gDeathGasTimer;
    pInfo.mRandomSeed = sRandomSeed;
    pInfo.mLeafRandIdx = Leaf::GetRandIdx();
    pInfo.mGuidCounter = Guid::GetNextGuidCounter();
    pInfo.mAbeInvincible = gAbeInvulnerableCheat != 0;
    pInfo.mControlledCharX = static_cast<s16>(FP_GetExponent(sControlledCharacter->mXPos));
    pInfo.mControlledCharY = rect.h;
    pInfo.mControlledCharAtFullScale = sControlledCharacter->GetSpriteScale() == FP_FromInteger(1);
}

void QuikSave::RestoreWorldInfo(const QuicksaveWorldInfoBase& pInfo)
{
    sGnFrame = static_cast<u32>(pInfo.mGnFrame);
    sRandomSeed = pInfo.mRandomSeed;
    Leaf::SetRandIdx(pInfo.mLeafRandIdx);
    BaseGameObject::RestoreGuidCounter(pInfo.mGuidCounter);
    gRescuedMudokons = pInfo.mRescuedMudokons;
    gKilledMudokons = pInfo.mKilledMudokons;
    gDeathGasTimer = pInfo.mGasTimer;
    gAbeInvulnerableCheat = pInfo.mAbeInvincible ? 1 : 0;
}

void QuikSave::DoQuicksave(BaseMap& map)
{
    map.GetResourceManager().RequestLoadingWait(LoadingIcon::eNow);

    if (gAbe && gAbe->mHealth > FP_FromInteger(0))
    {
        SaveWorldInfo(gActiveQuicksaveData.mWorldInfo, map);
        gActiveQuicksaveData.mSwitchStates = gSwitchStates;

        gActiveQuicksaveData.mObjectsStateData.WriteRewind();
        for (s32 idx = 0; idx < gBaseGameObjects->Size(); idx++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(idx);
            if (!pObj)
            {
                break;
            }

            if (!pObj->GetDead())
            {
                pObj->SaveState(gActiveQuicksaveData.mObjectsStateData);
            }
        }

        gActiveQuicksaveData.mObjectsStateData.Write(WorldStatics::GetSaveState());
        BaseGameObject::SaveListOrders(gActiveQuicksaveData.mObjectsStateData);
        gActiveQuicksaveData.mObjectsStateData.Write(EventsGetSaveState());

        map.SaveQuicksaveBlyData(gActiveQuicksaveData.mObjectBlyData);
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

void QuikSave::LoadActive(BaseMap& map)
{
    map.GetResourceManager().RequestLoadingWait(LoadingIcon::eNow);

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

    RestoreWorldInfo(gActiveQuicksaveData.mWorldInfo);
    RestoreWorldStatics(gActiveQuicksaveData.mObjectsStateData);
    gSwitchStates = gActiveQuicksaveData.mSwitchStates;

    map.mPendingSaveRestore = &gActiveQuicksaveData;
    map.SetActiveCam(
        gActiveQuicksaveData.mWorldInfo.mLevel,
        gActiveQuicksaveData.mWorldInfo.mPath,
        gActiveQuicksaveData.mWorldInfo.mCam,
        CameraSwapEffects::eInstantChange_0,{},
        1);
    map.mForceLoad = 1;
}

// Per-object restore dispatch. Every live object's VGetSaveState() gets
// called generically in DoQuicksave (base class default is a no-op), so
// the only types that actually appear in the stream are the ones that
// override it: Abe, Slig, Scrab, plus the relive_lib GameObjects that
// already implement CreateFromSaveState identically for both engines
// (Grenade, TrapDoor, TimerTrigger, AbilityRing, ThrowableArray). Anything
// else - AO's other enemy/NPC types - doesn't implement VGetSaveState yet,
// so it never writes a record here and there's nothing to restore for it;
// it just comes back however its own VUpdate() naturally spawns it from
// its TLV.
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

void QuikSave::RestoreBlyData(PendingObjectRestoreData& pSaveData, ResourceManager& resMan, BaseMap& map)
{
    pSaveData.mObjectsStateData.ReadRewind();
    // The object an animation is for may be made by a later save, so these go on once all are made
    std::vector<AnimationStateSaveState> animStates;
    std::vector<AliveObjectStateSaveState> aliveStates;
    std::vector<GameSpeakListenerSaveState> listenerStates;
    std::vector<std::function<void()>> afterCamera;
    // Every object that was saved has an animation record, in the order the objects were in
    std::vector<Guid> objectOrder;
    std::vector<Guid> drawablesOrder;
    std::vector<Guid> aliveOrder;
    std::vector<std::pair<Guid, s32>> animationsOrder;
    std::vector<Guid> platformsOrder;
    while (pSaveData.mObjectsStateData.CanRead())
    {
        const SaveStateBase* pSaveStateBase = pSaveData.mObjectsStateData.PeekTmpPtr<SaveStateBase>();
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
        if (pSaveStateBase->mType == ReliveTypes::eAnimationState)
        {
            objectOrder.push_back(pSaveStateBase->mObjectId);
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
        if (pSaveStateBase->mType == ReliveTypes::eGameSpeak)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const GameSpeakSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<GameSpeakSaveState>();
            afterCamera.push_back([state]() { GameSpeak::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eLever)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const LeverSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<LeverSaveState>();
            afterCamera.push_back([state]() { Lever::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eFootSwitch)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const FootSwitchSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<FootSwitchSaveState>();
            afterCamera.push_back([state]() { FootSwitch::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eSecurityDoor)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SecurityDoorSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<SecurityDoorSaveState>();
            afterCamera.push_back([state]() { SecurityDoor::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eMotionDetector)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MotionDetectorSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<MotionDetectorSaveState>();
            afterCamera.push_back([state]() { MotionDetector::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eSlogSpawner)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const SlogSpawnerSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<SlogSpawnerSaveState>();
            afterCamera.push_back([state]() { SlogSpawner::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::ePullRingRope)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const PullRingRopeSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<PullRingRopeSaveState>();
            afterCamera.push_back([state]() { PullRingRope::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eMine)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const MineSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<MineSaveState>();
            afterCamera.push_back([state]() { ::Mine::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eTimedMine)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const TimedMineSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<TimedMineSaveState>();
            afterCamera.push_back([state]() { ::TimedMine::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eElectricWall)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ElectricWallSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<ElectricWallSaveState>();
            afterCamera.push_back([state]() { ::ElectricWall::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eAlarm)
        {
            const AlarmSaveState state = *pSaveData.mObjectsStateData.PeekTmpPtr<AlarmSaveState>();
            if (state.mIsTlvAlarm)
            {
                pSaveData.mObjectsStateData.SkipRead(sizeof(AlarmSaveState));
                // Made by the camera (from its TLV), so it is put back once that has
                afterCamera.push_back([state]() { ::Alarm::RestoreFromSaveState(state); });
                continue;
            }
        }
        if (pSaveStateBase->mType == ReliveTypes::eBoomMachine)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BoomMachineSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<BoomMachineSaveState>();
            afterCamera.push_back([state]() { ::BoomMachine::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eZzzSpawner)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const ZzzSpawnerSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<ZzzSpawnerSaveState>();
            afterCamera.push_back([state]() { ::ZzzSpawner::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eRollingBallStopper)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const RollingBallStopperSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<RollingBallStopperSaveState>();
            afterCamera.push_back([state]() { ::RollingBallStopper::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eBackgroundAnimation)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BackgroundAnimationSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<BackgroundAnimationSaveState>();
            afterCamera.push_back([state]() { ::BackgroundAnimation::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eGasCountDown)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const GasCountDownSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<GasCountDownSaveState>();
            afterCamera.push_back([state]() { ::GasCountDown::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eDoor)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const DoorSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<DoorSaveState>();
            afterCamera.push_back([state]() { ::Door::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eMusicTrigger && pSaveData.mObjectsStateData.PeekTmpPtr<MusicTriggerSaveState>()->mTlvId.IsValid())
        {
            // Made by the camera (from its TLV), so it is put back once that has. One made by a script is made again below
            const MusicTriggerSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<MusicTriggerSaveState>();
            afterCamera.push_back([state]() { MusicTrigger::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eBirdPortal && pSaveData.mObjectsStateData.PeekTmpPtr<BirdPortalSaveState>()->mBase.mFullState)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const BirdPortalSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<BirdPortalSaveState>();
            afterCamera.push_back([state]() { BirdPortal::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eHoneySack)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const HoneySackSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<HoneySackSaveState>();
            afterCamera.push_back([state]() { HoneySack::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eLightEffect)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const LightEffectSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<LightEffectSaveState>();
            afterCamera.push_back([state]() { LightEffect::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eFlintLockFire)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const FlintLockFireSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<FlintLockFireSaveState>();
            afterCamera.push_back([state]() { FlintLockFire::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eDoorFlame)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const DoorFlameSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<DoorFlameSaveState>();
            afterCamera.push_back([state]() { DoorFlame::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eHintFly)
        {
            // Made by the camera (from its TLV), so it is put back once that has
            const HintFlySaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<HintFlySaveState>();
            afterCamera.push_back([state]() { HintFly::RestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eEventsState)
        {
            // Names objects the camera may make
            const EventsSaveState state = *pSaveData.mObjectsStateData.ReadTmpPtr<EventsSaveState>();
            afterCamera.push_back([state]() { EventsRestoreFromSaveState(state); });
            continue;
        }
        if (pSaveStateBase->mType == ReliveTypes::eWorldStatics)
        {
            // Put back when the save is loaded, before any object is made, see RestoreWorldStatics
            pSaveData.mObjectsStateData.SkipRead(sizeof(WorldStaticsSaveState));
            continue;
        }
        // The object made takes the id it was saved with, and what its constructor makes the ids that come from it.
        // Not Abe, who is restored in place and keeps his id
        std::optional<Guid::CreationScope> idScope;
        if (pSaveStateBase->mObjectId.IsValid() && pSaveStateBase->mType != ReliveTypes::eAbe)
        {
            idScope.emplace(pSaveStateBase->mObjectId);
        }
        switch (pSaveStateBase->mType)
        {
            case ReliveTypes::eAbe:
                Abe::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSlig:
                Slig::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSligSpawner:
                SligSpawner::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eScrab:
                Scrab::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eParamite:
                Paramite::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSlog:
                Slog::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMudokon:
                Mudokon::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::SlingMud:
                SlingMudokon::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eElum:
                Elum::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eUXB:
                UXB::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eRollingBall:
                RollingBall::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMovingBomb:
                MovingBomb::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eRockSpawner:
                FallingItem::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMeatSack:
                MeatSack::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eRockSack:
                RockSack::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eInvisibleSwitch:
                ::InvisibleSwitch::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSecurityClaw:
                SecurityClaw::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSecurityOrb:
                SecurityOrb::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eChimeLock:
                ChimeLock::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBackgroundGlukkon:
                BackgroundGlukkon::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eShrykull:
                Shrykull::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eLCDScreen:
                LCDScreen::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eDoorLight:
                DoorLight::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMeatSaw:
                MeatSaw::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eHoistParticle:
                HoistParticle::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eLiftPoint:
                LiftPoint::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eLiftMover:
                LiftMover::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBat:
                Bat::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBeeNest:
                BeeNest::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBeeSwarm:
                BeeSwarm::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBeeSwarmHole:
                BeeSwarmHole::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBellHammer:
                BellHammer::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eZBall:
                ZBall::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBirdPortal:
                BirdPortal::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eGrenade:
                Grenade::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eTrapDoor:
                TrapDoor::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eTimerTrigger:
                TimerTrigger::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eAbilityRing:
                AbilityRing::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eThrowableArray:
                ThrowableArray::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBulletShell:
                ::BulletShell::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::ePalOverwriter:
                ::PalleteOverwriter::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eElectrocute:
                ::Electrocute::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eGroundExplosion:
                ::GroundExplosion::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::ePossessionFlicker:
                ::PossessionFlicker::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eDeathGas:
                ::DeathGas::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eThrowableTotalIndicator:
                ::ThrowableTotalIndicator::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eOrbWhirlWind:
                ::OrbWhirlWind::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSpark:
                ::Spark::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eParticle:
                ::Particle::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eGibs:
                ::Gibs::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eParticleBurst:
                ::ParticleBurst::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBlood:
                ::Blood::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eDeathBird:
                ::DeathBirdParticle::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eZapSpark:
                ::ZapSpark::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eFlash:
                ::Flash::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eFade:
                ::Fade::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eScreenShake:
                ::ScreenShake::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eAirExplosion:
                ::AirExplosion::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eDove:
                ::Dove::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eLeaf:
                ::Leaf::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eSnoozeParticle:
                ::SnoozeParticle::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eGasEmitter:
                ::GasEmitter::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMusicTrigger:
                MusicTrigger::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBirdPortalTerminator:
                ::BirdPortalTerminator::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eScreenClipper:
                ::ScreenClipper::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eZapLine:
                ::ZapLine::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eCircularFade:
                ::CircularFade::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eParamiteWeb:
                ::ParamiteWeb::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eRollingBallShaker:
                ::RollingBallShaker::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eAlarm:
                ::Alarm::CreateFromSaveState(*pSaveData.mObjectsStateData.ReadTmpPtr<AlarmSaveState>(), resMan, map);
                break;

            case ReliveTypes::eBoomMachinePipe:
                ::BoomMachinePipe::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eRock:
                Rock::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eMeat:
                Meat::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eHoney:
                Honey::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eHoneyDrip:
                HoneyDrip::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBells:
                Bells::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBellSong:
                BellSong::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eBullet:
                Bullet::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eClaw:
                Claw::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            default:
                // Shouldn't happen given the comment above, but don't
                // corrupt the rest of the stream if it ever does.
                pSaveData.mObjectsStateData.SkipRead(pSaveStateBase->mSize);
                break;
        }
        BaseGameObject::ApplySavedHeader(*pSaveStateBase);
    }

    // The camera's own objects are made after these, so they get theirs then
    map.mAfterCameraObjects.clear();
    for (const std::function<void()>& after : afterCamera)
    {
        map.mAfterCameraObjects.push_back(after);
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
    map.mAfterCameraObjects.push_back([switchStates = gActiveQuicksaveData.mSwitchStates]()
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
                                          RestoreWorldStatics(gActiveQuicksaveData.mObjectsStateData);
                                          WorldStatics::LogChangedSince(before, "restore");
                                      });

    map.RestoreQuicksaveBlyData(pSaveData.mObjectBlyData);
    BaseGameObject::PostRestoreAll();
}

void QuikSave::SaveCheckpoint(BaseMap& map)
{
    SaveWorldInfo(gActiveQuicksaveData.mRestartPathWorldInfo, map);
    gAbe->GetSaveState(gActiveQuicksaveData.mRestartPathAbeState);
    gActiveQuicksaveData.mRestartPathSwitchStates = gSwitchStates;

    // Same generic per-object snapshot DoQuicksave uses, minus Abe (who is
    // captured above via mRestartPathAbeState instead - see RestoreCheckpoint).
    gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData.WriteRewind();
    for (s32 idx = 0; idx < gBaseGameObjects->Size(); idx++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(idx);
        if (!pObj)
        {
            break;
        }

        if (!pObj->GetDead() && pObj != gAbe)
        {
            pObj->SaveState(gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData);
        }
    }
    gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData.Write(WorldStatics::GetSaveState());
    BaseGameObject::SaveListOrders(gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData);
    gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData.Write(EventsGetSaveState());

    map.SaveQuicksaveBlyData(gActiveQuicksaveData.mRestartPathObjectData.mObjectBlyData);
}

void QuikSave::RestoreCheckpoint(ResourceManager& resMan, BaseMap& map)
{
    // Destroy everything except objects that opt out (Abe included - see
    // Abe's constructor) so the deferred restore below recreates the rest
    // fresh from the checkpoint snapshot instead of leaving stale/dead
    // instances (e.g. an already-exploded bomb) sitting around forever.
    DestroyObjects();

    gSwitchStates = gActiveQuicksaveData.mRestartPathSwitchStates;
    RestoreWorldStatics(gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData);
    Abe::CreateFromSaveState(gActiveQuicksaveData.mRestartPathAbeState, resMan, map);
    RestoreWorldInfo(gActiveQuicksaveData.mRestartPathWorldInfo);

    map.mPendingSaveRestore = &gActiveQuicksaveData.mRestartPathObjectData;
    map.SetActiveCam(
        gActiveQuicksaveData.mRestartPathWorldInfo.mLevel,
        gActiveQuicksaveData.mRestartPathWorldInfo.mPath,
        gActiveQuicksaveData.mRestartPathWorldInfo.mCam,
        CameraSwapEffects::eInstantChange_0,{},
        1);
    map.mForceLoad = 1;
}

} // namespace AO
