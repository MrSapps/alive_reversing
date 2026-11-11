#include "stdafx.h"
#include "../ObjectIds.hpp"
#include "../SerializedObjectData.hpp"
#include "RollingBallShaker.hpp"
#include "../Function.hpp"
#include "../PsxDisplay.hpp"
#include "../Primitives.hpp"
#include "BaseAnimatedWithPhysicsGameObject.hpp"

// NOTE: this is the AO code and hasn't been compared since it's unused in original AE levels

const static PSX_Pos16 sRollingBallShakerScreenOffsets[18] = {
    {1, 0},
    {0, 0},
    {-1, 1},
    {0, 0},
    {-1, -1},
    {0, 0},
    {1, -1},
    {0, 0},
    {0, 1},
    {0, 0},
    {1, 0},
    {0, 0},
    {1, 1},
    {0, 0},
    {-1, -1},
    {0, 0},
    {0, -1},
    {0, 0}};

RollingBallShaker::RollingBallShaker(ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetDrawable(true);

    SetType(ReliveTypes::eRollingBallStopperShaker);
    mShakeTableIdx = 0;
    mStopShaking = false; // Set externally
    gObjListDrawables->Push_Back(this);
}

RollingBallShaker::~RollingBallShaker()
{
    gObjListDrawables->Remove_Item(this);
}

void RollingBallShaker::VUpdate()
{
    mShakeTableIdx++;

    if (mShakeTableIdx >= ALIVE_COUNTOF(sRollingBallShakerScreenOffsets))
    {
        mShakeTableIdx = 0;
    }
}

std::vector<BaseGameObject::NotStateMemory> RollingBallShaker::VNotStateMemory()
{
    // What it was last drawn with
    return {{&mPrimScreenOffset, sizeof(mPrimScreenOffset)}};
}

void RollingBallShaker::VRender(OrderingTable& ot)
{
    if (mStopShaking)
    {
        // Unshake the screen
        mPrimScreenOffset.SetOffset(0, 0);
        ot.Add(Layer::eLayer_0, &mPrimScreenOffset);

        // Kill yourself
        SetDead(true);
    }
    else
    {
        mPrimScreenOffset.SetOffset(sRollingBallShakerScreenOffsets[mShakeTableIdx].x, sRollingBallShakerScreenOffsets[mShakeTableIdx].y);
        ot.Add(Layer::eLayer_0, &mPrimScreenOffset);
    }
}

void RollingBallShaker::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    RollingBallShakerSaveState data = {};
    data.mShakeTableIdx = mShakeTableIdx;
    data.mStopShaking = mStopShaking;
    pSaveBuffer.Write(data);
}

void RollingBallShaker::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const RollingBallShakerSaveState& state = *pData.ReadTmpPtr<RollingBallShakerSaveState>();
    RollingBallShaker* p = relive_new RollingBallShaker(resMan, map);
    if (!p)
    {
        return;
    }
    p->mShakeTableIdx = state.mShakeTableIdx;
    p->mStopShaking = state.mStopShaking;
}
