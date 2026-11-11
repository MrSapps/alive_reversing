#include "stdafx.h"
#include "Flash.hpp"
#include "../relive_lib/Engine.hpp"
#include "../SerializedObjectData.hpp"

Flash::Flash(Layer layer, s32 r, s32 g, s32 b, ResourceManager& resMan, BaseMap& map, relive::TBlendModes abr, s32 time)
    : EffectBase(layer, abr, resMan, map)
{
    SetType(ReliveTypes::eFlash);
    mEffectBaseRed = static_cast<s16>(r);
    mEffectBaseGreen = static_cast<s16>(g);
    mEffectBaseBlue = static_cast<s16>(b);
    mFlashTime = time + sGnFrame;
}

void Flash::VUpdate()
{
    if (static_cast<s32>(sGnFrame) >= mFlashTime)
    {
        SetDead(true);
    }
}

void Flash::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    FlashSaveState data = {};
    data.mEffect = GetEffectState();
    data.mFlashTime = mFlashTime;
    pSaveBuffer.Write(data);
}

void Flash::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const auto pState = pData.ReadTmpPtr<FlashSaveState>();
    auto pFlash = relive_new Flash(static_cast<Layer>(pState->mEffect.mLayer), 0, 0, 0, resMan, map, static_cast<relive::TBlendModes>(pState->mEffect.mBlendMode), 0);
    pFlash->SetEffectState(pState->mEffect);
    pFlash->mFlashTime = pState->mFlashTime;
}
