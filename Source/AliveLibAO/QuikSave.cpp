#include "stdafx_ao.h"
#include "QuikSave.hpp"
#include "Abe.hpp"
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
#include "HoistRocksEffect.hpp"
#include "LiftPoint.hpp"
#include "RollingBall.hpp"
#include "MovingBomb.hpp"
#include "FallingItem.hpp"
#include "MeatSack.hpp"
#include "RockSack.hpp"
#include "InvisibleSwitch.hpp"
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
    pInfo.mAbeInvincible = gAbeInvulnerableCheat != 0;
    pInfo.mControlledCharX = static_cast<s16>(FP_GetExponent(sControlledCharacter->mXPos));
    pInfo.mControlledCharY = rect.h;
    pInfo.mControlledCharAtFullScale = sControlledCharacter->GetSpriteScale() == FP_FromInteger(1);
}

void QuikSave::RestoreWorldInfo(const QuicksaveWorldInfoBase& pInfo)
{
    sGnFrame = static_cast<u32>(pInfo.mGnFrame);
    gRescuedMudokons = pInfo.mRescuedMudokons;
    gKilledMudokons = pInfo.mKilledMudokons;
    gDeathGasTimer = pInfo.mGasTimer;
    gAbeInvulnerableCheat = pInfo.mAbeInvincible ? 1 : 0;
}

void QuikSave::DoQuicksave(BaseMap& map)
{
    map.GetResourceManager().ShowLoadingIcon(map);

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
                pObj->VGetSaveState(gActiveQuicksaveData.mObjectsStateData);
            }
        }

        map.SaveQuicksaveBlyData(gActiveQuicksaveData.mObjectBlyData);
    }
}

void QuikSave::LoadActive(BaseMap& map)
{
    map.GetResourceManager().ShowLoadingIcon(map);

    DestroyObjects(map.GetResourceManager());
    EventsReset();
    gSkipGameObjectUpdates = true;

    RestoreWorldInfo(gActiveQuicksaveData.mWorldInfo);
    gSwitchStates = gActiveQuicksaveData.mSwitchStates;

    map.mPendingSaveRestore = &gActiveQuicksaveData;
    map.SetActiveCam(
        gActiveQuicksaveData.mWorldInfo.mLevel,
        gActiveQuicksaveData.mWorldInfo.mPath,
        gActiveQuicksaveData.mWorldInfo.mCam,
        CameraSwapEffects::eInstantChange_0,
        0,
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
void QuikSave::RestoreBlyData(PendingObjectRestoreData& pSaveData, ResourceManagerWrapper& resMan, BaseMap& map)
{
    pSaveData.mObjectsStateData.ReadRewind();
    while (pSaveData.mObjectsStateData.CanRead())
    {
        const SaveStateBase* pSaveStateBase = pSaveData.mObjectsStateData.PeekTmpPtr<SaveStateBase>();
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

            case ReliveTypes::eTimedMine:
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
                InvisibleSwitch::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
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

            case ReliveTypes::eHoist:
                HoistRocksEffect::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eHoistParticle:
                HoistParticle::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
                break;

            case ReliveTypes::eLiftPoint:
                LiftPoint::CreateFromSaveState(pSaveData.mObjectsStateData, resMan, map);
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

            default:
                // Shouldn't happen given the comment above, but don't
                // corrupt the rest of the stream if it ever does.
                pSaveData.mObjectsStateData.SkipRead(pSaveStateBase->mSize);
                break;
        }
    }

    map.RestoreQuicksaveBlyData(pSaveData.mObjectBlyData);
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
            pObj->VGetSaveState(gActiveQuicksaveData.mRestartPathObjectData.mObjectsStateData);
        }
    }

    map.SaveQuicksaveBlyData(gActiveQuicksaveData.mRestartPathObjectData.mObjectBlyData);
}

void QuikSave::RestoreCheckpoint(ResourceManagerWrapper& resMan, BaseMap& map)
{
    // Destroy everything except objects that opt out (Abe included - see
    // Abe's constructor) so the deferred restore below recreates the rest
    // fresh from the checkpoint snapshot instead of leaving stale/dead
    // instances (e.g. an already-exploded bomb) sitting around forever.
    DestroyObjects(resMan);

    gSwitchStates = gActiveQuicksaveData.mRestartPathSwitchStates;
    Abe::CreateFromSaveState(gActiveQuicksaveData.mRestartPathAbeState, resMan, map);
    RestoreWorldInfo(gActiveQuicksaveData.mRestartPathWorldInfo);

    map.mPendingSaveRestore = &gActiveQuicksaveData.mRestartPathObjectData;
    map.SetActiveCam(
        gActiveQuicksaveData.mRestartPathWorldInfo.mLevel,
        gActiveQuicksaveData.mRestartPathWorldInfo.mPath,
        gActiveQuicksaveData.mRestartPathWorldInfo.mCam,
        CameraSwapEffects::eInstantChange_0,
        0,
        1);
    map.mForceLoad = 1;
}

} // namespace AO
