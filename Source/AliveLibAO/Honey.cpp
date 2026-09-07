#include "stdafx_ao.h"
#include "Honey.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Path.hpp"

namespace AO {

void Honey::VUpdate()
{
    // Empty
}

Honey::Honey(FP xpos, FP ypos, ResourceManagerWrapper& resMan)
    : BaseAnimatedWithPhysicsGameObject(0, resMan)
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
    Path::TLV_Reset(mTlvInfo);
}

} // namespace AO
