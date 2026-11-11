#include "PaletteOverwriter.hpp"
#include "BaseAliveGameObject.hpp"
#include "../ObjectIds.hpp"
#include "../Animation.hpp"
#include "../SerializedObjectData.hpp"
#include <cstring>

PalleteOverwriter::PalleteOverwriter(const Guid& targetId, u32 sweepIndex, u32 kind, u8 r, u8 g, u8 b, bool addToUpdateList, ResourceManager& resMan, BaseMap& map)
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

PalOverwriterData PalleteOverwriter::GetData() const
{
    PalOverwriterData data;
    std::memset(static_cast<void*>(&data), 0, sizeof(data));
    data.mTargetId = mTargetId;
    data.mSweepIndex = mSweepIndex;
    data.mKind = mKind;
    data.mCurPalIndex = mCurPalIndex;
    data.mNumEntriesToOverwrite = mNumEntriesToOverwrite;
    data.mPalDepth = mPalDepth;
    data.mUpdateDelay = UpdateDelay();
    data.mR = mR;
    data.mG = mG;
    data.mB = mB;
    data.mFirstUpdate = mFirstUpdate;
    data.mDone = mDone;
    return data;
}

PalleteOverwriter* PalleteOverwriter::CreateFromData(const PalOverwriterData& data, bool addToUpdateList, ResourceManager& resMan, BaseMap& map)
{
    PalleteOverwriter* p = relive_new PalleteOverwriter(data.mTargetId, data.mSweepIndex, data.mKind, data.mR, data.mG, data.mB, addToUpdateList, resMan, map);
    if (p)
    {
        p->mCurPalIndex = data.mCurPalIndex;
        p->mNumEntriesToOverwrite = data.mNumEntriesToOverwrite;
        p->mPalDepth = data.mPalDepth;
        p->SetUpdateDelay(data.mUpdateDelay);
        p->mFirstUpdate = data.mFirstUpdate;
        p->mDone = data.mDone;
    }
    return p;
}

void PalleteOverwriter::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    PalleteOverwriterSaveState data = {};
    data.mData = GetData();
    pSaveBuffer.Write(data);
}

void PalleteOverwriter::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const PalleteOverwriterSaveState& state = *pData.ReadTmpPtr<PalleteOverwriterSaveState>();
    CreateFromData(state.mData, true, resMan, map);
}
