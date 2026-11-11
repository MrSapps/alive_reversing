#include "stdafx_ao.h"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "HoneyDrip.hpp"
#include "Sfx.hpp"
#include "../AliveLibAE/stdlib.hpp"

namespace AO {

HoneyDrip::HoneyDrip(FP xpos, FP ypos, ResourceManager& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map)
{
    mRGB.SetRGB(128, 128, 128);

    mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::Honey_Drip));
    Animation_Init(GetAnimRes(AnimId::Honey_Drip));
    mYPos = ypos;
    mXPos = xpos;

    GetAnimation().SetRenderLayer(Layer::eLayer_BeforeWell_22);

    mSoundPlaying = false;
}

void HoneyDrip::VUpdate()
{
    mXPos += mVelX;
    mYPos += mVelY;

    if (GetAnimation().GetCurrentFrame() == 7 && !mSoundPlaying)
    {
        SfxPlayMono(relive::SoundEffects::HoneyDrip, 0);
        mSoundPlaying = true;
    }

    if (GetAnimation().GetIsLastFrame())
    {
        SetDead(true);
    }
}

void HoneyDrip::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    HoneyDripSaveState data = {};
    data.mSoundPlaying = mSoundPlaying;
    pSaveBuffer.Write(data);
}

void HoneyDrip::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const HoneyDripSaveState& state = *pData.ReadTmpPtr<HoneyDripSaveState>();
    HoneyDrip* p = relive_new HoneyDrip(FP_FromInteger(0), FP_FromInteger(0), resMan, map);
    if (!p)
    {
        return;
    }
    p->mSoundPlaying = state.mSoundPlaying;
}

} // namespace AO
