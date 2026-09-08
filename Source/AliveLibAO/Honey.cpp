#include "stdafx_ao.h"
#include "Honey.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Path.hpp"
#include "Map.hpp"

namespace AO {

void Honey::VUpdate()
{
    // Empty
}

Honey::Honey(FP xpos, FP ypos, ResourceManagerWrapper& resMan, BaseMap& map)
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

} // namespace AO
