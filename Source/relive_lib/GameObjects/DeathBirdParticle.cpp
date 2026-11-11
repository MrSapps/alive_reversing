#include "stdafx.h"
#include "DeathBirdParticle.hpp"
#include "../Function.hpp"
#include "Dove.hpp"
#include "Sfx.hpp"
#include "../../AliveLibAE/Math.hpp"
#include "../FixedPoint.hpp"
#include "../SerializedObjectData.hpp"

void DeathBirdParticle::LoadAnimations()
{
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::DeathFlare_2));
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::DeathFlare_1));
}

DeathBirdParticle::DeathBirdParticle(FP xpos, FP ypos, s32 startTimer, bool playSound, FP scale, ResourceManager& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map),
    mStartTimer(startTimer),
    mPlaySound(playSound)
{
    SetType(ReliveTypes::eDeathBird);

    LoadAnimations();

    Animation_Init(GetAnimRes(AnimId::DeathFlare_1));

    SetApplyShadowZoneColour(false);
    GetAnimation().SetBlendMode(relive::TBlendModes::eBlend_1);
    SetSpriteScale(scale);
    GetAnimation().SetSpriteScale(scale);

    if (scale <= FP_FromDouble(0.5))
    {
        GetAnimation().SetRenderLayer(Layer::eLayer_Foreground_Half_17);
    }
    else
    {
        GetAnimation().SetRenderLayer(Layer::eLayer_Above_FG1_39);
    }

    mXPos = xpos;
    mYPos = ypos;
    mRandom = Math_NextRandom();
}

void DeathBirdParticle::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    DeathBirdParticleSaveState data = {};
    data.mStartTimer = mStartTimer;
    data.mPlaySound = mPlaySound;
    data.mScale = GetSpriteScale();
    data.mObject = GetAnimatedState();
    data.mRandom = mRandom;
    data.mState = static_cast<s8>(mState);
    pSaveBuffer.Write(data);
}

void DeathBirdParticle::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const auto pState = pData.ReadTmpPtr<DeathBirdParticleSaveState>();
    auto pParticle = relive_new DeathBirdParticle(FP_FromInteger(0), FP_FromInteger(0), pState->mStartTimer, pState->mPlaySound, pState->mScale, resMan, map);
    pParticle->SetAnimatedState(pState->mObject);
    pParticle->mRandom = pState->mRandom;
    pParticle->mState = static_cast<States>(pState->mState);
}

void DeathBirdParticle::VUpdate()
{
    switch (mState)
    {
        case States::eAnimateDeathFlares_0:
            if (static_cast<s32>(sGnFrame) > mStartTimer)
            {
                // Death "star"
                GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::DeathFlare_2));
                mState = States::eTransformStarsToDoves_1;
            }
            break;

        case States::eTransformStarsToDoves_1:
            // Has the Death "star" finished animating?
            if (GetAnimation().GetIsLastFrame())
            {
                // Yes so magic it into a dove
                auto pDove = relive_new Dove(
                    AnimId::Dove_Flying,
                    mXPos,
                    mYPos - FP_FromInteger(15),
                    GetSpriteScale(),
                    mResMan, mMap);

                if (pDove->GetAnimation().GetFlipX())
                {
                    pDove->mXPos += FP_FromInteger(8);
                }
                else
                {
                    pDove->mXPos -= FP_FromInteger(8);
                }

                SetDead(true);
                pDove->SetSpriteScale(GetSpriteScale());

                if (mPlaySound)
                {
                    SfxPlayMono(relive::SoundEffects::AbeDove, 0);
                }
            }
            break;
    }

    mXPos += FP_FromInteger(2) * Math_Sine(mRandom);
    mYPos -= FP_FromInteger(2);
    mRandom += 5;
}
