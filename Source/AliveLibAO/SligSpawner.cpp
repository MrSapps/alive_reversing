#include "stdafx_ao.h"
#include "../relive_lib/Function.hpp"
#include "SligSpawner.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Slig.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../relive_lib/Events.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Path.hpp"
#include "Map.hpp"

namespace AO {

SligSpawner::SligSpawner(relive::Path_TLV* pTlv, relive::Path_Slig* pTlvData, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eSligSpawner);

    mTlvInfo = tlvId;
    mPathTlv = *pTlv;

    mSpawnerFlags = 1;

    mSligSpawnerSwitchId = pTlvData->mSligSpawnerSwitchId;
}

void SligSpawner::VScreenChanged()
{
    SetDead(true);
}

void SligSpawner::VUpdate()
{
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (SwitchStates_Get(mSligSpawnerSwitchId))
    {
        auto pTlv = mMap.VTLV_Get_At_Of_Type(
            mPathTlv.mTopLeftX,
            mPathTlv.mTopLeftY,
            mPathTlv.mTopLeftX,
            mPathTlv.mTopLeftY,
            ReliveTypes::eSligSpawner).GetTlv<relive::Path_Slig>();

        if (pTlv)
        {
            relive_new Slig(pTlv, mTlvInfo, mResMan, mMap);
        }

        SetDead(true);
        mSpawnerFlags = 0;
    }
}

SligSpawner::~SligSpawner()
{
    if (mSpawnerFlags)
    {
        mMap.TLV_Reset(mTlvInfo);
    }
    else
    {
        mMap.TLV_Delete(mTlvInfo);
    }
}

void SligSpawner::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    SligSpawnerSaveState data = {};

    data.mTlvId = mTlvInfo;
    data.mSligSpawnerSwitchId = mSligSpawnerSwitchId;

    pSaveBuffer.Write(data);
}

void SligSpawner::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<SligSpawnerSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    if (!tlvIterator.GetTlv() || tlvIterator.GetTlv()->mTlvType != ReliveTypes::eSligSpawner)
    {
        // The saved tlv-info didn't resolve back to a TLV of the expected
        // type (can happen if two TLVs ended up sharing the same id) -
        // nothing safe to do here, so drop this record.
        return;
    }
    auto pTlv = tlvIterator.GetTlv<relive::Path_Slig>();

    auto pSpawner = relive_new SligSpawner(pTlv, pTlv, pState->mTlvId, resMan, map);
    if (pSpawner)
    {
        pSpawner->mSligSpawnerSwitchId = pState->mSligSpawnerSwitchId;
    }
}

} // namespace AO
