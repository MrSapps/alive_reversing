#include "stdafx.h"
#include "SligSpawner.hpp"
#include "../relive_lib/Function.hpp"
#include "Slig.hpp"
#include "stdlib.hpp"
#include "../relive_lib/Events.hpp"
#include "Sfx.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Map.hpp"
#include "Path.hpp"
#include "QuikSave.hpp"

SligSpawner::SligSpawner(relive::Path_Slig* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eSligSpawner);

    if (tlvId.IsValid())
    {
        mBaseGameObjectTlvInfo = tlvId;
    }

    mTlvInfo = tlvId;
    mPathTlv = *pTlv;

    mSligSpawnerSwitchId = pTlv->mSligSpawnerSwitchId;

    mUnlimitedSpawns = pTlv->mUnlimitedSpawns;

    mState = SpawnerStates::eInactive_0;
    mSpawnedSligId = Guid{};
}

SligSpawner::~SligSpawner()
{
    if (mDontDestroyTLV)
    {
        mMap.TLV_Reset(mTlvInfo);
    }
    else
    {
        mMap.TLV_Delete(mTlvInfo);
    }
}

std::vector<::BaseGameObject::NotStateMemory> SligSpawner::VNotStateMemory()
{
    // The TLV it was made from, with the flags that TLV had then
    return {{&mPathTlv, sizeof(mPathTlv)}};
}

void SligSpawner::VScreenChanged()
{
    if (mMap.LevelChanged() || mMap.PathChanged() || mState == SpawnerStates::eInactive_0)
    {
        SetDead(true);
    }
}

void SligSpawner::VUpdate()
{
    const auto pSpawnedSlig = static_cast<Slig*>(sObjectIds.Find_Impl(mSpawnedSligId));

    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (mState == SpawnerStates::eSligSpawned_1)
    {
        if (!pSpawnedSlig || pSpawnedSlig->GetDead() || pSpawnedSlig->mHealth <= FP_FromInteger(0))
        {
            SwitchStates_Set(mSligSpawnerSwitchId, 0);
            mState = SpawnerStates::eInactive_0;
        }
    }
    else if (mState == SpawnerStates::eInactive_0)
    {
        if (SwitchStates_Get(mSligSpawnerSwitchId))
        {
            TlvIterator sligSpawnerIterator = mMap.VTLV_Get_At_Of_Type(mPathTlv.mTopLeftX, mPathTlv.mTopLeftY, mPathTlv.mTopLeftX, mPathTlv.mTopLeftY, ReliveTypes::eSligSpawner);
            if (sligSpawnerIterator.GetTlv())
            {
                auto pSlig = relive_new Slig(sligSpawnerIterator.GetTlv<relive::Path_Slig>(), mTlvInfo, mResMan, mMap);
                if (pSlig)
                {
                    mSpawnedSligId = pSlig->mBaseGameObjectId;
                }

                mState = SpawnerStates::eSligSpawned_1;
                SfxPlayMono(relive::SoundEffects::SligSpawn, 0);
            }

            if (!mUnlimitedSpawns)
            {
                SetDead(true);
                mDontDestroyTLV = false;
            }
        }
    }
}

void SligSpawner::VPostRestore()
{
    // The save has the TLV the slig was made from, and the slig is known by its id
    if (mSpawnedSligId != Guid{})
    {
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            auto pObj = gBaseGameObjects->ItemAt(i);
            if (!pObj)
            {
                break;
            }

            if (pObj->Type() == ReliveTypes::eSlig && pObj->mBaseGameObjectTlvInfo == mSpawnedSligId)
            {
                mSpawnedSligId = pObj->mBaseGameObjectId;
                break;
            }
        }
    }
}

void SligSpawner::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    SligSpawnerSaveState data = {};
    data.mTlvId = mTlvInfo;
    data.mState = mState;
    data.mSpawnedSligId = mSpawnedSligId;
    if (mSpawnedSligId != Guid{})
    {
        BaseGameObject* pSpawnedSlig = sObjectIds.Find_Impl(mSpawnedSligId);
        if (pSpawnedSlig)
        {
            data.mSpawnedSligId = pSpawnedSlig->mBaseGameObjectTlvInfo;
        }
        // else: the slig is gone, and the id of it is kept as it is
    }
    pSaveBuffer.Write(data);
}

void SligSpawner::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManager& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<SligSpawnerSaveState>();
    auto pTlv = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId).GetTlv<relive::Path_Slig>();
    auto pSpawner = relive_new SligSpawner(pTlv, pState->mTlvId, resMan, map);
    if (pSpawner)
    {
        pSpawner->mState = pState->mState;
        pSpawner->mSpawnedSligId = pState->mSpawnedSligId;
    }
}
