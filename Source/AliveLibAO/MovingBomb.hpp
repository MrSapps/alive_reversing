#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;
enum eLineTypes : s16;

namespace relive {
struct Path_MovingBomb;
}

namespace AO {

struct MovingBombSaveState final : public SaveStateBase
{
    MovingBombSaveState()
        : SaveStateBase(ReliveTypes::eTimedMine, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    FP mVelX = {};
    FP mVelY = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    bool bFlipX = false;
    bool mRender = true;
    u16 mLastLineYPos = 0;
    eLineTypes mCollisionLineType = eLineTypes::eNone_m1;
    s16 mState = 0;
    Guid mTlvId;
    s32 mTimer = 0;
    FP mSpeed = {};
    u16 mStartMovingSwitchId = 0;
    s16 mMinStopTime = 0;
    s16 mMaxStopTime = 0;
    s32 mChannelMask = 0;
    bool mPersistOffscreen = false;
};

class MovingBomb final : public ::BaseAliveGameObject
{
public:
    MovingBomb(relive::Path_MovingBomb* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~MovingBomb();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual bool VTakeDamage(BaseGameObject* pFrom) override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    void FollowLine();
    s16 HitObject();

    enum class States : s16
    {
        eTriggeredByAlarm_0 = 0,
        eTriggeredBySwitch_1 = 1,
        eMoving_2 = 2,
        eStopMoving_3 = 3,
        eWaitABit_4 = 4,
        eToMoving_5 = 5,
        eBlowingUp_6 = 6,
        eKillMovingBomb_7 = 7
    };
    States mState = States::eTriggeredByAlarm_0;
    Guid mTlvId;
    s32 mTimer = 0;
    FP mSpeed = {};
    u16 mStartMovingSwitchId = 0;
    s16 mMinStopTime = 0;
    s16 mMaxStopTime = 0;
    s32 mChannelMask = 0;
    bool mPersistOffscreen = false;
};

} // namespace AO
