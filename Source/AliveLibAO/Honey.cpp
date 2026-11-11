#include "stdafx_ao.h"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Honey.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Path.hpp"
#include "Map.hpp"

namespace AO {

void Honey::VUpdate()
{
    // Empty
}

Honey::Honey(FP xpos, FP ypos, ResourceManager& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map)
{
    SetType(ReliveTypes::eHoney);

    mRGB.SetRGB(128, 128, 128);

    mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::Honey));
    Animation_Init(GetAnimRes(AnimId::Honey));

    mYPos = ypos;
    mXPos = xpos;
}

void Honey::VScreenChanged()
{
    SetDead(true);
    mMap.TLV_Reset(mTlvInfo);
}

void Honey::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    HoneySaveState data = {};
    data.mTlvInfo = mTlvInfo;
    pSaveBuffer.Write(data);
}

void Honey::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const HoneySaveState& state = *pData.ReadTmpPtr<HoneySaveState>();
    Honey* p = relive_new Honey(FP_FromInteger(0), FP_FromInteger(0), resMan, map);
    if (!p)
    {
        return;
    }
    p->mTlvInfo = state.mTlvInfo;
}

} // namespace AO
