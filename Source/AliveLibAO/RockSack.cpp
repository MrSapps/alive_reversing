#include "stdafx_ao.h"
#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/Events.hpp"
#include "Math.hpp"
#include "Rock.hpp"
#include "RockSack.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Sfx.hpp"
#include "../relive_lib/GameObjects/ThrowableArray.hpp"
#include "../relive_lib/AnimResources.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "Abe.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/Shadow.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "Path.hpp"
#include "Map.hpp"

namespace AO {

void RockSack::LoadAnimations()
{
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::RockSack_Idle));
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::RockSack_SoftHit));
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::RockSack_HardHit));
}

RockSack::RockSack(relive::Path_RockSack* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : ::BaseAliveGameObject(0, resMan, map)
{
    SetType(ReliveTypes::eRockSack);

    LoadAnimations();
    Animation_Init(GetAnimRes(AnimId::RockSack_Idle));

    GetAnimation().SetSemiTrans(false);

    SetApplyShadowZoneColour(false);

    mTlvId = tlvId;
    mHasBeenHit = false;
    mXPos = FP_FromInteger(pTlv->mTopLeftX);
    mYPos = FP_FromInteger(pTlv->mTopLeftY);
    mTlvVelX = FP_FromRaw(pTlv->mVelX << 8);
    mTlvVelY = FP_FromRaw(-256 * pTlv->mVelY);

    if (pTlv->mRockFallDirection == relive::reliveXDirection::eLeft)
    {
        mTlvVelX = -mTlvVelX;
    }

    if (pTlv->mScale == relive::reliveScale::eHalf)
    {
        SetSpriteScale(FP_FromDouble(0.5));
        SetScale(Scale::Bg);
    }
    else
    {
        SetSpriteScale(FP_FromInteger(1));
        SetScale(Scale::Fg);
    }

    mRockAmount = pTlv->mRockAmount;
    mPlayWobbleSound = true;
    mForceWobbleSound = true;

    if (mMap.mCurrentLevel == EReliveLevelIds::eStockYards || mMap.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
    {
        mLoadedPals.push_back(resMan.LoadPal(PalId::BlueRockSack));
        GetAnimation().LoadPal(GetPalRes(PalId::BlueRockSack));
    }

    CreateShadow();
}

RockSack::~RockSack()
{
    mMap.TLV_Reset(mTlvId);
}

void RockSack::VScreenChanged()
{
    SetDead(true);
}

void RockSack::VUpdate()
{
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (GetAnimation().GetCurrentFrame() == 2)
    {
        if (mPlayWobbleSound)
        {
            if (Math_NextRandom() < 40u || mForceWobbleSound)
            {
                mPlayWobbleSound = false;
                mForceWobbleSound = false;
                SFX_Play_Pitch(relive::SoundEffects::SackWobble, 24, Math_RandomRange(-2400, -2200));
            }
        }
    }
    else
    {
        mPlayWobbleSound = true;
    }

    if (mHasBeenHit)
    {
        if (GetAnimation().GetIsLastFrame())
        {
            GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::RockSack_Idle));
            mHasBeenHit = false;
        }
    }
    else
    {
        if (GetAnimation().GetFrameChangeCounter() == 0)
        {
            GetAnimation().SetFrameChangeCounter(Math_RandomRange(2, 10));
        }

        const PSX_RECT bPlayerRect = gAbe->VGetBoundingRect();
        const PSX_RECT bRect = VGetBoundingRect();

        if (bRect.x <= bPlayerRect.w
            && bRect.w >= bPlayerRect.x
            && bRect.h >= bPlayerRect.y
            && bRect.y <= bPlayerRect.h
            && GetSpriteScale() == gAbe->GetSpriteScale())
        {
            if (gThrowableArray)
            {
                if (gThrowableArray->mCount)
                {
                    if (gAbe->mCurrentMotion == eAbeMotions::Motion_33_RunJumpMid)
                    {
                        GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::RockSack_HardHit));
                    }
                    else
                    {
                        GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::RockSack_SoftHit));
                    }
                    mHasBeenHit = true;
                    return;
                }
            }
            else
            {
                gThrowableArray = relive_new ThrowableArray(mResMan, mMap);
            }

            gThrowableArray->Add(mRockAmount);

            auto pRock = relive_new Rock(mXPos, mYPos - FP_FromInteger(30), mRockAmount, mResMan, mMap);
            if (pRock)
            {
                pRock->VThrow(mTlvVelX, mTlvVelY);
            }

            SfxPlayMono(relive::SoundEffects::SackHit, 0);
            Environment_SFX(EnvironmentSfx::eDeathNoise_7, 0, 0x7FFF, 0, mMap);

            if (gAbe->mCurrentMotion == eAbeMotions::Motion_33_RunJumpMid)
            {
                GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::RockSack_HardHit));
            }
            else
            {
                GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::RockSack_SoftHit));
            }

            mHasBeenHit = true;
        }
    }
}

void RockSack::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    if (GetElectrocuted())
    {
        return;
    }

    RockSackSaveState data = {};

    data.mXPos = mXPos;
    data.mYPos = mYPos;
    data.mCurrentPath = mCurrentPath;
    data.mCurrentLevel = mCurrentLevel;
    data.mSpriteScale = GetSpriteScale();
    data.mScale = GetScale();

    data.mTlvId = mTlvId;
    data.mHasBeenHit = mHasBeenHit;
    data.mRockAmount = mRockAmount;
    data.mPlayWobbleSound = mPlayWobbleSound;
    data.mForceWobbleSound = mForceWobbleSound;
    data.mTlvVelX = mTlvVelX;
    data.mTlvVelY = mTlvVelY;

    pSaveBuffer.Write(data);
}

void RockSack::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<RockSackSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_RockSack>(ReliveTypes::eRockSack);

    auto pSack = relive_new RockSack(pTlv, pState->mTlvId, resMan, map);
    if (pSack)
    {
        pSack->mXPos = pState->mXPos;
        pSack->mYPos = pState->mYPos;
        pSack->mCurrentPath = pState->mCurrentPath;
        pSack->mCurrentLevel = pState->mCurrentLevel;
        pSack->SetSpriteScale(pState->mSpriteScale);
        pSack->SetScale(pState->mScale);

        pSack->mTlvId = pState->mTlvId;
        pSack->mHasBeenHit = pState->mHasBeenHit;
        pSack->mRockAmount = pState->mRockAmount;
        pSack->mPlayWobbleSound = pState->mPlayWobbleSound;
        pSack->mForceWobbleSound = pState->mForceWobbleSound;
        pSack->mTlvVelX = pState->mTlvVelX;
        pSack->mTlvVelY = pState->mTlvVelY;
    }
}

}
