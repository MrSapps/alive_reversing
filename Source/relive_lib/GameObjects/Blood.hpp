#pragma once

#include "../Function.hpp"
#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../Layer.hpp"
#include "../SaveStateBase.hpp"


struct BloodParticle final
{
    FP x;
    FP y;
    FP mOffX;
    FP mOffY;
    Poly_FT4 mPoly;
};

struct BloodSaveState final : public SaveStateBase
{
    BloodSaveState()
        : SaveStateBase(ReliveTypes::eBlood, sizeof(*this))
    { }
    // What it was made with
    s32 mTotalBloodCount;
    FP mScale;
    AnimatedObjectSaveState mObject;
    s16 mBloodXPos;
    s16 mBloodYPos;
    u8 mRandSeed;
    s32 mCurrentBloodCount;
    s32 mUpdateCalls;
    s16 mOtLayer;
    struct Drop final
    {
        FP mX;
        FP mY;
        FP mOffX;
        FP mOffY;
    };
    // The ones that are gone are never looked at again
    Drop mDrops[64];
};

class Blood final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Blood(FP xpos, FP ypos, FP xOff, FP yOff, FP scale, s32 count, ResourceManager& resMan, BaseMap& map);
    ~Blood();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    s16 mBloodXPos = 0;
    s16 mBloodYPos = 0;
    u8 mRandSeed = 0;
    s32 mTotalBloodCount = 0;
    s32 mCurrentBloodCount = 0;
    BloodParticle* mBloodParticle = nullptr;
    s32 mUpdateCalls = 0;
    Layer mOtLayer = Layer::eLayer_0;
};
