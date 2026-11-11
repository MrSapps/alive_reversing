#include "stdafx.h"
#include "../ObjectIds.hpp"
#include "../SerializedObjectData.hpp"
#include "BackgroundAnimation.hpp"
#include "../Animation.hpp"
#include "../Sound/Midi.hpp"
#include "../../AliveLibAE/stdlib.hpp"
#include "../Events.hpp"
#include "Sfx.hpp"
#include "../MapWrapper.hpp"
#include "../GameType.hpp"
#include "BaseMap.hpp"

void BackgroundAnimation::VStopAudio()
{
    if (mSoundChannelsMask)
    {
        SND_Stop_Channels_Mask(mSoundChannelsMask);
        mSoundChannelsMask = 0;
    }
}

BackgroundAnimation::BackgroundAnimation(relive::Path_BackgroundAnimation* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map)
    : BaseAnimatedWithPhysicsGameObject(0, resMan, map),
    mTlvId(tlvId)
{
    SetType(ReliveTypes::eBackgroundAnimation);

    mXPos = FP_FromInteger(pTlv->mTopLeftX);
    mYPos = FP_FromInteger(pTlv->mTopLeftY);

    mObjectXPos = FP_FromInteger(pTlv->mTopLeftX);
    mObjectYPos = FP_FromInteger(pTlv->mTopLeftY);

    const AnimRecord& anim = PerGameBgAnimRec(pTlv->mAnimId);
    AnimResource res = mResMan.LoadAnimation(anim.mId);

    Animation_Init(res);

    GetAnimation().SetSemiTrans(pTlv->mIsSemiTrans);
    GetAnimation().SetBlending(true);

    GetAnimation().SetBlendMode(pTlv->mSemiTransMode);

    GetAnimation().SetRenderLayer(Layer::eLayer_1);

    if (GetGameType() == GameType::eAe)
    {
        if (pTlv->mLayer > relive::Path_BackgroundAnimation::Layer::eLayer0)
        {
            const s32 translatedLayer = static_cast<s32>(pTlv->mLayer) - 1;
            if (!translatedLayer)
            {
                GetAnimation().SetRenderLayer(Layer::eLayer_Above_FG1_Half_20);
            }
            if (translatedLayer == 1)
            {
                GetAnimation().SetRenderLayer(Layer::eLayer_Above_FG1_39);
            }
        }
    }
    else
    {
        mYOffset = 0;
        mSoundEffect = pTlv->mSoundEffect;
    }
}

void BackgroundAnimation::VUpdate()
{
    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }
    else
    {
        if (!mSoundChannelsMask)
        {
            switch (mSoundEffect)
            {
                case relive::Path_BackgroundAnimation::BgAnimSounds::eFire:
                    mSoundChannelsMask = SfxPlayMono(relive::SoundEffects::LoudFire, 0);
                    break;
            }
        }
        mXPos = mObjectXPos;
        mYPos = mObjectYPos;
    }
}

void BackgroundAnimation::VScreenChanged()
{
    SetDead(true);
}

BackgroundAnimation::~BackgroundAnimation()
{
    mMap.TLV_Reset(mTlvId);
    if (mSoundChannelsMask)
    {
        SND_Stop_Channels_Mask(mSoundChannelsMask);
    }
}

std::vector<BaseGameObject::NotStateMemory> BackgroundAnimation::VNotStateMemory()
{
    // The channels its sound plays on
    return {{&mSoundChannelsMask, sizeof(mSoundChannelsMask)}};
}

void BackgroundAnimation::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    BackgroundAnimationSaveState data = {};
    data.mSoundChannelsMask = 0; // the sound system's channels, not state: the sound is made again
    pSaveBuffer.Write(data);
}

void BackgroundAnimation::RestoreFromSaveState(const BackgroundAnimationSaveState& state)
{
    BaseGameObject* pObj = sObjectIds.Find_Impl(state.mObjectId);
    if (!pObj || pObj->Type() != ReliveTypes::eBackgroundAnimation)
    {
        return;
    }
    BackgroundAnimation* p = static_cast<BackgroundAnimation*>(pObj);
    p->mSoundChannelsMask = state.mSoundChannelsMask;
}
