#pragma once

#include "../relive_lib/GameObjects/BaseThrowable.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace AO {

struct MeatSaveState final : public SaveStateBase
{
    MeatSaveState()
        : SaveStateBase(ReliveTypes::eMeat, sizeof(*this))
    { }
    s16 mState;
    FP mPreviousXPos;
    FP mPreviousYPos;
    s32 mShimmerTimer;
    s32 mDeadTimer;
    s16 mBaseThrowableCount;
    s16 mBaseThrowableDead;
    s32 mPathLineType;
};

class Meat final : public ::BaseThrowable
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    Meat(FP xpos, FP ypos, s16 count, ResourceManager& resMan, BaseMap& map);
    ~Meat();

    virtual void VScreenChanged() override;
    virtual void VThrow(FP velX, FP velY) override;
    virtual bool VCanThrow() override;
    virtual bool VCanEatMe();
    virtual bool VIsFalling() override;
    virtual void VTimeToExplodeRandom() override;
    virtual void VPostRestore() override;
    virtual void VUpdate() override;
    virtual void VOnTrapDoorOpen() override;
    virtual s16 VGetCount() override;

    bool VOnPlatformIntersection(BaseAnimatedWithPhysicsGameObject* pPlatform) override
    {
        return OnCollision(pPlatform);
    }

private:
    void InTheAir();
    void AddToPlatform();
    bool OnCollision(BaseAnimatedWithPhysicsGameObject* pObj);

private:
    s16 mState = 0; // TODO: enum - we can probably use the AE enum
    FP mPreviousXPos = {};
    FP mPreviousYPos = {};
    s32 mShimmerTimer = 0;
    s32 mDeadTimer = 0;

public:
    // Only between a restore and VPostRestore: the type of line it was on
    s32 mSavedPathLineType = -1;
    PathLine* mPathLine = nullptr;
};


} // namespace AO
