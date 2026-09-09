#include "stdafx_ao.h"
#include "../relive_lib/Function.hpp"
#include "HoistRocksEffect.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Math.hpp"
#include "../relive_lib/Collisions.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Path.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "Map.hpp"

namespace AO {

HoistParticle::HoistParticle(FP xpos, FP ypos, FP scale, AnimId animId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map), mAnimId(animId)
{
    mXPos = xpos;
    mYPos = ypos;

    /*u16 maxW = 7;
    if (mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarms || mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarmsReturn)
    {
        maxW = 5;
    }
    */

    mLoadedAnims.push_back(resMan.LoadAnimation(animId));
    Animation_Init(GetAnimRes(animId));
    // TODO: Add way to override anim height
    //Animation_Init(frameTableOffset, maxW, 4, ppRes);


    SetSpriteScale(scale);

    if (scale == FP_FromInteger(1))
    {
        GetAnimation().SetRenderLayer(Layer::eLayer_BeforeShadow_25);
    }
    else
    {
        GetAnimation().SetRenderLayer(Layer::eLayer_BeforeShadow_Half_6);
    }

    mHitGround = false;
}

void HoistParticle::VUpdate()
{
    if (mVelY >= (GetSpriteScale() * FP_FromInteger(10)))
    {
        if (!mMap.Is_Point_In_Current_Camera(
                mCurrentLevel,
                mCurrentPath,
                mXPos,
                mYPos,
                0))
        {
            SetDead(true);
        }
    }
    else
    {
        mVelY += (GetSpriteScale() * FP_FromDouble(0.6));
    }

    const FP oldY = mYPos;
    mYPos += mVelY;

    if (!mHitGround)
    {
        PathLine* pLine = nullptr;
        FP hitX = {};
        FP hitY = {};
        if (gCollisions->Raycast(
                mXPos,
                oldY,
                mXPos,
                mYPos,
                &pLine,
                &hitX,
                &hitY,
                GetSpriteScale() != FP_FromDouble(0.5) ? kFgWallsOrFloor : kBgWallsOrFloor))
        {
            mYPos = hitY;
            mVelY = (mVelY * FP_FromDouble(-0.3));
            mHitGround = true;
        }
    }
}

void HoistParticle::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    HoistParticleSaveState data = {};
    data.mXPos = mXPos;
    data.mYPos = mYPos;
    data.mVelY = mVelY;
    data.mScale = GetSpriteScale();
    data.mAnimId = mAnimId;
    data.mHitGround = mHitGround;
    data.mCurrentPath = mCurrentPath;
    data.mCurrentLevel = mCurrentLevel;
    pSaveBuffer.Write(data);
}

void HoistParticle::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<HoistParticleSaveState>();
    auto pParticle = relive_new HoistParticle(pState->mXPos, pState->mYPos, pState->mScale, pState->mAnimId, resMan, map);
    if (pParticle)
    {
        pParticle->mVelY = pState->mVelY;
        pParticle->mHitGround = pState->mHitGround;
        pParticle->mCurrentPath = pState->mCurrentPath;
        pParticle->mCurrentLevel = pState->mCurrentLevel;
    }
}

void HoistRocksEffect::VScreenChanged()
{
    SetDead(true);
}

HoistRocksEffect::~HoistRocksEffect()
{
    mMap.TLV_Reset(mTlvId);
}

HoistRocksEffect::HoistRocksEffect(relive::Path_Hoist* pTlv, const Guid& tlvInfo, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map), mTlvId(tlvInfo)
{
    mTlvXPos = FP_FromInteger(pTlv->mTopLeftX + 12);
    mTlvYPos = FP_FromInteger(pTlv->mTopLeftY);
}

void HoistRocksEffect::VUpdate()
{
    const auto rnd = Math_RandomRange(1, 4) - 2;
    if (rnd)
    {
        if (rnd == 1)
        {
            AnimId hoistRock = AnimId::AO_HoistRock2;
            if (mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarms || mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarmsReturn)
            {
                hoistRock = AnimId::RuptureFarms_HoistRock2;
            }

            const FP y = mTlvYPos + FP_FromInteger(Math_RandomRange(-4, 4));
            const FP x = mTlvXPos + FP_FromInteger(Math_RandomRange(-8, 8));
            relive_new HoistParticle(
                x, y,
                FP_FromInteger(1),
                hoistRock,
                mResMan, mMap);

            SetUpdateDelay(Math_RandomRange(30, 50));
        }
        else
        {
            AnimId hoistRock = AnimId::AO_HoistRock3;
            if (mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarms || mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarmsReturn)
            {
                hoistRock = AnimId::RuptureFarms_HoistRock3;
            }

            const FP y = mTlvYPos + FP_FromInteger(Math_RandomRange(-4, 4));
            const FP x = mTlvXPos + FP_FromInteger(Math_RandomRange(-8, 8));
            relive_new HoistParticle(
                x, y,
                FP_FromInteger(1),
                hoistRock,
                mResMan, mMap);

            SetUpdateDelay(Math_RandomRange(5, 10));
        }
    }
    else
    {
        AnimId hoistRock = AnimId::AO_HoistRock1;
        if (mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarms || mMap.mCurrentLevel == EReliveLevelIds::eRuptureFarmsReturn)
        {
            hoistRock = AnimId::RuptureFarms_HoistRock1;
        }

        const FP y = mTlvYPos + FP_FromInteger(Math_RandomRange(-4, 4));
        const FP x = mTlvXPos + FP_FromInteger(Math_RandomRange(-8, 8));
        relive_new HoistParticle(
            x, y,
            FP_FromInteger(1),
            hoistRock,
            mResMan, mMap);

        SetUpdateDelay(Math_RandomRange(10, 20));
    }
}

void HoistRocksEffect::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    HoistRocksEffectSaveState data = {};
    data.mTlvId = mTlvId;
    pSaveBuffer.Write(data);
}

void HoistRocksEffect::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<HoistRocksEffectSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_Hoist>(ReliveTypes::eHoist);

    relive_new HoistRocksEffect(pTlv, pState->mTlvId, resMan, map);
}

} // namespace AO
