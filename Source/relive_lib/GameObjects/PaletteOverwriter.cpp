#include "PaletteOverwriter.hpp"
#include "BaseAliveGameObject.hpp"
#include "../ObjectIds.hpp"
#include "../Animation.hpp"

PalleteOverwriter::PalleteOverwriter(const Guid& targetId, u32 sweepIndex, u32 kind, u8 r, u8 g, u8 b, bool addToUpdateList, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(addToUpdateList, 0, resMan, map), mTargetId(targetId), mSweepIndex(sweepIndex), mKind(kind), mR(r), mG(g), mB(b)
{
    SetType(ReliveTypes::ePalOverwriter);

    gObjListDrawables->Push_Back(this);

    mPalDepth = 64;
    if (auto pTarget = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetId)))
    {
        // The original sprite's palette: the colours go in a few at a time, so it took as many
        // updates as the palette had entries
        mPalDepth = pTarget->GetAnimation().PaletteDepth();
    }

    SetDrawable(true);

    mNumEntriesToOverwrite = 8;
    mCurPalIndex = 1;
    mFirstUpdate = true;
    mDone = false;
}

PalleteOverwriter::~PalleteOverwriter()
{
    gObjListDrawables->Remove_Item(this);
}

void PalleteOverwriter::VScreenChanged()
{
    // Stayin' alive
}

void PalleteOverwriter::VRender(OrderingTable& /*ot*/)
{
    if (mDone)
    {
        return;
    }

    auto pTarget = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetId));
    if (!pTarget)
    {
        return;
    }

    // The entries from 1 up to the end of this go are overwritten. Entry 0 is the transparent
    // colour, the original never touched it.
    pTarget->GetAnimation().Fx().SetLuma(pTarget->GetAnimation().mAnimRes.mJsonPtr->mAttributes.mLuma);
    SpriteFx::Sweep& sweep = pTarget->GetAnimation().Fx().mSweeps[mSweepIndex];
    sweep.mR = mR / 255.0f;
    sweep.mG = mG / 255.0f;
    sweep.mB = mB / 255.0f;
    sweep.mKind = mKind;
    const f32 entries = static_cast<f32>(mPalDepth > 1 ? mPalDepth - 1 : 1);
    sweep.mProgress = std::min(1.0f, (mCurPalIndex + mNumEntriesToOverwrite - 1) / entries);
}

void PalleteOverwriter::VUpdate()
{
    if (mFirstUpdate || mDone)
    {
        // First time round or when done do nothing
        mFirstUpdate = false;
        return;
    }

    if (mCurPalIndex == mPalDepth - 1)
    {
        // Got to the end
        mDone = true;
        LOG_INFO("pal overwriter done");
        return;
    }

    mCurPalIndex += 8;

    // Don't go out of bounds (overflow)
    if (mCurPalIndex >= mPalDepth - 1)
    {
        mCurPalIndex = mPalDepth - 1;
    }

    // Don't let the amount of entries to overwrite go out of bounds (overflow)
    if (mCurPalIndex + mNumEntriesToOverwrite >= mPalDepth - 1)
    {
        mNumEntriesToOverwrite = mPalDepth - mCurPalIndex;
    }
}
