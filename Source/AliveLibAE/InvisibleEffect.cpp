#include "stdafx.h"
#include "InvisibleEffect.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/GameObjects/PossessionFlicker.hpp"
#include "../relive_lib/Clamp.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "stdlib.hpp"
#include "../relive_lib/data_conversion/AnimationConverter.hpp"

// The original faded the colours of its palette (5 bits per channel) one bit a step, so the fade took
// at most this many steps
static constexpr u8 kMaxFadeSteps = 31;

// What a step of the fade takes from every channel: one bit of a 5 bit channel
static constexpr f32 kFadeStep = 8.0f / 255.0f;

InvisibleEffect::InvisibleEffect(BaseAliveGameObject* pTarget, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
     SetType(ReliveTypes::eInvisibleEffect);

    mTargetId = pTarget->mBaseGameObjectId;

    mSemiTrans = false;
    mBlending = false;
    mIsInvisible = false;

    if (pTarget->GetAnimation().GetSemiTrans())
    {
        mSemiTrans = true;
    }
    if (pTarget->GetAnimation().GetBlending())
    {
        mBlending = true;
    }

    mOldRenderMode = pTarget->GetAnimation().GetBlendMode();
    mState = InvisibleState::eSetBlendMode1_0;
}

InvisibleEffect::~InvisibleEffect()
{

}

void InvisibleEffect::InstantInvisibility()
{
    mIsInvisible = true;
    SetUpdateDelay(1);
    mState = InvisibleState::eSetInvisibile_1;
}

void InvisibleEffect::BecomeVisible()
{
    mState = InvisibleState::eBecomeVisible_4;
}

void InvisibleEffect::ClearInvisibility()
{
    SetUpdateDelay(1);
    mState = InvisibleState::eClearInvisibility_5;
}

void InvisibleEffect::BecomeInvisible()
{
    SetUpdateDelay(1);
    mState = InvisibleState::eSetInvisibile_1;
}

void InvisibleEffect::VUpdate()
{
    auto pTarget = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetId));
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (!pTarget || pTarget->GetDead())
    {
        SetDead(true);
        return;
    }

    switch (mState)
    {
        case InvisibleState::eSetBlendMode1_0:
        {
            pTarget->GetAnimation().SetBlendMode(relive::TBlendModes::eBlend_1);
            return;
        }
        case InvisibleState::eSetInvisibile_1:
        {
            // The original set the semi-transparency bit on all of the palette's entries
            // from the 8th, the first few stayed visible. A true colour sprite is all or nothing.
            pTarget->GetAnimation().Fx().mForceBlend = 1;

            pTarget->SetInvisible(true);

            pTarget->GetAnimation().SetBlending(false);
            pTarget->GetAnimation().SetSemiTrans(true);
            pTarget->GetAnimation().SetBlendMode(relive::TBlendModes::eBlend_1);

            SetUpdateDelay(1);
            mState = InvisibleState::eBecomeInvisible_2;
            if (mIsInvisible)
            {
                mState = InvisibleState::eUnknown_3;
            }
            return;
        }
        case InvisibleState::eBecomeInvisible_2:
        {
            // Every channel loses a bit a step, and with the sprite added to the picture
            // (blend mode 1) it fades away. When there is nothing left to take it stops.
            if (mFadeSteps < kMaxFadeSteps)
            {
                mFadeSteps++;
                pTarget->GetAnimation().Fx().mSubtract = mFadeSteps * kFadeStep;
                SetUpdateDelay(1);
            }
            else
            {
                mState = InvisibleState::eSetBlendMode1_0;
            }

            break;
        }
        case InvisibleState::eUnknown_3:
        {
            // Invisible straight away (when a save is loaded): nothing is left of the colours
            mFadeSteps = kMaxFadeSteps;
            pTarget->GetAnimation().Fx().mSubtract = mFadeSteps * kFadeStep;

            mIsInvisible = false;
            SetUpdateDelay(1);
            mState = InvisibleState::eSetBlendMode1_0;
            break;
        }
        case InvisibleState::eBecomeVisible_4:
        {
            // The colours come back a bit a step, slower than they went
            if (mFadeSteps > 0)
            {
                mFadeSteps--;
                pTarget->GetAnimation().Fx().mSubtract = mFadeSteps * kFadeStep;

                pTarget->GetAnimation().SetBlendMode(relive::TBlendModes::eBlend_1);
                SetUpdateDelay(5);
            }
            else
            {
                mState = InvisibleState::eClearInvisibility_5;
            }

            break;
        }
        case InvisibleState::eClearInvisibility_5:
        {
            // The sprite's own colours
            pTarget->GetAnimation().Fx().mSubtract = 0.0f;
            pTarget->GetAnimation().Fx().mForceBlend = 0;
            mFadeSteps = 0;

            pTarget->GetAnimation().SetSemiTrans(mSemiTrans);
            pTarget->GetAnimation().SetBlending(mBlending);
            pTarget->GetAnimation().SetBlendMode(mOldRenderMode);

            pTarget->SetInvisible(false);

            SetUpdateDelay(1);
            relive_new PossessionFlicker(pTarget, 16, 255, 128, 128, mResMan, mMap);
            mState = InvisibleState::eSetDead_6;
            break;
        }
        case InvisibleState::eSetDead_6:
        {
            SetDead(true);
            return;
        }
        default:
        {
            return;
        }
    }
}

void InvisibleEffect::VScreenChanged()
{

}
