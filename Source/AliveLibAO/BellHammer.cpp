#include "stdafx_ao.h"
#include "BellHammer.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Elum.hpp"
#include "Abe.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Sfx.hpp"
#include "Midi.hpp"
#include "../relive_lib/Grid.hpp"
#include "Path.hpp"
#include "Map.hpp"
#include "../relive_lib/SerializedObjectData.hpp"

namespace AO {

void BellHammer::LoadAnimations()
{
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::BellHammer_Idle));
    mLoadedAnims.push_back(mResMan.LoadAnimation(AnimId::BellHammer_Smashing));
}


BellHammer::BellHammer(relive::Path_BellHammer* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map)
{
    SetType(ReliveTypes::eBellHammer);

    LoadAnimations();
    Animation_Init(GetAnimRes(AnimId::BellHammer_Idle));

    GetAnimation().SetSemiTrans(false);
    mSpawnElum = false;
    mState = BellHammerStates::eWaitForActivation_0;

    mXPos = FP_FromInteger(pTlv->mTopLeftX + 82);
    mYPos = FP_FromInteger(pTlv->mTopLeftY + 94);

    mSwitchId = pTlv->mSwitchId;
    mTlvInfo = tlvId;

    if (pTlv->mScale == relive::reliveScale::eHalf)
    {
        SetSpriteScale(FP_FromDouble(0.5));
        SetScale(Scale::Bg);
        GetAnimation().SetRenderLayer(Layer::eLayer_BeforeShadow_Half_6);
    }
    else
    {
        SetSpriteScale(FP_FromInteger(1));
        SetScale(Scale::Fg);
        GetAnimation().SetRenderLayer(Layer::eLayer_BeforeShadow_25);
    }

    if (pTlv->mDirection == relive::reliveXDirection::eRight)
    {
        GetAnimation().SetFlipX(true);
    }

    if (gElum)
    {
        return;
    }
}

BellHammer::~BellHammer()
{
    mMap.TLV_Reset(mTlvInfo);
}

void BellHammer::VScreenChanged()
{
    SetDead(true);
}

void BellHammer::VUpdate()
{
    switch (mState)
    {
        case BellHammerStates::eWaitForActivation_0:
            if (SwitchStates_Get(mSwitchId))
            {
                mState = BellHammerStates::eSmashingBell_1;
                GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::BellHammer_Smashing));
            }
            break;

        case BellHammerStates::eSmashingBell_1:
            if (GetAnimation().GetIsLastFrame())
            {
                mState = BellHammerStates::eWaitForActivation_0;
                GetAnimation().Set_Animation_Data(GetAnimRes(AnimId::BellHammer_Idle));
                SwitchStates_Set(mSwitchId, 0);

                // Spawn the foo if he ain't already here
                if (gElum == nullptr)
                {
                    mSpawnElum = true;
                }
            }
            else
            {
                // Play those bell smashing sounds
                if (GetAnimation().GetCurrentFrame() == 5)
                {
                    SfxPlayMono(relive::SoundEffects::RingBellHammer, 0);
                }
                else if (GetAnimation().GetCurrentFrame() == 15)
                {
                    SND_SEQ_PlaySeq(SeqId::eRingBellHammerAndExtraSound_13, 1, 1);
                }
            }
            break;

        default:
            break;
    }

    if (mSpawnElum)
    {
        mSpawnElum = false;
        Elum::Spawn(mTlvInfo, mResMan, mMap);

        PSX_Point mapCoords = {};
        mMap.GetCurrentCamCoords(&mapCoords);

        if (gElum)
        {
            gElum->mXPos = (FP_FromInteger(mapCoords.x + XGrid_Index_To_XPos_AO(GetSpriteScale(), 0))) - ScaleToGridSize(GetSpriteScale());
            gElum->mYPos = gElum->mYPos + FP_FromInteger(450);
        }
    }
}

void BellHammer::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    BellHammerSaveState data = {};

    data.mTlvInfo = mTlvInfo;
    data.mState = mState;

    pSaveBuffer.Write(data);
}

void BellHammer::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<BellHammerSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvInfo);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_BellHammer>(ReliveTypes::eBellHammer);

    auto pHammer = relive_new BellHammer(pTlv, pState->mTlvInfo, resMan, map);
    if (pHammer)
    {
        pHammer->mState = pState->mState;
        if (pHammer->mState == BellHammerStates::eSmashingBell_1)
        {
            pHammer->GetAnimation().Set_Animation_Data(pHammer->GetAnimRes(AnimId::BellHammer_Smashing));
        }
    }
}

} // namespace AO
