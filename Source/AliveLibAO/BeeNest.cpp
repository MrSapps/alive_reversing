#include "stdafx_ao.h"
#include "../relive_lib/Function.hpp"
#include "BeeNest.hpp"
#include "BeeSwarm.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Abe.hpp"
#include "Path.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Map.hpp"

namespace AO {

BeeNest::BeeNest(relive::Path_BeeNest* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eBeeNest);

    mTlvInfo = tlvId;

    mSwarmSize = pTlv->mSwarmSize;

    mSwitchId = pTlv->mSwitchId;

    mBeeSwarmX = FP_FromInteger(pTlv->mTopLeftX);
    mBeeSwarmY = FP_FromInteger(pTlv->mTopLeftY);

    mTotalChaseTime = pTlv->mChaseTime;
    mSpeed = FP_FromRaw(pTlv->mSpeed << 8);

    mState = BeeNestStates::eWaitForTrigger_0;

    // The "idle" swarm that hovers around the nest
    relive_new BeeSwarm(mBeeSwarmX, mBeeSwarmY, FP_FromInteger(0), pTlv->mBeesAmount, 0, resMan, map);
}

BeeNest::~BeeNest()
{

}

void BeeNest::VScreenChanged()
{
    if (mMap.LevelChanged() || mMap.PathChanged() || !mBeeSwarm.IsValid())
    {
        mMap.TLV_Reset(mTlvInfo);
        mBeeSwarm = Guid{};
        SetDead(true);
    }
}

void BeeNest::VUpdate()
{
    switch (mState)
    {
        case BeeNestStates::eWaitForTrigger_0:
            if (SwitchStates_Get(mSwitchId))
            {
                auto pBeeSwarm = relive_new BeeSwarm(
                    mBeeSwarmX,
                    mBeeSwarmY,
                    mSpeed,
                    mSwarmSize,
                    mTotalChaseTime,
                    mResMan, mMap);

                mBeeSwarm = pBeeSwarm->mBaseGameObjectId;
                if (pBeeSwarm)
                {
                    pBeeSwarm->Chase(gAbe);
                    mState = BeeNestStates::eResetIfDead_1;
                }
            }
            break;

        case BeeNestStates::eResetIfDead_1:
        {
            auto pBeeSwarm = sObjectIds.Find(mBeeSwarm, ReliveTypes::eBeeSwarm);
            if (!pBeeSwarm || pBeeSwarm->GetDead())
            {
                mState = BeeNestStates::eWaitForTrigger_0;
                mBeeSwarm = Guid{};
                SwitchStates_Set(mSwitchId, 0);
            }
            break;
        }

        default:
            break;
    }
}

void BeeNest::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    BeeNestSaveState data = {};

    data.mTlvInfo = mTlvInfo;
    data.mState = mState;

    pSaveBuffer.Write(data);
}

void BeeNest::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<BeeNestSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvInfo);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_BeeNest>(ReliveTypes::eBeeNest);

    auto pNest = relive_new BeeNest(pTlv, pState->mTlvInfo, resMan, map);
    if (pNest)
    {
        if (pState->mState == BeeNestStates::eResetIfDead_1)
        {
            // The chase swarm that was active can't be restored (see
            // BeeNestSaveState comment) - mirror the same cleanup
            // eResetIfDead_1 itself does when a swarm finishes, so the
            // trigger switch doesn't stay stuck "on" with nothing chasing.
            SwitchStates_Set(pNest->mSwitchId, 0);
        }
        else
        {
            pNest->mState = pState->mState;
        }
    }
}

} // namespace AO
