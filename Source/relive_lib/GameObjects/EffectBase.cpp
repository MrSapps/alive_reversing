#include "stdafx.h"
#include "EffectBase.hpp"
#include "MapWrapper.hpp"
#include "PsxDisplay.hpp"
#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "BaseMap.hpp"

EffectBase::EffectBase(Layer layer, relive::TBlendModes blendMode, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
    , mBlendMode(blendMode)
{
    SetType(ReliveTypes::eEffectBase);
    gObjListDrawables->Push_Back(this);
    SetDrawable(true);
    mEffectBasePathId = mMap.mCurrentPath;
    mEffectBaseLevelId = mMap.mCurrentLevel;
    mEffectBaseLayer = layer;
    mSemiTrans = 1;
}

EffectBase::~EffectBase()
{
    gObjListDrawables->Remove_Item(this);
}

std::vector<BaseGameObject::NotStateMemory> EffectBase::VNotStateMemory()
{
    // What it was last drawn with
    return {{&mEffectBaseTile, sizeof(mEffectBaseTile)}};
}

EffectBaseSaveState EffectBase::GetEffectState() const
{
    EffectBaseSaveState state = {};
    state.mBlendMode = static_cast<u32>(mBlendMode);
    state.mLayer = static_cast<s16>(mEffectBaseLayer);
    state.mPathId = mEffectBasePathId;
    state.mLevelId = static_cast<s16>(mEffectBaseLevelId);
    state.mRed = mEffectBaseRed;
    state.mGreen = mEffectBaseGreen;
    state.mBlue = mEffectBaseBlue;
    state.mSemiTrans = mSemiTrans;
    return state;
}

void EffectBase::SetEffectState(const EffectBaseSaveState& state)
{
    mEffectBasePathId = state.mPathId;
    mEffectBaseLevelId = static_cast<EReliveLevelIds>(state.mLevelId);
    mEffectBaseRed = state.mRed;
    mEffectBaseGreen = state.mGreen;
    mEffectBaseBlue = state.mBlue;
    mSemiTrans = state.mSemiTrans;
}

void EffectBase::VRender(OrderingTable& ot)
{
    Poly_G4* pTile = &mEffectBaseTile;
    pTile->SetRGB0(static_cast<u8>(mEffectBaseRed), static_cast<u8>(mEffectBaseGreen), static_cast<u8>(mEffectBaseBlue));
    pTile->SetRGB1(static_cast<u8>(mEffectBaseRed), static_cast<u8>(mEffectBaseGreen), static_cast<u8>(mEffectBaseBlue));
    pTile->SetRGB2(static_cast<u8>(mEffectBaseRed), static_cast<u8>(mEffectBaseGreen), static_cast<u8>(mEffectBaseBlue));
    pTile->SetRGB3(static_cast<u8>(mEffectBaseRed), static_cast<u8>(mEffectBaseGreen), static_cast<u8>(mEffectBaseBlue));  
    pTile->SetXYWH(0, 0, gPsxDisplay.mWidth, gPsxDisplay.mHeight);
    pTile->SetBlendMode(mBlendMode);

    if (mEffectBaseRed || mEffectBaseGreen || mEffectBaseBlue || !mSemiTrans)
    {
        pTile->SetSemiTransparent(mSemiTrans);
        ot.Add(mEffectBaseLayer, pTile);
    }
}
