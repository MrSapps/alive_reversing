#include "stdafx_ao.h"
#include "../relive_lib/Function.hpp"
#include "DoorLight.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Math.hpp"
#include "Engine.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "MusicTrigger.hpp"
#include "Abe.hpp"
#include "Midi.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"
#include "../relive_lib/Engine.hpp"
#include "Path.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "Map.hpp"

namespace AO {

static s32 sNextDoorLightUpdate = -1;
static s32 sDoorLightUpdateTimer = 0;

DoorLight::DoorLight(relive::Path_LightEffect* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map)
{
    mTlvId = tlvId;
    mWidth = pTlv->mSize;
    mHeight = pTlv->mSize;

    SetType(ReliveTypes::eNone);
    mHasSwitchId = false;
    mSwitchId = pTlv->mSwitchId;
    mSwitchState = SwitchStates_Get(pTlv->mSwitchId);

    s32 xOff = 0;
    switch (pTlv->mType)
    {
        case relive::Path_LightEffect::Type::GoldGlow:
        {
            mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::GoldGlow));
            Animation_Init(GetAnimRes(AnimId::GoldGlow));
            break;
        }

        case relive::Path_LightEffect::Type::GreenGlow:
        {
            mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::GreenGlow));
            Animation_Init(GetAnimRes(AnimId::GreenGlow));
            break;
        }

        case relive::Path_LightEffect::Type::FlintGlow:
        {
            mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::FlintGlow));
            Animation_Init(GetAnimRes(AnimId::FlintGlow));
            mHasSwitchId = true;
            break;
        }

        case relive::Path_LightEffect::Type::Switchable_RedGreenDoorLights:
        {
            mWidth = 0;
            mHeight = 0;
            if (SwitchStates_Get(pTlv->mSwitchId))
            {
                mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::GreenDoorLight));
                Animation_Init(GetAnimRes(AnimId::GreenDoorLight));
            }
            else
            {
                mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::RedDoorLight));
                Animation_Init(GetAnimRes(AnimId::RedDoorLight));
                xOff = 6;
            }
            break;
        }

        case relive::Path_LightEffect::Type::Switchable_RedGreenHubLight:
        {
            mWidth = 0;
            mHeight = 0;
            if (SwitchStates_Get(pTlv->mSwitchId))
            {
                mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::GreenHubLight));
                Animation_Init(GetAnimRes(AnimId::GreenHubLight));
            }
            else
            {
                mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::RedHubLight));
                Animation_Init(GetAnimRes(AnimId::RedHubLight));
            }
            break;
        }

        default:
            break;
    }

    GetAnimation().SetFlipX(pTlv->mDirection == relive::reliveXDirection::eLeft);

    if (sNextDoorLightUpdate < 0)
    {
        sNextDoorLightUpdate = sGnFrame;
        sDoorLightUpdateTimer = sNextDoorLightUpdate + Math_RandomRange(30, 45);
    }

    GetAnimation().SetIgnorePosOffset(true);

    SetApplyShadowZoneColour(false);
    GetAnimation().SetRenderLayer(Layer::eLayer_Foreground_Half_17);
    GetAnimation().SetBlendMode(relive::TBlendModes::eBlend_3);

    GetAnimation().SetBlending(false);
    GetAnimation().SetSemiTrans(true);

    if (GetAnimation().GetFlipX())
    {
        mXPos = FP_FromInteger(pTlv->mTopLeftX - xOff);
    }
    else
    {
        mXPos = FP_FromInteger(xOff + pTlv->mTopLeftX);
    }

    SetSpriteScale(FP_FromInteger(1));
    mYPos = FP_FromInteger(pTlv->mTopLeftY);
}

DoorLight::~DoorLight()
{
    mMap.TLV_Reset(mTlvId);
}

void DoorLight::VScreenChanged()
{
    SetDead(true);
    sNextDoorLightUpdate = -1;
}

void DoorLight::VUpdate()
{
    if (static_cast<s32>(sGnFrame) > sDoorLightUpdateTimer)
    {
        sNextDoorLightUpdate = MakeTimer(Math_RandomRange(6, 20));
        sDoorLightUpdateTimer = sNextDoorLightUpdate + Math_RandomRange(30, 45);
        mRGB.SetRGB(32, 32, 32);
    }
    else if (static_cast<s32>(sGnFrame) >= sNextDoorLightUpdate)
    {
        const FP lightAngle = (FP_FromInteger(128) * FP_FromInteger(sGnFrame - sNextDoorLightUpdate) / FP_FromInteger(sDoorLightUpdateTimer - sNextDoorLightUpdate));

        const FP lightAngleCosine = -Math_Cosine(FP_GetExponent(lightAngle) & 0xFF);
        const s32 rgbVal = FP_GetExponent(FP_FromInteger(255) * lightAngleCosine) + 32;

        u8 rgb = 0;
        if (rgbVal <= 255)
        {
            rgb = rgbVal & 0xFF;
        }
        else
        {
            rgb = 255;
        }

        if (mHasSwitchId)
        {
            if (SwitchStates_Get(mSwitchId))
            {
                if (mSwitchState == 0)
                {
                    mSwitchState = 1;

                    if (sControlledCharacter == gAbe)
                    {
                        relive_new MusicTrigger(relive::Path_MusicTrigger::MusicTriggerMusicType::eSecretAreaShort, relive::Path_MusicTrigger::TriggeredBy::eTouching, 0, 15, mResMan, mMap);
                    }
                    else
                    {
                        SND_SEQ_Play(SeqId::eSaveTriggerMusic_45, 1, 127, 127);
                    }
                }
                mRGB.SetRGB(32, rgb, 32);
            }
            else
            {
                mRGB.SetRGB(rgb, 32, 32);
            }
        }
        else
        {
            mRGB.SetRGB(rgb, rgb, rgb);
        }
    }
}

void DoorLight::VRender(OrderingTable& ot)
{
    if (gNumCamSwappers == 0)
    {
        const FP xpos = FP_FromInteger(gScreenManager->mCamXOff) + mXPos - gScreenManager->mCamPos->x;
        const FP ypos = FP_FromInteger(gScreenManager->mCamYOff) + mYPos - gScreenManager->mCamPos->y;

        GetAnimation().SetRGB(mRGB.r, mRGB.g, mRGB.b);

        GetAnimation().VRender(
            FP_GetExponent(FP_FromInteger((FP_GetExponent(xpos) - mWidth / 2))),
            FP_GetExponent(FP_FromInteger((FP_GetExponent(ypos) - mHeight / 2))),
            ot,
            mWidth,
            mHeight);
    }
}

void DoorLight::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    DoorLightSaveState data = {};

    data.mTlvId = mTlvId;
    data.mSwitchState = mSwitchState;
    data.mRed = mRGB.r;
    data.mGreen = mRGB.g;
    data.mBlue = mRGB.b;

    pSaveBuffer.Write(data);
}

void DoorLight::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<DoorLightSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    if (!tlvIterator.GetTlv() || tlvIterator.GetTlv()->mTlvType != ReliveTypes::eLightEffect)
    {
        // The saved tlv-info didn't resolve back to a TLV of the expected
        // type (can happen if two TLVs ended up sharing the same id) -
        // nothing safe to do here, so drop this record.
        return;
    }
    auto pTlv = tlvIterator.GetTlv<relive::Path_LightEffect>();

    auto pLight = relive_new DoorLight(pTlv, pState->mTlvId, resMan, map);
    if (pLight)
    {
        pLight->mSwitchState = pState->mSwitchState;
        pLight->mRGB.SetRGB(pState->mRed, pState->mGreen, pState->mBlue);
    }
}

} // namespace AO
