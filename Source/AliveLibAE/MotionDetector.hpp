#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/Layer.hpp"

class BaseAliveGameObject;

namespace relive
{
    struct Path_MotionDetector;
}

class MotionDetectorLaser final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    MotionDetectorLaser(FP xpos, FP ypos, FP scale, Layer layer, ResourceManager& resMan, BaseMap& map);

};


struct MotionDetectorSaveState final : public SaveStateBase
{
    MotionDetectorSaveState()
        : SaveStateBase(ReliveTypes::eMotionDetector, sizeof(*this))
    { }
    Guid mLaserId;
    Guid mOwnerId;
    s32 mState;
    s32 mPauseTimer;
    bool mHasOwner;
    bool mDontComeBack;
    FP mTopLeftX;
    FP mTopLeftY;
    FP mBottomRightX;
    FP mBottomRightY;
    FP mSpeed;
    bool mObjectInLaser;
};

class MotionDetector final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const MotionDetectorSaveState& state);

    MotionDetector(relive::Path_MotionDetector* pTlv, const Guid& tlvId, BaseAnimatedWithPhysicsGameObject* pOwner, ResourceManager& resMan, BaseMap& map);
    ~MotionDetector();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VScreenChanged() override;

private:
    s16 IsInLaser(BaseAliveGameObject* pWho, BaseAliveGameObject* pOwner);

public:
    Guid mLaserId;

private:
    Guid mTlvId;
    Guid mOwnerId;

    enum class States : s16
    {
        eMoveRight_0 = 0,
        eWaitThenMoveLeft_1 = 1,
        eMoveLeft_2 = 2,
        eWaitThenMoveRight_3 = 3
    };
    States mState = States::eMoveRight_0;

    s32 mPauseTimer = 0;
    s16 mDisableSwitchId = 0;
    s16 mAlarmSwitchId = 0;
    s16 mAlarmDuration = 0;
    bool mHasOwner = false;
    bool mDontComeBack = false;
    FP mTopLeftX = {};
    FP mTopLeftY = {};
    FP mBottomRightX = {};
    FP mBottomRightY = {};
    Poly_G3 mPrims = {};
    FP mSpeed = {};
    bool mObjectInLaser = false;
};
