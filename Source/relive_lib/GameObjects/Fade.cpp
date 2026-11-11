#include "stdafx.h"
#include "Fade.hpp"
#include "../../AliveLibAE/stdlib.hpp"
#include "../FatalError.hpp"
#include "../GameType.hpp"
#include "../SerializedObjectData.hpp"
#include "../WorldStatic.hpp"

static WorldStatic<bool> sIsFadingOut("Fade.sIsFadingOut", false);

Fade::Fade(Layer layer, FadeOptions fade, bool destroyOnDone, s32 speed, relive::TBlendModes abr, ResourceManager& resMan, BaseMap& map)
    : EffectBase(layer, abr, resMan, map)
{
    // TODO: sort this out
    if (GetGameType() == GameType::eAe)
    {
        SetType(ReliveTypes::eMainMenuTransistion);
    }
    else
    {
        SetType(ReliveTypes::eFade);
    }

    if (fade == FadeOptions::eFadeIn)
    {
        mFadeColour = 0;
    }
    else if (fade == FadeOptions::eFadeOut)
    {
        mFadeColour = 255;
    }

    Init(layer, fade, destroyOnDone, speed);

    mEffectBaseBlue = mFadeColour;
    mEffectBaseGreen = mFadeColour;
    mEffectBaseRed = mFadeColour;
}

Fade::~Fade()
{

}

void Fade::VScreenChanged()
{
    // Empty
}

void Fade::Init(Layer layer, FadeOptions fade, bool destroyOnDone, s32 speed)
{
    mEffectBaseLayer = layer;
    mFadeOption = fade;
    mDone = false;
    mDestroyOnDone = destroyOnDone;

    if (fade == FadeOptions::eFadeOut)
    {
        mSpeed = -speed;
    }
    else if (fade == FadeOptions::eFadeIn)
    {
        mSpeed = speed;
    }

    sIsFadingOut = true;
}

void Fade::VUpdate()
{
    if (!mDone)
    {
        mFadeColour += mSpeed;
        if (mFadeOption == FadeOptions::eFadeIn)
        {
            if (mFadeColour > 255)
            {
                mFadeColour = 255;
            }
        }
        else if (mFadeOption == FadeOptions::eFadeOut)
        {
            if (mFadeColour < 0)
            {
                mFadeColour = 0;
            }
        }
    }
}

void Fade::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    FadeSaveState data = {};
    // The type of a fade is the game's own
    data.mType = Type();
    data.mEffect = GetEffectState();
    data.mFadeOption = static_cast<s32>(mFadeOption);
    data.mDestroyOnDone = mDestroyOnDone;
    data.mSpeed = mSpeed;
    data.mFadeColour = mFadeColour;
    data.mDone = mDone;
    pSaveBuffer.Write(data);
}

void Fade::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const auto pState = pData.ReadTmpPtr<FadeSaveState>();
    auto pFade = relive_new Fade(static_cast<Layer>(pState->mEffect.mLayer), FadeOptions::eFadeIn, false, 0, static_cast<relive::TBlendModes>(pState->mEffect.mBlendMode), resMan, map);
    pFade->SetEffectState(pState->mEffect);
    pFade->mFadeOption = static_cast<FadeOptions>(pState->mFadeOption);
    pFade->mDestroyOnDone = pState->mDestroyOnDone;
    pFade->mSpeed = pState->mSpeed;
    pFade->mFadeColour = pState->mFadeColour;
    pFade->mDone = pState->mDone;
}

void Fade::VRender(OrderingTable& ot)
{
    mEffectBaseBlue = mFadeColour;
    mEffectBaseGreen = mFadeColour;
    mEffectBaseRed = mFadeColour;

    EffectBase::VRender(ot);

    if ((mFadeColour == 255 && mFadeOption == FadeOptions::eFadeIn) ||
        (mFadeColour == 0 && mFadeOption == FadeOptions::eFadeOut))
    {
        mDone = true;
        // we skip this if statement in AO because sIsFadingOut is only used in AE
        if (!sIsFadingOut || GetGameType() == GameType::eAo)
        {
            if (mDestroyOnDone)
            {
                SetDead(true);
            }
        }
        else
        {
            sIsFadingOut = false;
        }
    }
}
