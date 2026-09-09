#include "stdafx_ao.h"
#include "MeatSack.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/Events.hpp"
#include "Path.hpp"
#include "Math.hpp"
#include "Sfx.hpp"
#include "../relive_lib/GameObjects/ThrowableArray.hpp"
#include "Abe.hpp"
#include "Meat.hpp"
#include "Map.hpp"

namespace AO
{

void MeatSack::LoadAnimations()
{
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::MeatSack_Hit));
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::MeatSack_Idle));
}

MeatSack::MeatSack(relive::Path_MeatSack* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : ::BaseAliveGameObject(0, resMan, map)
{
    SetType(ReliveTypes::eMeatSack);

    LoadAnimations();

    Animation_Init(GetAnimRes(AnimId::MeatSack_Idle));

    SetApplyShadowZoneColour(false);
    mTlvId = tlvId;

    mHasBeenHit = false;

    mXPos = FP_FromInteger(pTlv->mTopLeftX);
    mYPos = FP_FromInteger(pTlv->mTopLeftY);

    mTlvVelX = FP_FromRaw(pTlv->mVelX << 8);

    // Throw the meat up into the air as it falls from the sack
    mTlvVelY = -FP_FromRaw(pTlv->mVelY << 8);

    if (pTlv->mMeatFallDirection == relive::reliveXDirection::eLeft)
    {
        mTlvVelX = -mTlvVelX;
    }

    if (pTlv->mScale == relive::reliveScale::eHalf)
    {
        SetSpriteScale(FP_FromDouble(0.5));
        GetAnimation().SetRenderLayer(Layer::eLayer_8);
        SetScale(Scale::Bg);
    }
    else
    {
        SetSpriteScale(FP_FromInteger(1));
        GetAnimation().SetRenderLayer(Layer::eLayer_27);
        SetScale(Scale::Fg);
    }

    mMeatAmount = pTlv->mMeatAmount;

    CreateShadow();
}

MeatSack::~MeatSack()
{
    mMap.TLV_Reset(mTlvId);
}

void MeatSack::VUpdate()
{
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }

    if (GetAnimation().GetCurrentFrame() == 2)
    {
        if (mPlayWobbleSound)
        {
            if (Math_NextRandom() < 40u)
            {
                mPlayWobbleSound = false;
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
            GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::MeatSack_Idle));
            mHasBeenHit = false;
        }
        return;
    }

    const PSX_RECT abeRect = gAbe->VGetBoundingRect();
    const PSX_RECT ourRect = VGetBoundingRect();

    if (ourRect.Overlaps(abeRect))
    {
        if (GetSpriteScale() == gAbe->GetSpriteScale())
        {
            if (!gThrowableArray)
            {
                gThrowableArray = relive_new ThrowableArray(mResMan, mMap);
            }

            if (gThrowableArray)
            {
                if (gThrowableArray->mCount > 0)
                {
                    GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::MeatSack_Hit));
                    mHasBeenHit = true;
                    return;
                }

                gThrowableArray->Add(mMeatAmount);
            }

            auto pMeat = relive_new Meat(
                mXPos,
                mYPos - FP_FromInteger(30),
                mMeatAmount,
                mResMan, mMap);
            if (pMeat)
            {
                pMeat->VThrow(mTlvVelX, mTlvVelY);
                pMeat->SetSpriteScale(GetSpriteScale());
            }

            SfxPlayMono(relive::SoundEffects::SackHit, 0);
            Environment_SFX(EnvironmentSfx::eDeathNoise_7, 0, 0x7FFF, nullptr, mMap);

            GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::MeatSack_Hit));
            mHasBeenHit = true;
            return;
        }
    }
}

void MeatSack::VScreenChanged()
{
    SetDead(true);
}

Meat::Meat(FP xpos, FP ypos, s16 count, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseThrowable(resMan, map)
{
    mBaseThrowableDead = 0;

    SetType(ReliveTypes::eMeat);

    mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::Meat));
    Animation_Init(GetAnimRes(AnimId::Meat));

    mXPos = xpos;
    mYPos = ypos;

    mPreviousXPos = xpos;
    mPreviousYPos = ypos;

    mVelX = FP_FromInteger(0);
    mVelY = FP_FromInteger(0);
    mShimmerTimer = 0;
    SetInteractive(false);

    GetAnimation().SetRender(false);
    GetAnimation().SetSemiTrans(false);

    mDeadTimer = MakeTimer(600);
    mPathLine = 0;
    mBaseThrowableCount = count;
    mState = 0;

    CreateShadow();
}

void MeatSack::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    if (GetElectrocuted())
    {
        return;
    }

    MeatSackSaveState data = {};

    data.mXPos = mXPos;
    data.mYPos = mYPos;
    data.mCurrentPath = mCurrentPath;
    data.mCurrentLevel = mCurrentLevel;
    data.mSpriteScale = GetSpriteScale();
    data.mScale = GetScale();

    data.mTlvId = mTlvId;
    data.mHasBeenHit = mHasBeenHit;
    data.mMeatAmount = mMeatAmount;
    data.mPlayWobbleSound = mPlayWobbleSound;
    data.mTlvVelX = mTlvVelX;
    data.mTlvVelY = mTlvVelY;

    pSaveBuffer.Write(data);
}

void MeatSack::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<MeatSackSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    if (!tlvIterator.GetTlv() || tlvIterator.GetTlv()->mTlvType != ReliveTypes::eMeatSack)
    {
        // The saved tlv-info didn't resolve back to a TLV of the expected
        // type (can happen if two TLVs ended up sharing the same id) -
        // nothing safe to do here, so drop this record.
        return;
    }
    auto pTlv = tlvIterator.GetTlv<relive::Path_MeatSack>();

    auto pSack = relive_new MeatSack(pTlv, pState->mTlvId, resMan, map);
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
        pSack->mMeatAmount = pState->mMeatAmount;
        pSack->mPlayWobbleSound = pState->mPlayWobbleSound;
        pSack->mTlvVelX = pState->mTlvVelX;
        pSack->mTlvVelY = pState->mTlvVelY;
    }
}

} // namespace AO
