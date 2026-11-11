#pragma once


#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/Primitives.hpp"

namespace relive
{
    struct Path_MotionDetector;
}

namespace AO {

class MotionDetectorLaser final : public BaseAnimatedWithPhysicsGameObject
{
public:
    MotionDetectorLaser(ResourceManager& resMan, BaseMap& map)
        : BaseAnimatedWithPhysicsGameObject(0, resMan, map)
    {
        mLoadedAnims.push_back(resMan.LoadAnimation(AnimId::MotionDetector_Laser));
    }

    virtual void VScreenChanged() override
    {
        // Empty
    }
};

struct MotionDetectorSaveState final : public SaveStateBase
{
    MotionDetectorSaveState()
        : SaveStateBase(ReliveTypes::eMotionDetector, sizeof(*this))
    { }
    s32 mState;
    s32 mPauseTimer;
    bool mDontComeBack;
    FP mTopLeftX;
    FP mTopLeftY;
    FP mBottomRightX;
    FP mBottomRightY;
    Guid mLaserId;
    FP mSpeed;
    bool mObjectInLaser;
};

class MotionDetector final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const MotionDetectorSaveState& state);

    MotionDetector(relive::Path_MotionDetector* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~MotionDetector();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

    void SetDontComeBack(bool bDontComeBack);

    Guid mTlvId;
    enum class States : s16
    {
        eMoveRight_0 = 0,
        eWaitThenMoveLeft_1 = 1,
        eMoveLeft_2 = 2,
        eWaitThenMoveRight_3 = 3
    };
    States mState = States::eMoveRight_0;
    s32 mPauseTimer = 0;
    u16 mDisableSwitchId = 0;
    s16 mAlarmSwitchId = 0;
    s16 mAlarmDuration = 0;
    bool mDontComeBack = false;
    FP mTopLeftX = {};
    FP mTopLeftY = {};
    FP mBottomRightX = {};
    FP mBottomRightY = {};
    Guid mLaserId;
    Poly_G3 mPrim = {};
    FP mSpeed = {};
    bool mObjectInLaser = false;
};

} // namespace AO
